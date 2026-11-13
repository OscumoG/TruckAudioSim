// SPDX-License-Identifier: MIT
#include "profile.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
int main(int argc,char**argv){
 if(argc!=2){std::cerr<<"usage: tas_validate preset.json\n";return 2;}
 try{
  const auto p=tas::loadProfile(argv[1]);
  if(p.speakers.size()!=8)throw std::runtime_error("Example should have eight speakers");
  auto v=tas::relativePosition({0,0,-1},{0,0,0},90);
  if(std::fabs(v.x+1)>0.0001f||std::fabs(v.z)>0.0001f)throw std::runtime_error("Yaw rotation incorrect");
  tas::Speaker s;s.channel="LFE";
  if(tas::channelSample(s,0.9f,0.9f)!=0)throw std::runtime_error("Stereo must not create an LFE signal");
  std::cout<<"PASS: "<<p.name<<", speakers="<<p.speakers.size()<<", coordinates and bass headroom\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
