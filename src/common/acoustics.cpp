// SPDX-License-Identifier: MIT
#include "acoustics.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace tas {
namespace {
float distance(Vec3 a, Vec3 b) { return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z); }
float coefficient(float hz, float rate) {
 return 1.0f-std::exp(-2.0f*std::numbers::pi_v<float>*hz/rate);
}
std::array<float,3> axes(Vec3 v) { return {v.x,v.y,v.z}; }
float smoothstep(float t) {
 t=std::clamp(t,0.0f,1.0f);
 return t*t*(3.0f-2.0f*t);
}
}

void CabinAcoustics::reset() {
 history_.clear();
 taps_={};
 cursor_=0;
 damped_=filtered_=occlusion_=0;
 loss_=1;
}

void CabinAcoustics::process(std::span<float> samples, const Acoustics& room, const Speaker& speaker,
                             const Pose& pose, float rate) {
 if (!room.enabled) {
  if (!history_.empty()) reset();
  return;
 }
 const size_t size=static_cast<size_t>(rate*0.1f)+4;
 const bool initialize=history_.size()!=size;
 if (initialize) {
  reset();
  history_.resize(size);
 }
 const auto lo=axes(room.min), hi=axes(room.max), p=axes(speaker.pos);
 const float direct=distance(speaker.pos,pose.head);
 const float upper=smoothstep((pose.head.y-room.deckHeight+0.18f)/0.36f);
 const float deck=room.splitDeck && !pose.exterior ? (speaker.pos.y<room.deckHeight?upper:1.0f-upper) : 0.0f;
 std::array<Tap,6> target{};
 for (size_t i=0; i<target.size(); ++i) {
  const size_t axis=i/2;
  // The deck surface bounds the current acoustic compartment.
  float wall=i%2?hi[axis]:lo[axis];
  if (room.splitDeck && axis==1 && ((i%2 && speaker.pos.y<room.deckHeight) ||
      (!(i%2) && speaker.pos.y>=room.deckHeight))) wall=room.deckHeight;
  auto image=p;
  image[axis]=2.0f*wall-image[axis];
  const float path=distance({image[0],image[1],image[2]},pose.head);
  auto& tap=target[i];
  tap.delay=std::clamp((path-direct)*rate/343.0f,1.0f,static_cast<float>(size-2));
  constexpr std::array<float,6> surfaces{0.85f,0.85f,0.46f,0.66f,0.9f,0.53f};
  const float opening=axis==0 ? (i%2?room.rightWindow:room.leftWindow) : 0.0f;
  tap.gain=pose.exterior?0.0f:room.reflections*(1.0f-0.65f*room.absorption)*
     surfaces[i]*(1.0f-0.55f*opening)*(1.0f-0.65f*deck)/(1.0f+path);
  if (path<direct) tap.gain=0;
 }
 float opening=0;
 if (pose.exterior) {
  const float windowY=std::clamp(0.16f,lo[1],hi[1]);
  const float windowZ=std::clamp(-0.20f,lo[2],hi[2]);
  for (size_t side=0; side<2; ++side) {
   const float wall=side?hi[0]:lo[0];
   const auto aperture=Vec3{wall,windowY,windowZ};
   const float outward=std::max(0.0f,(pose.head.x-wall)*(side?1.0f:-1.0f));
   const float range=distance(aperture,pose.head);
   const float facing=std::clamp((outward+0.35f)/(range+0.35f),0.0f,1.0f);
   const float sourceDistance=distance(aperture,speaker.pos);
   const float fraction=side?room.rightWindow:room.leftWindow;
   opening+=fraction*facing/(1.0f+0.20f*sourceDistance);
  }
  opening=std::clamp(opening,0.0f,1.0f);
 }
 const float closed=pose.exterior?1.0f-opening:0.0f;
 const float low=std::clamp(closed+deck*(1.0f-closed),0.0f,1.0f);
 const float targetLoss=dbToAmplitude(-(room.bodyLoss*closed+7.0f*deck));
 const float body=coefficient(pose.exterior?room.bodyCutoff:950.0f,rate);
 const float damping=coefficient(5200.0f-3700.0f*room.absorption,rate);
 const float smoothing=coefficient(11.0f,rate);
 if (initialize) for (size_t i=0;i<taps_.size();++i) taps_[i].delay=target[i].delay;
 for (float& sample:samples) {
  damped_+=damping*(sample-damped_);
  history_[cursor_]=damped_;
  float early=0;
  for (size_t i=0; i<taps_.size(); ++i) {
   auto& tap=taps_[i];
   tap.gain+=smoothing*(target[i].gain-tap.gain);
   if (tap.gain<1e-6f) continue;
   tap.delay+=std::clamp(smoothing*(target[i].delay-tap.delay),-0.12f,0.12f);
   const size_t whole=static_cast<size_t>(tap.delay);
   const size_t index=(cursor_+size-whole)%size;
   early+=tap.gain*std::lerp(history_[index],history_[(index+size-1)%size],tap.delay-whole);
  }
  if (++cursor_==size) cursor_=0;
  const float dry=sample+early;
  filtered_+=body*(dry-filtered_);
  loss_+=smoothing*(targetLoss-loss_);
  occlusion_+=smoothing*(low-occlusion_);
  sample=loss_*std::lerp(dry,filtered_,occlusion_);
 }
}

void SpeakerTest::prepare(unsigned sequence, unsigned rate) {
 if (sequence==sequence_) return;
 sequence_=sequence;
 rate_=rate;
 cursor_=0;
 length_=sequence ? static_cast<size_t>(rate*0.74f) : 0;
}

void SpeakerTest::render(std::span<float> output, bool subwoofer) const {
 const size_t ramp=rate_/40;
 for (size_t i=0; i<output.size(); ++i) {
  const size_t n=cursor_+i;
  if (n>=length_) { output[i]=0; continue; }
  const float t=static_cast<float>(n)/rate_;
  const size_t half=length_/2, local=n%half;
  const float envelope=smoothstep(static_cast<float>(std::min(local,half-local))/ramp);
  const auto noise=[](uint32_t seed) {
   seed=(seed^(seed>>16))*0x7feb352du;
   seed=(seed^(seed>>15))*0x846ca68bu;
   return static_cast<float>((seed^(seed>>16))>>8)/8388608.0f-1.0f;
  };
  const float tone=std::sin(2.0f*std::numbers::pi_v<float>*(subwoofer?71.0f:360.0f)*t);
  output[i]=envelope*(subwoofer?0.19f*tone:0.045f*tone+0.10f*noise(static_cast<uint32_t>(n))+
      0.035f*noise(static_cast<uint32_t>(n/8)));
 }
}
}
