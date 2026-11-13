// SPDX-License-Identifier: MIT
#include "routing.hpp"
#include <bit>
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
}

SpeakerRoute speakerRoute(const Speaker& s,uint16_t channels,uint32_t mask){
 SpeakerRoute route{};
 if(channels>route.size())throw std::runtime_error("Invalid source channel count");
 const auto add=[&](uint32_t bit,float weight){
  if(!(mask&bit))return;
  const auto index=std::popcount(mask&(bit-1));
  if(index>=channels)throw std::runtime_error("Invalid source channel index");
  route[index]+=weight;
 };
 const auto bit=sourceChannelBit(s,channels,mask);
 if(channels==2 && bit==FC){
  const bool auxiliary=s.cabAudio!=CabAudio::Legacy && (s.channel=="FC"||s.type=="sub");
  const float weight=auxiliary?0.25f:0.5f;
  add(FL,weight);
  add(FR,weight);
  return route;
 }
 if(s.cabAudio==CabAudio::Realistic && channels>2 && s.type!="sub"
    && (s.channel=="L"||s.channel=="R")){
  const bool left=s.channel=="L";
  add(left?FL:FR,1);
  add(FC,0.70710678f);
  add(left?SL:SR,0.70710678f);
  add(left?BL:BR,0.70710678f);
  return route;
 }
 if(s.cabAudio==CabAudio::Surround51 && channels==8 && (bit==SL||bit==SR)){
  add(bit,0.70710678f);
  add(bit==SL?BL:BR,0.70710678f);
  return route;
 }
 add(bit,1);
 return route;
}

float sampleForRoute(const SpeakerRoute& route,const float* frame,uint16_t channels){
 float sample=0;
 for(uint16_t i=0;i<channels;++i){
  if(route[i] && std::isfinite(frame[i]))sample+=route[i]*frame[i];
 }
 return sample;
}

float sourceForSpeaker(const Speaker& s,const float* frame,uint16_t channels,uint32_t mask){
 if(!frame)throw std::runtime_error("Null audio frame");
 return sampleForRoute(speakerRoute(s,channels,mask),frame,channels);
}
}
