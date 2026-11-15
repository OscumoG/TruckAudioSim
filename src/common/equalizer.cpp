// SPDX-License-Identifier: MIT
#include "equalizer.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace tas {
void EqualizerFilter::process(std::span<float> pcm, const Equalizer& equalizer, float rate) {
 const std::array<float,5> gains=equalizer.enabled?equalizer.gains:std::array<float,5>{};
 if (rate_!=rate || gains!=gains_) {
  const bool fresh=rate_!=rate;
  rate_=rate;
  gains_=gains;
  active_=std::ranges::any_of(gains,[](float gain){return gain!=0;});
  constexpr std::array<float,5> frequencies{80,250,1000,3000,8000};
  constexpr float tau=2*std::numbers::pi_v<float>;
  for (size_t i=0;i<bands_.size();++i) {
   auto& band=bands_[i];
   // Peaking filters with enough overlap for broad tonal adjustments.
   const float gain=std::pow(10.0f,gains[i]/40.0f);
   const float angle=tau*std::min(frequencies[i],0.45f*rate)/rate;
   const float alpha=std::sin(angle)/(2*0.85f);
   const float b1=-2*std::cos(angle), a0=1+alpha/gain;
   band.target={(1+alpha*gain)/a0,b1/a0,(1-alpha*gain)/a0,b1/a0,(1-alpha/gain)/a0};
   if (gains[i]==0) band.target={1,0,0,0,0};
  }
  // Reserve headroom for the combined response, including overlapping bands.
  float peak=1;
  if (active_) for (int i=0;i<=90;++i) {
   const float hz=20*std::pow(std::min(19000.0f,0.49f*rate)/20,i/90.0f);
   const auto z=std::polar(1.0f,-tau*hz/rate), z2=z*z;
   float response=1;
   for (const auto& band:bands_) {
    const auto& c=band.target;
    response*=std::abs((c[0]+c[1]*z+c[2]*z2)/(1.0f+c[3]*z+c[4]*z2));
   }
   peak=std::max(peak,response);
  }
  targetHeadroom_=1/peak;
  transition_=fresh?0:static_cast<unsigned>(rate*0.012f);
  if (fresh) {
   headroom_=targetHeadroom_;
   for (auto& band:bands_) {
    band.coefficients=band.target;
    band.z1=band.z2=0;
   }
  }
 }
 if (!active_ && !transition_) return;
 for (float& sample:pcm) {
  const float step=transition_?1.0f/transition_:0;
  headroom_+=(targetHeadroom_-headroom_)*step;
  float value=sample;
  for (auto& band:bands_) {
   auto& c=band.coefficients;
   if (transition_) for (size_t i=0;i<c.size();++i) c[i]+=(band.target[i]-c[i])*step;
   if (c[0]==1 && c[2]==0 && c[4]==0) {
    band.z1=band.z2=0;
    continue;
   }
   const float output=c[0]*value+band.z1;
   band.z1=c[1]*value-c[3]*output+band.z2;
   band.z2=c[2]*value-c[4]*output;
   value=output;
  }
  sample=value*headroom_;
  if (transition_) --transition_;
 }
}
}
