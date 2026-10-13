#pragma once
#include "measurements.h"
#include <math.h>
#include <stdlib.h>

// Comfortzone v1.1 write registers. Do not substitute qix67 v1.6 addresses.
namespace comfortzone_control {
enum class Kind : uint8_t { Fan, Room, Water, Extra };
enum class State : uint8_t { Idle, Queued, Sent, Confirmed, Unconfirmed, BusBusy, Interrupted };
struct Spec {const char *name,*reading;int minimum,maximum,scale;};
static const Spec specs[]={
 {"fan","fan_mode",1,3,1}, {"room_temperature","room_setpoint",100,500,10},
 {"hot_water_temperature","hot_water_setpoint",100,600,10},
 {"extra_hot_water","extra_hot_water_code",0,1,1}
};
inline const char *stateName(State s){
 switch(s){case State::Idle:return "idle";case State::Queued:return "queued";case State::Sent:return "verifying";
 case State::Confirmed:return "confirmed";case State::Unconfirmed:return "unconfirmed";case State::BusBusy:return "bus_busy";
 case State::Interrupted:return "interrupted";}return "unknown";
}
inline bool parse(Kind kind,const char *text,int &raw){
 // Strict decimal syntax: no whitespace, exponents, non-finite values, or extra precision.
 if(!text||!*text)return false;
 const char *p=text;unsigned digits=0;while(*p>='0'&&*p<='9'){++p;++digits;}if(!digits)return false;
 if(*p=='.'){++p;if(*p<'0'||*p>'9')return false;++p;}
 if(*p)return false;
 const auto &s=specs[int(kind)];double v=strtod(text,nullptr)*s.scale;
 if(!isfinite(v)||v<s.minimum||v>s.maximum||fabs(v-round(v))>0.00001)return false;
 raw=int(round(v));return true;
}
struct Frame {uint8_t bytes[24]{};size_t length=0;};
inline Frame make(Kind kind,int raw,const uint8_t *route){
 Frame f;uint32_t reg=0;uint16_t wire=raw;uint8_t width=2;
 switch(kind){
 case Kind::Fan:reg=0x0200d503;width=1;break;
 case Kind::Room:reg=0x0200a602;break;
 case Kind::Water:reg=0x02007303;break;
 case Kind::Extra:reg=raw?0x02811900:0x02411900;wire=raw?1:0xfffe;break;
 }
 memcpy(f.bytes,route,10);f.length=22+width;f.bytes[10]=f.length;f.bytes[11]='W';
 const uint8_t prefix[]={1,2,3,4,11};memcpy(f.bytes+12,prefix,5);
 for(int i=0;i<4;++i)f.bytes[17+i]=uint8_t(reg>>(24-i*8));
 f.bytes[21]=wire;if(width==2)f.bytes[22]=wire>>8;
 f.bytes[f.length-1]=comfortzone::crc(f.bytes,f.length-1);return f;
}
inline bool valid(const uint8_t *f,size_t n){
 if(n<22||n>255||f[10]!=n||comfortzone::crc(f,n-1)!=f[n-1])return false;
 uint8_t complement[4];for(int i=0;i<4;++i)complement[i]=~f[i];
 const uint8_t prefix[]={1,2,3,4,11,2};
 return f[4]==comfortzone::crc(f,4)&&f[5]==comfortzone::crc(complement,4)&&!memcmp(f+12,prefix,6);
}
struct Result {
 uint32_t id=0,queuedMs=0,sentMs=0,completedMs=0;
 Kind kind=Kind::Fan;State state=State::Idle;int requested=0,observed=0,replyCode=-1;
 bool acknowledged=false,readback=false;Frame tx;
};
class Engine {
 uint8_t query[10]{},route[10]{};bool querySeen=false,routeSeen=false,gap=false;
 uint32_t queryMs=0,routeMs=0,lastByte=0,nextId=0;
public:
 Result result;
 bool active() const{return result.state==State::Queued||result.state==State::Sent;}
 bool ready(uint32_t now) const{return routeSeen&&uint32_t(now-routeMs)<15000;}
 void byte(uint32_t now){lastByte=now;gap=false;}
 bool queue(Kind kind,int raw,uint32_t now){
  const auto &s=specs[int(kind)];if(active()||!ready(now)||raw<s.minimum||raw>s.maximum)return false;
  result=Result{};result.id=++nextId;result.kind=kind;result.requested=raw;result.queuedMs=now;result.state=State::Queued;return true;
 }
 void interrupt(uint32_t now){if(active()){result.state=State::Interrupted;result.completedMs=now;}}
 void observe(const uint8_t *f,size_t n,uint32_t now,const comfortzone::Value *values){
  if(!valid(f,n))return;
  const bool endBlock=f[18]==5&&f[19]==0&&f[20]==0;
  if(f[11]=='R'&&endBlock){memcpy(query,f,10);querySeen=true;queryMs=now;}
  if(f[11]=='r'&&endBlock&&querySeen&&uint32_t(now-queryMs)<1000&&
     !memcmp(f,query+6,4)&&!memcmp(f+6,query,4)){
   memcpy(route,query,10);routeSeen=true;routeMs=now;querySeen=false;gap=true;
  }
  if(result.state!=State::Sent)return;
  const auto &tx=result.tx;
  if(n==23&&f[11]=='w'&&!memcmp(f,tx.bytes+6,4)&&!memcmp(f+6,tx.bytes,4)&&!memcmp(f+12,tx.bytes+12,9)){
   result.acknowledged=true;result.replyCode=f[21];
  }
  const size_t index=comfortzone::indexOf(specs[int(result.kind)].reading);const auto &d=comfortzone::definitions[index];
  uint32_t block=uint32_t(f[17])<<24|uint32_t(f[18])<<16|uint32_t(f[19])<<8|f[20];
  const auto &v=values[index];
  if(f[11]=='r'&&block==d.block&&n>=size_t(22+d.offset+d.width)&&
     !memcmp(f,tx.bytes+6,4)&&!memcmp(f+6,tx.bytes,4)&&v.received&&!v.fault&&v.updatedMs==now){
   result.observed=v.raw;
   result.readback=v.raw==(result.kind==Kind::Extra?result.requested*32:result.requested);
  }
  if(result.acknowledged&&result.readback){result.state=State::Confirmed;result.completedMs=now;}
 }
 // Called only by the UART owner. One attempt; never retry an ambiguous write.
 bool tick(uint32_t now,bool uartEmpty,Frame &out){
  if(result.state==State::Queued){
   if(uint32_t(now-result.queuedMs)>=10000){result.state=State::BusBusy;result.completedMs=now;return false;}
   uint32_t quiet=now-lastByte;
   if(gap&&ready(now)&&uartEmpty&&quiet>=100&&quiet<1000){
    gap=false;result.tx=make(result.kind,result.requested,route);out=result.tx;
    result.sentMs=now;result.state=State::Sent;return true;
   }
  }else if(result.state==State::Sent&&uint32_t(now-result.sentMs)>=15000){result.state=State::Unconfirmed;result.completedMs=now;}
  return false;
 }
};
}
