#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Comfortzone v1.1 payload offsets, validated on EX50 RS485 traffic.
// Per-field evidence distinguishes Influx comparisons, mappings and derivations.
// Power labels do not assert electrical input power or provide a COP calculation.
namespace comfortzone {
struct Definition {
  const char *id,*label,*unit,*group;
  uint32_t block; uint16_t offset; uint8_t scale;
  bool signedValue; int16_t faultOffset; uint8_t faultMask;
  uint8_t width=2, mask=0; bool inverted=false; int16_t add=0;
  const char *kind="number", *source="Loggamera v1.1", *evidence="influx";
};
static const Definition definitions[]={
 {"outdoor","Outdoor","°C","Temperatures",0x02010000,46,10,true,33,1},
 {"indoor","Indoor","°C","Temperatures",0x02010000,52,10,true,33,8},
 {"hot_water","Hot water","°C","Temperatures",0x02010000,94,10,true,36,1},
 {"flow","Flow","°C","Temperatures",0x02010000,48,10,true,33,2},
 {"return","Return","°C","Temperatures",0x02010000,50,10,true,33,4},
 {"hot_gas","Hot gas","°C","Temperatures",0x02010000,54,10,true,33,16},
 {"evaporator_out","Evaporator out","°C","Temperatures",0x02010000,56,10,true,33,32},
 {"evaporator_in","Evaporator in","°C","Temperatures",0x02010000,58,10,true,33,64},
 {"exhaust_air","Exhaust air","°C","Temperatures",0x02010000,60,10,true,33,128},
 {"compressor_frequency","Compressor frequency","Hz","Operation",0x0201c802,10,10,true,-1,0},
 {"compressor_power","Compressor power (reported)","W","Operation",0x0201c802,12,1,true,-1,0},
 {"additional_power","Additional heat","W","Operation",0x0201c802,14,1,false,-1,0},
 {"room_setpoint","Room setpoint","°C","Settings",0x02016401,166,10,true,-1,0},
 {"hot_water_setpoint","Hot-water setpoint","°C","Settings",0x0200c802,171,10,true,-1,0},
 {"heating_curve","Heating curve","","Settings",0x0201c802,108,1,false,-1,0,1},
 {"highgaspresalarm","High gas pressure flag","","Alarm flags",0x02010000,29,1,false,-1,0,1,1,false,0,"flag","Loggamera v1.1","mapped"},
 {"hot_water_overheatalarm","Hot-water overheat flag","","Alarm flags",0x02010000,30,1,false,-1,0,1,1,false,0,"flag","Loggamera v1.1","mapped"},
 {"lackofcoolantalarm","Low refrigerant flag","","Alarm flags",0x02010000,30,1,false,-1,0,1,2,false,0,"flag","Loggamera v1.1","mapped"},
 {"pressureratioalarm","Pressure ratio flag","","Alarm flags",0x02010000,30,1,false,-1,0,1,32,false,0,"flag","Loggamera v1.1","mapped"},
 {"heat_pump_boxalarm","Heat-pump box flag","","Alarm flags",0x02010000,31,1,false,-1,0,1,1,false,0,"flag","Loggamera v1.1","mapped"},
 {"filterchangealarm","Filter change flag","","Alarm flags",0x02010000,31,1,false,-1,0,1,2,false,0,"flag","Loggamera v1.1","mapped"},
 {"heating_overheatalarm","Heating overheat flag","","Alarm flags",0x02010000,31,1,false,-1,0,1,128,false,0,"flag","Loggamera v1.1","mapped"},
 {"te0_sensorfail","TE0 sensor fault flag","","Alarm flags",0x02010000,33,1,false,-1,0,1,1,false,0,"flag","Loggamera v1.1","mapped"},
 {"te1_sensorfail","TE1 sensor fault flag","","Alarm flags",0x02010000,33,1,false,-1,0,1,2,false,0,"flag","Loggamera v1.1","mapped"},
 {"te2_sensorfail","TE2 sensor fault flag","","Alarm flags",0x02010000,33,1,false,-1,0,1,4,false,0,"flag","Loggamera v1.1","mapped"},
 {"te3_sensorfail","TE3 sensor fault flag","","Alarm flags",0x02010000,33,1,false,-1,0,1,8,false,0,"flag","Loggamera v1.1","mapped"},
 {"te4_sensorfail","TE4 sensor fault flag","","Alarm flags",0x02010000,33,1,false,-1,0,1,16,false,0,"flag","Loggamera v1.1","mapped"},
 {"te5_sensorfail","TE5 sensor fault flag","","Alarm flags",0x02010000,33,1,false,-1,0,1,32,false,0,"flag","Loggamera v1.1","mapped"},
 {"te6_sensorfail","TE6 sensor fault flag","","Alarm flags",0x02010000,33,1,false,-1,0,1,64,false,0,"flag","Loggamera v1.1","mapped"},
 {"te7_sensorfail","TE7 sensor fault flag","","Alarm flags",0x02010000,33,1,false,-1,0,1,128,false,0,"flag","Loggamera v1.1","mapped"},
 {"te24_sensorfail","TE24 sensor fault flag","","Alarm flags",0x02010000,36,1,false,-1,0,1,1,false,0,"flag","Loggamera v1.1","mapped"},
 {"fan_mode","Fan mode","","Operation",0x02007a03,91,1,false,-1,0,1,0,false,0,"fan","Loggamera v1.1","influx"},
 {"clock_hours","Pump hours","","Pump clock",0x02010000,4,1,false,-1,0,1,0,false,0,"number","Loggamera v1.1","influx"},
 {"clock_minutes","Pump minutes","","Pump clock",0x02010000,5,1,false,-1,0,1,0,false,0,"number","Loggamera v1.1","influx"},
 {"clock_seconds","Pump seconds","","Pump clock",0x02010000,6,1,false,-1,0,1,0,false,0,"number","Loggamera v1.1","influx"},
 {"clock_day","Pump day","","Pump clock",0x02010000,7,1,false,-1,0,1,0,false,0,"number","Loggamera v1.1","influx"},
 {"clock_month","Pump month","","Pump clock",0x02010000,8,1,false,-1,0,1,0,false,0,"number","Loggamera v1.1","influx"},
 {"clock_year","Pump year","","Pump clock",0x02010000,9,1,false,-1,0,1,0,false,2000,"number","Loggamera v1.1","influx"},
 {"fireplace_code","Fireplace action code","","Protocol codes",0x02011602,156,1,false,-1,0,2,0,false,0,"code","Loggamera v1.1","mapped"},
 {"control_mode_code","Heating control mode code","","Protocol codes",0x02000000,12,1,false,-1,0,1,0,false,0,"code","Loggamera v1.1","mapped"},
 {"hot_water_priority_code","Hot-water priority code","","Settings",0x02000000,14,1,false,-1,0,1,0,false,0,"code","Loggamera v1.1","influx"},
 {"holiday_days","Holiday reduction","days","Settings",0x02001602,140,1,false,-1,0,2,0,false,0,"number","Loggamera v1.1","influx"},
 {"curve_10","Heating curve at +10°C","°C","Heating curve",0x02016401,124,10,true,-1,0,2,0,false,0,"number","Loggamera v1.1","influx"},
 {"curve_5","Heating curve at +5°C","°C","Heating curve",0x02016401,126,10,true,-1,0,2,0,false,0,"number","Loggamera v1.1","influx"},
 {"curve_0","Heating curve at +0°C","°C","Heating curve",0x02016401,128,10,true,-1,0,2,0,false,0,"number","Loggamera v1.1","influx"},
 {"curve_m5","Heating curve at -5°C","°C","Heating curve",0x02016401,130,10,true,-1,0,2,0,false,0,"number","Loggamera v1.1","influx"},
 {"curve_m10","Heating curve at -10°C","°C","Heating curve",0x02016401,132,10,true,-1,0,2,0,false,0,"number","Loggamera v1.1","influx"},
 {"curve_m15","Heating curve at -15°C","°C","Heating curve",0x02016401,134,10,true,-1,0,2,0,false,0,"number","Loggamera v1.1","influx"},
 {"minimum_return","Minimum return temperature","°C","Settings",0x02001602,170,10,true,-1,0,2,0,false,0,"number","Loggamera v1.1","influx"},
 {"additional_heat_threshold","Additional heat outdoor threshold","°C","Settings",0x0200c802,104,10,true,-1,0,2,0,false,0,"number","Loggamera v1.1","influx"},
 {"filter_days","Time to filter change","days","Settings",0x02007a03,122,1,false,-1,0,2,0,false,0,"number","Loggamera v1.1","influx"},
 {"heating_valve","Valve directed to heating","","Operation",0x02010000,26,1,false,-1,0,1,4,true,0,"boolean","Loggamera v1.1","influx"},
 {"hot_water_valve","Valve directed to hot water","","Operation",0x02010000,26,1,false,-1,0,1,4,false,0,"boolean","Loggamera v1.1","influx"},
 {"extra_hot_water_code","Extra hot-water code (0=off, 32=on)","","Settings",0x02010000,24,1,false,-1,0,1,0,false,0,"code","Loggamera v1.1","mapped"},
 {"highgaspres_alarmreset","High gas pressure acknowledgement bit","","Acknowledgement bits",0x02010000,37,1,false,-1,0,1,1,false,0,"flag","Loggamera v1.1","mapped"},
 {"hot_water_overheatalarmreset","Hot-water overheat acknowledgement bit","","Acknowledgement bits",0x02010000,38,1,false,-1,0,1,1,false,0,"flag","Loggamera v1.1","mapped"},
 {"lackofcoolantalarmreset","Low refrigerant acknowledgement bit","","Acknowledgement bits",0x02010000,38,1,false,-1,0,1,2,false,0,"flag","Loggamera v1.1","mapped"},
 {"pressureratioalarmreset","Pressure ratio acknowledgement bit","","Acknowledgement bits",0x02010000,38,1,false,-1,0,1,32,false,0,"flag","Loggamera v1.1","mapped"},
 {"heat_pump_boxalarmreset","Heat-pump box acknowledgement bit","","Acknowledgement bits",0x02010000,39,1,false,-1,0,1,1,false,0,"flag","Loggamera v1.1","mapped"},
 {"heating_overheatalarmreset","Heating overheat acknowledgement bit","","Acknowledgement bits",0x02010000,39,1,false,-1,0,1,128,false,0,"flag","Loggamera v1.1","mapped"},
 {"te0_sensorfailreset","TE0 sensor fault acknowledgement bit","","Acknowledgement bits",0x02010000,41,1,false,-1,0,1,1,false,0,"flag","Loggamera v1.1","mapped"},
 {"te1_sensorfailreset","TE1 sensor fault acknowledgement bit","","Acknowledgement bits",0x02010000,41,1,false,-1,0,1,2,false,0,"flag","Loggamera v1.1","mapped"},
 {"te2_sensorfailreset","TE2 sensor fault acknowledgement bit","","Acknowledgement bits",0x02010000,41,1,false,-1,0,1,4,false,0,"flag","Loggamera v1.1","mapped"},
 {"te3_sensorfailreset","TE3 sensor fault acknowledgement bit","","Acknowledgement bits",0x02010000,41,1,false,-1,0,1,8,false,0,"flag","Loggamera v1.1","mapped"},
 {"te4_sensorfailreset","TE4 sensor fault acknowledgement bit","","Acknowledgement bits",0x02010000,41,1,false,-1,0,1,16,false,0,"flag","Loggamera v1.1","mapped"},
 {"te5_sensorfailreset","TE5 sensor fault acknowledgement bit","","Acknowledgement bits",0x02010000,41,1,false,-1,0,1,32,false,0,"flag","Loggamera v1.1","mapped"},
 {"te6_sensorfailreset","TE6 sensor fault acknowledgement bit","","Acknowledgement bits",0x02010000,41,1,false,-1,0,1,64,false,0,"flag","Loggamera v1.1","mapped"},
 {"te7_sensorfailreset","TE7 sensor fault acknowledgement bit","","Acknowledgement bits",0x02010000,41,1,false,-1,0,1,128,false,0,"flag","Loggamera v1.1","mapped"},
 {"te24_sensorfailreset","TE24 sensor fault acknowledgement bit","","Acknowledgement bits",0x02010000,44,1,false,-1,0,1,1,false,0,"flag","Loggamera v1.1","mapped"},
 {"filterchangealarmmute","Filter change acknowledgement bit","","Acknowledgement bits",0x02010000,39,1,false,-1,0,1,2,false,0,"flag","Loggamera v1.1","mapped"},
 {"compressor_running","Compressor running (from frequency)","","Operation",0,0,1,false,-1,0,0,0,false,0,"boolean","Derived from frequency or power and valve","derived"},
 {"compressor_power_heating","Compressor power to heating (derived)","W","Operation",0,0,1,false,-1,0,0,0,false,0,"number","Derived from frequency or power and valve","derived"},
 {"compressor_power_hot_water","Compressor power to hot water (derived)","W","Operation",0,0,1,false,-1,0,0,0,false,0,"number","Derived from frequency or power and valve","derived"},
 {"additional_power_heating","Additional heat to heating (derived)","W","Operation",0,0,1,false,-1,0,0,0,false,0,"number","Derived from frequency or power and valve","derived"},
 {"additional_power_hot_water","Additional heat to hot water (derived)","W","Operation",0,0,1,false,-1,0,0,0,false,0,"number","Derived from frequency or power and valve","derived"},
};
constexpr size_t count=sizeof(definitions)/sizeof(definitions[0]);
struct Value { int32_t raw=0; uint32_t updatedMs=0; bool received=false, fault=false; };
inline uint8_t crc(const uint8_t *p,size_t n) {
 uint8_t c=0;while(n--){c^=*p++;for(int j=0;j<8;++j)c=(c>>1)^((c&1)?0x8c:0);}return c;
}
inline size_t indexOf(const char *id) {
 for(size_t i=0;i<count;++i)if(!strcmp(definitions[i].id,id))return i;
 return count;
}
inline void derive(Value *values,uint32_t now) {
 auto &running=values[indexOf("compressor_running")];
 const auto &frequency=values[indexOf("compressor_frequency")];
 running=frequency;running.raw=frequency.raw>0;
 const auto &valve=values[indexOf("hot_water_valve")];
 const char *outputs[]={"compressor_power_heating","compressor_power_hot_water","additional_power_heating","additional_power_hot_water"};
 for(size_t i=0;i<4;++i) {
  const auto &power=values[indexOf(i<2?"compressor_power":"additional_power")];
  auto &v=values[indexOf(outputs[i])];v.received=power.received&&valve.received;
  v.fault=power.fault||valve.fault;
  // Report the age of the older dependency, including millis() wraparound.
  v.updatedMs=uint32_t(now-power.updatedMs)>uint32_t(now-valve.updatedMs)?power.updatedMs:valve.updatedMs;
  v.raw=bool(valve.raw)==bool(i%2)?power.raw:0;
 }
}
inline bool decode(const uint8_t *frame,size_t length,uint32_t now,Value *values) {
 static const uint8_t prefix[]={1,2,3,4,11};
 if(length<22 || length>255 || frame[10]!=length || frame[11]!='r' ||
    memcmp(frame+12,prefix,5)!=0 || crc(frame,length-1)!=frame[length-1])return false;
 uint32_t block=uint32_t(frame[17])<<24|uint32_t(frame[18])<<16|uint32_t(frame[19])<<8|frame[20];
 const uint8_t *p=frame+21;size_t size=length-22;bool changed=false;
 for(size_t i=0;i<count;++i) {
  const auto &d=definitions[i];
  const size_t width=d.width;
  if(block!=d.block || d.offset+width>size)continue;
  uint16_t raw=p[d.offset];if(width==2)raw|=uint16_t(p[d.offset+1])<<8;
  auto &v=values[i];v.raw=d.signedValue?int32_t(int16_t(raw)):int32_t(raw);
  if(d.mask)v.raw=((raw&d.mask)!=0)^d.inverted;
  v.raw+=d.add;
  v.received=true;v.updatedMs=now;
  v.fault=(d.signedValue && (raw==0x8000 || raw==0x7fff)) ||
    (d.faultOffset>=0 && size_t(d.faultOffset)<size && (p[d.faultOffset]&d.faultMask));
  changed=true;
 }
 if(changed)derive(values,now);
 return changed;
}
}
