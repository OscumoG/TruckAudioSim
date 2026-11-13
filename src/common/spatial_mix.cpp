// SPDX-License-Identifier: MIT
#include "spatial_mix.hpp"
#include "routing.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace tas {
float SpeakerFilter::Section::process(float sample) {
 const float out=b0*sample+z1;
 z1=b1*sample-a1*out+z2;
 z2=b2*sample-a2*out;
 return out;
}

float SpeakerFilter::Section::magnitude(double cosine) const {
 const double sineSquared=std::max(0.0,1-cosine*cosine);
 const auto norm=[=](double first,double middle,double last){
  const double real=middle+(first+last)*cosine;
  const double imaginary=first-last;
  return real*real+imaginary*imaginary*sineSquared;
 };
 return static_cast<float>(std::sqrt(norm(b0,b1,b2)/norm(1,a1,a2)));
}

void SpeakerFilter::configure(const Speaker& speaker,float sampleRate) {
 count_=0;
 if(speaker.cabAudio==CabAudio::Legacy)return;
 const auto add=[&](float hz,bool highPass){
  const float angle=2*std::numbers::pi_v<float>*hz/sampleRate;
  const float c=std::cos(angle);
  const float alpha=std::sin(angle)/std::numbers::sqrt2_v<float>;
  const float a0=1+alpha;
  const float b0=(highPass?1+c:1-c)/(2*a0);
  const Section section{b0,(highPass?-2:2)*b0,b0,-2*c/a0,(1-alpha)/a0};
  sections_[count_++]=section;
  sections_[count_++]=section;
 };
 const auto& type=speaker.type;
 if(type=="sub")add(120,false);
 else if(type=="small")add(120,true);
 else if(type=="tweeter")add(2500,true);
 else if(type=="mid"){
  add(500,true);
  add(2500,false);
 }else if(type=="midbass"){
  add(60,true);
  add(2500,false);
 }
}

void SpeakerFilter::reset() {
 for(size_t i=0;i<count_;++i)sections_[i].z1=sections_[i].z2=0;
}

float SpeakerFilter::process(float sample) {
 for(size_t i=0;i<count_;++i)sample=sections_[i].process(sample);
 return sample;
}

float SpeakerFilter::magnitude(double cosine) const {
 float response=1;
 for(size_t i=0;i<count_;++i)response*=sections_[i].magnitude(cosine);
 return response;
}

SpatialMixStats mixSpatialObject(const Speaker& speaker,
                                 const std::vector<float>& interleaved,
                                 size_t validFrames, size_t outputFrames,
                                 uint16_t channels, uint32_t channelMask,
                                 float gain, bool electronicsOn, float* mono,
                                 SpeakerFilter* filter) {
 if (!mono && outputFrames) throw std::invalid_argument("Null spatial output");
 if (!channels || validFrames > outputFrames ||
     validFrames > interleaved.size() / channels || !std::isfinite(gain) || gain < 0.0f)
  throw std::invalid_argument("Invalid spatial PCM frame count, format or gain");
 SpatialMixStats stats;
 if (!outputFrames) return stats;
 if (!electronicsOn) {
  if(filter)filter->reset();
  std::fill_n(mono, outputFrames, 0.0f);
  return stats;
 }
 const auto route = speakerRoute(speaker, channels, channelMask);
 for (size_t f = 0; f < validFrames; ++f) {
  const float dry = sampleForRoute(route, interleaved.data() + f * channels, channels);
  const float raw = (filter?filter->process(dry):dry) * gain;
  // Catch nonfinite input before it can reach the Windows spatial renderer.
  const float safe = std::isfinite(raw) ? raw : 0.0f;
  stats.inputPeak = std::max(stats.inputPeak, std::abs(safe));
  if (std::abs(safe) > 1.0f) ++stats.clippedSamples;
  const float sample = std::clamp(safe, -1.0f, 1.0f);
  stats.outputPeak = std::max(stats.outputPeak, std::abs(sample));
  mono[f] = sample;
 }
 if(filter && validFrames<outputFrames)filter->reset();
 std::fill(mono + validFrames, mono + outputFrames, 0.0f);
 return stats;
}
}
