// Decode captured hex frames with the same header used by the device.
#include "measurements.h"
#include <iostream>
#include <string>
#include <vector>
int main(){
 comfortzone::Value values[comfortzone::count];std::string line;uint32_t n=0;
 while(std::getline(std::cin,line)){
  std::vector<uint8_t> frame;
  for(size_t i=0;i+1<line.size();i+=2)frame.push_back(std::stoi(line.substr(i,2),nullptr,16));
  comfortzone::decode(frame.data(),frame.size(),++n,values);
 }
 for(size_t i=0;i<comfortzone::count;++i){const auto &d=comfortzone::definitions[i];const auto &v=values[i];
  std::cout<<d.id<<'\t'<<double(v.raw)/d.scale<<'\t'<<v.received<<'\t'<<v.fault<<'\n';
 }
}
