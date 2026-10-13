#include "measurements.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <string>
int main(){
 comfortzone::Value v[comfortzone::count];for(auto &x:v)assert(!x.received);
 std::string line;std::vector<uint8_t> temperature;size_t accepted=0;
 while(std::getline(std::cin,line)){
  std::vector<uint8_t> f;
  for(size_t i=0;i+1<line.size();i+=2)f.push_back(std::stoi(line.substr(i,2),nullptr,16));
  if(comfortzone::decode(f.data(),f.size(),1000,v))++accepted;
  if(f.size()>120&&f[11]=='r'&&f[17]==2&&f[18]==1&&f[19]==0&&f[20]==0)temperature=f;
 }
 assert(accepted>0&&!temperature.empty());
 const int expected[]={150,200,500,300,280,350,100,50,80,0,0,0,200,500,3};
 for(size_t i=0;i<15;++i){assert(v[i].received&&!v[i].fault);assert(v[i].raw==expected[i]);}
 for(size_t i=0;i<comfortzone::count;++i)assert(v[i].received&&!v[i].fault);
 auto index=[](const char *id){for(size_t i=0;i<comfortzone::count;++i)if(!strcmp(comfortzone::definitions[i].id,id))return i;assert(false);return size_t(0);};
 assert(v[index("fan_mode")].raw==2);assert(v[index("heating_valve")].raw==1);assert(v[index("hot_water_valve")].raw==0);
 assert(v[index("holiday_days")].raw==0);assert(v[index("minimum_return")].raw==160);
 assert(v[index("additional_heat_threshold")].raw==40);assert(v[index("filter_days")].raw==40);
 assert(v[index("curve_10")].raw==230);assert(v[index("curve_m15")].raw==370);
 assert(v[index("hot_water_priority_code")].raw==17);assert(v[index("clock_year")].raw==2000);
 auto corrupt=temperature;corrupt.back()^=1;assert(!comfortzone::decode(corrupt.data(),corrupt.size(),2000,v));assert(v[0].updatedMs==1000);
 assert(!comfortzone::decode(temperature.data(),temperature.size()-1,2000,v));
 auto query=temperature;query[11]='R';query.back()=comfortzone::crc(query.data(),query.size()-1);assert(!comfortzone::decode(query.data(),query.size(),2000,v));
 // Signed readings, sensor-fault bit, and reserved error value.
 auto f=temperature;f[21+46]=0xc9;f[21+47]=0xff;f.back()=comfortzone::crc(f.data(),f.size()-1);
 assert(comfortzone::decode(f.data(),f.size(),2000,v));assert(v[0].raw==-55&&!v[0].fault);
 f[21+33]|=1;f.back()=comfortzone::crc(f.data(),f.size()-1);assert(comfortzone::decode(f.data(),f.size(),3000,v));assert(v[0].fault);
 f[21+33]&=~1;f[21+46]=0;f[21+47]=0x80;f.back()=comfortzone::crc(f.data(),f.size()-1);assert(comfortzone::decode(f.data(),f.size(),4000,v));assert(v[0].fault);
 // Derived split power uses valve direction and the older dependency timestamp.
 auto &power=v[index("compressor_power")];auto &valve=v[index("hot_water_valve")];
 power={2345,100,true,false};valve={1,200,true,false};
 v[index("compressor_frequency")]={450,150,true,false};comfortzone::derive(v,300);
 assert(v[index("compressor_power_hot_water")].raw==2345);
 assert(v[index("compressor_power_heating")].raw==0);
 assert(v[index("compressor_power_hot_water")].updatedMs==100);
 assert(v[index("compressor_running")].raw==1);
 valve.received=false;comfortzone::derive(v,300);assert(!v[index("compressor_power_hot_water")].received);
 valve={0,50,true,false};comfortzone::derive(v,300);
 assert(v[index("compressor_power_heating")].raw==2345);
 assert(v[index("compressor_power_heating")].updatedMs==50);
 // Every alarm/acknowledgement mask must reject unrelated bits and decode its own bit.
 for(size_t i=0;i<comfortzone::count;++i){const auto &d=comfortzone::definitions[i];if(!d.mask)continue;
  for(int byte=0;byte<256;++byte){auto bitframe=temperature;bitframe[21+d.offset]=byte;
   bitframe.back()=comfortzone::crc(bitframe.data(),bitframe.size()-1);
   assert(comfortzone::decode(bitframe.data(),bitframe.size(),5000,v));
   assert(v[i].raw==int(bool(byte&d.mask)^d.inverted));
  }
 }
 // A valid, shorter reply never updates a field whose bytes are absent.
 auto shortFrame=temperature;shortFrame.resize(21+47+1);shortFrame[10]=shortFrame.size();shortFrame.back()=comfortzone::crc(shortFrame.data(),shortFrame.size()-1);
 auto before=v[0].updatedMs;comfortzone::decode(shortFrame.data(),shortFrame.size(),6000,v);assert(v[0].updatedMs==before);
 assert(!comfortzone::decode(nullptr,0,7000,v));
 std::cout<<"PASS: "<<accepted<<" synthetic replies; 75 received fields; synthetic values and all bit patterns; CRC/truncation/query rejection; signed values and sensor faults.\n";
}
