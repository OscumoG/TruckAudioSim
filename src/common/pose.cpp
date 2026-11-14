// SPDX-License-Identifier: MIT
#include "pose.hpp"
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <numbers>
#include <stdexcept>
namespace tas {
Pose parsePoseText(const std::string& raw){
 if(raw.size()>64*1024)throw std::runtime_error("Pose JSON is too large");
 auto root=JsonParser(raw).parse();if(root.kind!=Json::Kind::Object)throw std::runtime_error("Pose must be object");
 Pose p;
 if(auto x=root.get("yawDeg"))p.yawDegrees=float(x->num());
 if(auto x=root.get("pitchDeg"))p.pitchDegrees=float(x->num());
 if(auto x=root.get("rollDeg"))p.rollDegrees=float(x->num());
 if(auto x=root.get("volume"))p.volume=float(x->num());
 if(auto x=root.get("exteriorGain"))p.exteriorGain=float(x->num());
 if(auto x=root.get("exterior"))p.exterior=x->kind==Json::Kind::Boolean && x->boolean;
 if(auto x=root.get("testSequence"))p.testSequence=static_cast<unsigned>(std::clamp(x->num(),0.0,2147483647.0));
 if(auto x=root.get("testSpeaker"))p.testSpeaker=x->str();
 if(auto x=root.get("head")){
  if(x->kind!=Json::Kind::Array||x->array.size()!=3)throw std::runtime_error("Pose head must contain three numbers");
  p.head={float(x->array[0].num()),float(x->array[1].num()),float(x->array[2].num())};
 }
 if(auto x=root.get("electronicsOn")){
  if(x->kind!=Json::Kind::Boolean)throw std::runtime_error("Pose electronicsOn must be boolean");
  p.electronicsOn=x->boolean;
 }
 const auto badAngle=[](float angle){return !std::isfinite(angle)||std::fabs(angle)>36000;};
 if(badAngle(p.yawDegrees)||badAngle(p.pitchDegrees)||badAngle(p.rollDegrees) ||
    !std::isfinite(p.volume)||p.volume<0||p.volume>kMaxVolume ||
    !std::isfinite(p.exteriorGain)||p.exteriorGain<0||p.exteriorGain>1 ||
    !std::isfinite(p.head.x)||!std::isfinite(p.head.y)||!std::isfinite(p.head.z)||
    std::fabs(p.head.x)>40.01f||std::fabs(p.head.y)>40.01f||std::fabs(p.head.z)>40.01f)
  throw std::runtime_error("Out of bounds pose");
 return p;
}
Pose loadPoseFile(const std::string& path){
 std::ifstream f(std::filesystem::path(std::u8string(path.begin(),path.end())),std::ios::binary);if(!f)throw std::runtime_error("Cannot open pose: "+path);
 std::string raw((std::istreambuf_iterator<char>(f)),{});return parsePoseText(raw);
}
Pose smoothPose(const Pose& prev,const Pose& target,float alpha){
 alpha=std::clamp(alpha,0.0f,1.0f);
 Pose out;
 const auto angle=[alpha](float from,float to){return from+alpha*std::remainder(to-from,360.0f);};
 out.yawDegrees=angle(prev.yawDegrees,target.yawDegrees);
 out.pitchDegrees=angle(prev.pitchDegrees,target.pitchDegrees);
 out.rollDegrees=angle(prev.rollDegrees,target.rollDegrees);
 out.volume=prev.volume+alpha*(target.volume-prev.volume);
 if(std::fabs(out.volume-target.volume)<0.0001f)out.volume=target.volume;
 out.exteriorGain=prev.exteriorGain+alpha*(target.exteriorGain-prev.exteriorGain);
 if(std::fabs(out.exteriorGain-target.exteriorGain)<0.0001f)out.exteriorGain=target.exteriorGain;
 out.head={prev.head.x+alpha*(target.head.x-prev.head.x),
           prev.head.y+alpha*(target.head.y-prev.head.y),
           prev.head.z+alpha*(target.head.z-prev.head.z)};
 out.electronicsOn=target.electronicsOn;
 out.exterior=target.exterior;
 out.testSequence=target.testSequence;
 out.testSpeaker=target.testSpeaker;
 return out;
}
Vec3 relativePosition(const Vec3& speaker,const Pose& pose){
 const auto yaw=relativePosition(speaker,pose.head,pose.yawDegrees);
 if(pose.pitchDegrees==0 && pose.rollDegrees==0)return yaw;
 const float pitch=pose.pitchDegrees*std::numbers::pi_v<float>/180;
 const float roll=pose.rollDegrees*std::numbers::pi_v<float>/180;
 const float y=std::cos(pitch)*yaw.y+std::sin(pitch)*yaw.z;
 const float z=-std::sin(pitch)*yaw.y+std::cos(pitch)*yaw.z;
 return {std::cos(roll)*yaw.x+std::sin(roll)*y,-std::sin(roll)*yaw.x+std::cos(roll)*y,z};
}
}
