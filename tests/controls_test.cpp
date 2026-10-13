#include "controls.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <string>
using namespace comfortzone_control;
std::vector<uint8_t> fromHex(const std::string &s){std::vector<uint8_t> f;for(size_t i=0;i<s.size();i+=2)f.push_back(std::stoi(s.substr(i,2),nullptr,16));return f;}
void seal(std::vector<uint8_t> &f){f[10]=f.size();f[4]=comfortzone::crc(f.data(),4);uint8_t inverted[4];for(int i=0;i<4;++i)inverted[i]=~f[i];f[5]=comfortzone::crc(inverted,4);f.back()=comfortzone::crc(f.data(),f.size()-1);}
std::vector<uint8_t> reply(const Frame &tx,char cmd,uint32_t block,size_t payload){
 std::vector<uint8_t> f(22+payload);memcpy(f.data(),tx.bytes+6,4);memcpy(f.data()+6,tx.bytes,4);f[11]=cmd;memcpy(f.data()+12,tx.bytes+12,5);
 for(int i=0;i<4;++i){f[17+i]=block>>(24-8*i);}
 seal(f);return f;
}
int main(){
 int raw;assert(parse(Kind::Fan,"2",raw)&&raw==2);assert(parse(Kind::Room,"21.3",raw)&&raw==213);assert(parse(Kind::Water,"60",raw)&&raw==600);
 for(auto s:{"","NaN","inf","1e2"," 21","21x","21.01","-1","999999999999999999999999999"})assert(!parse(Kind::Room,s,raw));
 assert(!parse(Kind::Fan,"1.5",raw));assert(!parse(Kind::Fan,"4",raw));assert(!parse(Kind::Water,"60.1",raw));assert(!parse(Kind::Extra,"2",raw));
 const auto route=fromHex("656fde02d35e41444452");
 // Byte-for-byte vendor templates, including independent stored CRCs.
 for(auto pair:{std::make_pair(1,"656fde02d35e414444521757010203040b0200d5030105"),std::make_pair(2,"656fde02d35e414444521757010203040b0200d50302e7"),std::make_pair(3,"656fde02d35e414444521757010203040b0200d50303b9")}){
  auto f=make(Kind::Fan,pair.first,route.data());auto expected=fromHex(pair.second);assert(f.length==expected.size()&&!memcmp(f.bytes,expected.data(),f.length));
 }
 for(auto pair:{std::make_pair(1,"656fde02d35e414444521857010203040b028119000100ed"),std::make_pair(0,"656fde02d35e414444521857010203040b02411900feff7b")}){
  auto f=make(Kind::Extra,pair.first,route.data());auto expected=fromHex(pair.second);assert(f.length==expected.size()&&!memcmp(f.bytes,expected.data(),f.length));
 }
 auto room=make(Kind::Room,213,route.data());assert(room.bytes[17]==2&&room.bytes[18]==0&&room.bytes[19]==0xa6&&room.bytes[20]==2&&room.bytes[21]==213&&room.bytes[22]==0&&valid(room.bytes,room.length));
 auto water=make(Kind::Water,550,route.data());assert(water.bytes[19]==0x73&&water.bytes[20]==3&&water.bytes[21]==0x26&&water.bytes[22]==2&&valid(water.bytes,water.length));
 std::string a,b;std::getline(std::cin,a);std::getline(std::cin,b);auto q=fromHex(a),r=fromHex(b);assert(q[11]=='R'&&r[11]=='r');
 comfortzone::Value values[comfortzone::count];
 auto learn=[&](Engine &e,uint32_t now){e.byte(now);e.observe(q.data(),q.size(),now,values);e.byte(now+1);e.observe(r.data(),r.size(),now+1,values);};
 Engine e;assert(!e.queue(Kind::Fan,2,1));learn(e,100);assert(e.ready(102));assert(!e.queue(Kind::Fan,4,102));assert(e.queue(Kind::Fan,2,102));assert(!e.queue(Kind::Fan,1,103));
 Frame tx;assert(!e.tick(150,true,tx));assert(!e.tick(205,false,tx));assert(e.tick(205,true,tx));assert(!e.tick(206,true,tx));
 auto ack=reply(tx,'w',0x0200d503,1);e.observe(ack.data(),ack.size(),220,values);assert(e.result.acknowledged&&e.result.state==State::Sent);
 auto data=reply(tx,'r',0x02007a03,178);data[21+91]=2;seal(data);
 // Cached data must never confirm a write.
 values[comfortzone::indexOf("fan_mode")]={2,50,true,false};e.observe(data.data(),data.size(),230,values);assert(e.result.state==State::Sent);
 comfortzone::decode(data.data(),data.size(),240,values);e.observe(data.data(),data.size(),240,values);assert(e.result.state==State::Confirmed);
 Engine noAck;learn(noAck,100);assert(noAck.queue(Kind::Fan,2,102));assert(noAck.tick(205,true,tx));comfortzone::decode(data.data(),data.size(),240,values);noAck.observe(data.data(),data.size(),240,values);assert(noAck.result.state==State::Sent);
 auto badAck=ack;badAck.back()^=1;noAck.observe(badAck.data(),badAck.size(),250,values);assert(!noAck.result.acknowledged);
 badAck=ack;badAck[6]^=1;seal(badAck);noAck.observe(badAck.data(),badAck.size(),250,values);assert(!noAck.result.acknowledged);
 assert(!noAck.tick(15205,true,tx));assert(noAck.result.state==State::Unconfirmed);assert(!noAck.tick(16000,true,tx));
 Engine busy;learn(busy,100);assert(busy.queue(Kind::Room,210,102));busy.byte(150);assert(!busy.tick(300,true,tx));assert(!busy.tick(10102,true,tx));assert(busy.result.state==State::BusBusy);
 learn(busy,11000);assert(busy.queue(Kind::Room,210,11002));busy.interrupt(11003);assert(!busy.tick(11200,true,tx));assert(busy.result.state==State::Interrupted);
 Engine wrongRoute;wrongRoute.observe(r.data(),r.size(),100,values);assert(!wrongRoute.ready(101));
 Engine wrap;learn(wrap,0xffffff00u);assert(wrap.queue(Kind::Fan,2,0xffffff03u));assert(wrap.tick(0xffffff80u,true,tx));assert(!wrap.tick(16000,true,tx));assert(wrap.result.state==State::Unconfirmed);
 std::cout<<"PASS: vendor write templates, decimal validation, paired route learning, idle gap, single attempt, ACK/address/CRC and fresh readback, timeouts, OTA interruption, millis wrap.\n";
}
