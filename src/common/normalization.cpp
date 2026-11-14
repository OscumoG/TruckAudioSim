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
  SpeakerRoute weights;
  SpeakerFilter filter;
 };
 std::vector<float> gains;gains.reserve(speakers.size());
 std::vector<Route> routes;routes.reserve(speakers.size());
 std::vector<float> budgets(channels,0);
 const bool distributed=std::any_of(speakers.begin(),speakers.end(),[](const auto& s){return s.coach;});
 bool filtered=false;
 for(const Speaker& s:speakers){
  const float gain=s.enabled?dbToAmplitude(s.gainDb):0.0f;
  if(!std::isfinite(gain))throw std::invalid_argument("Nonfinite speaker gain");
  gains.push_back(gain);
  auto& route=routes.emplace_back(Route{speakerRoute(s,channels,mask),{}});
  if(sampleRate>0 && gain>0 && s.cabAudio!=CabAudio::Legacy){
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
   for(size_t channel=0;channel<budgets.size();++channel){
    const float level=gain*route.weights[channel];
    budgets[channel]+=distributed?level*level:level;
   }
  }
  const float peak=budgets.empty()?0.0f:*std::max_element(budgets.begin(),budgets.end());
  // Coach speakers are spread along the vehicle and attenuated at the listener.
  return distributed?std::sqrt(peak):peak;
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
