// SPDX-License-Identifier: MIT
#include "pose.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace tas;
int main(){
 try{
  auto p=parsePoseText(R"({"yawDeg":65,"head":[-0.4,0.1,0.2],"electronicsOn":false})");
  if(p.electronicsOn||p.yawDegrees!=65||p.head.x!=-0.4f)throw std::runtime_error("pose parse");
  auto desktop=parsePoseText(R"({"head":[0,0,0],"yawDeg":-90,"pitchDeg":0,"rollDeg":0,"volume":0.25})");
  auto ahead=relativePosition({0,0,-1},desktop);
  if(std::fabs(ahead.x-1)>0.0001f||desktop.volume!=0.25f)throw std::runtime_error("left head turn");
  desktop.yawDegrees=0;desktop.pitchDegrees=90;
  ahead=relativePosition({0,0,-1},desktop);
  if(std::fabs(ahead.y+1)>0.0001f)throw std::runtime_error("upward head turn");
  desktop.pitchDegrees=0;desktop.rollDegrees=90;
  const auto up=relativePosition({0,1,0},desktop);
  if(std::fabs(up.x-1)>0.0001f)throw std::runtime_error("left head tilt");
  Pose a,b;a.yawDegrees=179;b.yawDegrees=-179;
  auto s=smoothPose(a,b,0.5f);
  if(std::fabs(s.yawDegrees-180)>0.0001f)throw std::runtime_error("incorrect shortest yaw arc");
  bool rejected=false;
  try{(void)parsePoseText(R"({"yawDeg":200000})");}catch(const std::exception&){rejected=true;}
  if(!rejected)throw std::runtime_error("out of range accepted");
  const auto boosted=parsePoseText(R"({"volume":5,"exteriorGain":0.25})");
  const auto transition=smoothPose(Pose{},boosted,0.5f);
  if(boosted.volume!=5 || transition.volume!=3 || transition.exteriorGain!=0.625f)
   throw std::runtime_error("boosted volume was lost");
  rejected=false;
  try{(void)parsePoseText(R"({"volume":5.01})");}catch(const std::exception&){rejected=true;}
  if(!rejected)throw std::runtime_error("volume above 500% accepted");
  std::cout<<"PASS: pose parse, bounds, electronics state, shortest yaw smoothing\n";
  return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
