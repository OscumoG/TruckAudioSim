// SPDX-License-Identifier: MIT
#include "normalization.hpp"
#include "routing.hpp"
#include "spatial_mix.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
namespace tas {
std::vector<float> neutralVoiceGains(const std::vector<Speaker>& speakers,
                                     uint16_t channels,uint32_t mask,float peakCeiling,float sampleRate){
 if(!(peakCeiling>0&&peakCeiling<=1) || !std::isfinite(peakCeiling))throw std::invalid_argument("Invalid peak ceiling");
 struct Route {
  size_t index;
  bool phantomCenter;
  SpeakerFilter filter;
 };
 std::vector<float> gains;gains.reserve(speakers.size());
 std::vector<Route> routes;routes.reserve(speakers.size());
 std::vector<float> budgets(channels,0);
 bool filtered=false;
 for(const Speaker& s:speakers){
  const float gain=s.enabled?dbToAmplitude(s.gainDb):0.0f;
  if(!std::isfinite(gain))throw std::invalid_argument("Nonfinite speaker gain");
  gains.push_back(gain);
  const uint32_t bit=sourceChannelBit(s,channels,mask);
  const bool phantomCenter=channels==2&&bit==0x4u;
  size_t idx=0;for(uint32_t b=1;b<bit;b<<=1)if(mask&b)++idx;
  auto& route=routes.emplace_back(Route{phantomCenter||((mask&bit)&&idx<channels)?idx:size_t(channels),phantomCenter,{}});
  if(sampleRate>0 && bit && gain>0 && s.cabAudio!=CabAudio::Legacy){
   route.filter.configure(s,sampleRate);
   filtered=true;
  }
 }
 // A crossover's two drivers share a channel but contribute to different
 // bands. Estimate the worst combined response across the audible spectrum.
 const auto budgetAt=[&](double cosine){
  std::fill(budgets.begin(),budgets.end(),0);
  for(size_t i=0;i<routes.size();++i){
   const auto& route=routes[i];
   const float gain=gains[i]*route.filter.magnitude(cosine);
   if(route.phantomCenter){
    budgets[0]+=gain*0.5f;budgets[1]+=gain*0.5f;
   }else if(route.index<budgets.size())budgets[route.index]+=gain;
  }
  return budgets.empty()?0.0f:*std::max_element(budgets.begin(),budgets.end());
 };
 float largest=budgetAt(1);
 if(filtered){
  constexpr int steps=96;
  const double ratio=std::pow(sampleRate/40.0,1.0/steps);
  for(int i=0;i<=steps;++i){
   const double frequency=20*std::pow(ratio,i);
   const double cosine=std::cos(2*std::numbers::pi*frequency/sampleRate);
   largest=std::max(largest,budgetAt(cosine));
  }
 }
 if(largest>peakCeiling){const float factor=peakCeiling/largest;for(float& g:gains)g*=factor;}
 return gains;
}
}
