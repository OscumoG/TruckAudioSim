// SPDX-License-Identifier: MIT
#include "channel_layout.hpp"
#include "routing.hpp"
#include "spatial_mix.hpp"
#include "wav.hpp"
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <vector>
#include <string>

int main(){
 using namespace tas;
 const struct Layout{uint16_t count;uint32_t mask;const char* names;} layouts[]={
  {2,kStereo,"FL FR"},{3,kTwoPointOne,"FL FR LFE"},
  {6,kFivePointOneSide,"FL FR FC LFE SL SR"},
  {6,kFivePointOneBack,"FL FR FC LFE BL BR"},
  {8,kSevenPointOne,"FL FR FC LFE BL BR SL SR"}
 };
 for(const auto& l:layouts){
  auto p=makeChannelTestProfile(l.count,l.mask);
  assert(p.speakers.size()==l.count);
  std::string names;
  for(size_t i=0;i<p.speakers.size();++i){
   const auto& speaker=p.speakers[i];
   if(!names.empty())names+=' ';
   names+=speaker.channel;
   // Isolate each input PCM channel and verify that exactly one object hears it.
   for(size_t source=0;source<l.count;++source){
    std::vector<float> pcm(l.count,0.0f);
    pcm[source]=0.25f;
    float mono[4]={99,99,99,99};
    const auto stats=mixSpatialObject(speaker,pcm,1,4,l.count,l.mask,1.0f,true,mono);
    assert(std::abs(mono[0]-(i==source?0.25f:0.0f))<0.000001f);
    for(size_t k=1;k<4;++k)assert(mono[k]==0.0f);
    assert(stats.clippedSamples==0);
   }
  }
  assert(names==l.names);
 }
 bool thrown=false;
 try{(void)makeChannelTestProfile(8,kFivePointOneSide);}catch(const std::invalid_argument&){thrown=true;}
 assert(thrown);
 thrown=false;
 try{(void)makeChannelTestProfile(6,0);}catch(const std::invalid_argument&){thrown=true;}
 assert(thrown);
}
