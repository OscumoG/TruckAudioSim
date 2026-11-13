// SPDX-License-Identifier: MIT
#include "profile.hpp"
#include "routing.hpp"
#include "wav.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace tas;
namespace{
void eq(float a,float b,const char* msg){if(std::fabs(a-b)>0.00001f)throw std::runtime_error(msg);}
Speaker at(const char* ch,float z=0,const char* type="full"){
 Speaker s;s.channel=ch;s.type=type;s.pos.z=z;return s;
}
}
int main(){
 try{
  const float stereo[]={0.5f,-0.25f};
  eq(sourceForSpeaker(at("L"),stereo,2,kStereo),0.5f,"stereo left");
  eq(sourceForSpeaker(at("M"),stereo,2,kStereo),0.125f,"stereo phantom center");
  eq(sourceForSpeaker(at("LFE"),stereo,2,kStereo),0,"no synthesized LFE");
  eq(sourceForSpeaker(at("M",0,"sub"),stereo,2,kStereo),0,"no bass boost");
  // Ascending order: FL FR FC LFE BL BR SL SR.
  const float surround[]={0.10f,0.20f,0.30f,0.40f,0.50f,0.60f,0.70f,0.80f};
  eq(sourceForSpeaker(at("L",-0.8f),surround,8,kSevenPointOne),0.10f,"front L");
  eq(sourceForSpeaker(at("R",-0.8f),surround,8,kSevenPointOne),0.20f,"front R");
  eq(sourceForSpeaker(at("M",-0.8f),surround,8,kSevenPointOne),0.30f,"center");
  eq(sourceForSpeaker(at("M",0,"sub"),surround,8,kSevenPointOne),0.40f,"discrete LFE");
  eq(sourceForSpeaker(at("L",0.5f),surround,8,kSevenPointOne),0.50f,"rear L");
  eq(sourceForSpeaker(at("R",0.5f),surround,8,kSevenPointOne),0.60f,"rear R");
  eq(sourceForSpeaker(at("L",0),surround,8,kSevenPointOne),0.70f,"side L");
  eq(sourceForSpeaker(at("R",0),surround,8,kSevenPointOne),0.80f,"side R");
  const float side51[]={0.1f,0.2f,0.3f,0.4f,0.5f,0.6f};
  eq(sourceForSpeaker(at("L",0.5f),side51,6,kFivePointOneSide),0.5f,"5.1 side fallback");
  eq(sourceForSpeaker(at("SL"),side51,6,kFivePointOneSide),0.5f,"native SL");
  eq(sourceForSpeaker(at("BL"),side51,6,kFivePointOneSide),0.0f,"no invented rear");
  const float back51[]={0.1f,0.2f,0.3f,0.4f,0.5f,0.6f};
  eq(sourceForSpeaker(at("BL"),back51,6,kFivePointOneBack),0.5f,"back 5.1");
  const float pcm21[]={0.1f,0.2f,0.3f};
  eq(sourceForSpeaker(at("M",0,"sub"),pcm21,3,kTwoPointOne),0.3f,"2.1 LFE");
  std::cout<<"PASS: channel masks 2.0/2.1/5.1/7.1, stereo neutrality, discrete LFE, legacy routing\n";
  return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
