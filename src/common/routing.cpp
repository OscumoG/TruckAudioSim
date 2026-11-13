// SPDX-License-Identifier: MIT
#include "routing.hpp"
#include <cmath>
#include <stdexcept>
namespace tas {
namespace {
constexpr uint32_t FL=0x1, FR=0x2, FC=0x4, LFE=0x8, BL=0x10, BR=0x20, SL=0x200, SR=0x400;
bool isLeft(const std::string& s){return s=="L"||s=="FL"||s=="SL"||s=="BL";}
bool isRight(const std::string& s){return s=="R"||s=="FR"||s=="SR"||s=="BR";}
uint32_t directBit(const std::string& channel){
 if(channel=="FL")return FL;
 if(channel=="FR")return FR;
 if(channel=="FC")return FC;
 if(channel=="LFE")return LFE;
 if(channel=="BL")return BL;
 if(channel=="BR")return BR;
 if(channel=="SL")return SL;
 if(channel=="SR")return SR;
 return 0;
}
}
uint32_t sourceChannelBit(const Speaker& s,uint16_t channels,uint32_t mask){
 if(s.type=="sub"||s.channel=="LFE"){
  if(mask&LFE)return LFE;
  return channels==2 && s.cabAudio!=CabAudio::Legacy && s.type=="sub"?FC:0;
 }
 if(channels==2){
  if((s.cabAudio==CabAudio::Surround51 || s.cabAudio==CabAudio::Surround71) &&
     (s.channel=="SL"||s.channel=="SR"||s.channel=="BL"||s.channel=="BR"))return 0;
  if(isLeft(s.channel))return FL;
  if(isRight(s.channel))return FR;
  return FC; // special: phantom center from L+R
 }
 uint32_t direct=directBit(s.channel);
 if(s.cabAudio==CabAudio::Surround51 && channels==6 && !(mask&(SL|SR))){
  if(direct==SL)direct=BL;
  if(direct==SR)direct=BR;
 }
 if(direct)return (mask&direct)?direct:0;
 if(s.channel=="M")return (mask&FC)?FC:0;
 if(s.channel=="L"||s.channel=="R"){
  const bool left=s.channel=="L";
  // Legacy Trucker AUX presets use L/R for *all* speaker positions.
  // Classify against the cab front-to-back Z axis (front is negative).
  if(s.pos.z>0.20f){
   if(mask&(left?BL:BR))return left?BL:BR;
   if(mask&(left?SL:SR))return left?SL:SR;
  }else if(s.pos.z>-0.30f){
   if(mask&(left?SL:SR))return left?SL:SR;
   if(mask&(left?BL:BR))return left?BL:BR;
  }
  return left?FL:FR;
 }
 return 0;
}
float sourceForSpeaker(const Speaker& s,const float* frame,uint16_t channels,uint32_t mask){
 if(!frame)throw std::runtime_error("Null audio frame");
 const uint32_t bit=sourceChannelBit(s,channels,mask);
 if(bit==0)return 0;
 if(channels==2 && bit==FC){
  const float left=std::isfinite(frame[0])?frame[0]:0;
  const float right=std::isfinite(frame[1])?frame[1]:0;
  return 0.5f*(left+right);
 }
 if(!(mask&bit))return 0;
 // WAVEFORMATEXTENSIBLE channels are ordered by ascending speaker-mask bit.
 unsigned index=0;
 for(uint32_t b=1;b<bit;b<<=1)if(mask&b)++index;
 if(index>=channels)throw std::runtime_error("Invalid source channel index");
 const float sample=frame[index];return std::isfinite(sample)?sample:0.0f;
}
}
