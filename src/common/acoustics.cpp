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
 cachedRate_=0;
 taps_={};
 for (auto& line:diffuse_) {
  line.buffer.clear();
  line.cursor=0;
  line.low=0;
 }
 cursor_=0;
 damped_=bright_=filtered_=occlusion_=wet_=0;
 loss_=1;
}

void CabinAcoustics::updateParameters(const Acoustics& room, Vec3 source, Vec3 head, bool exterior, float rate) {
 auto lo=axes(room.min), hi=axes(room.max);
 const auto p=axes(source);
 const float direct=distance(source,head);
 const float upper=smoothstep((head.y-room.deckHeight+0.18f)/0.36f);
 const bool lowerSpeaker=source.y<room.deckHeight;
 const float deck=room.splitDeck && !exterior ? (lowerSpeaker?upper:1.0f-upper) : 0.0f;
 const float windowY=std::clamp(0.16f,lo[1],hi[1]);
 const float windowZ=std::clamp(-0.20f,lo[2],hi[2]);
 if (room.splitDeck) (lowerSpeaker?hi:lo)[1]=room.deckHeight;

 auto& target=parameters_.paths;
 target={};
 if (!exterior && room.reflections>0) {
  // Side glass, floor, ceiling, windshield and the usually upholstered rear.
  const std::array<float,6> surfaces{
   0.84f-0.50f*room.leftWindow,0.84f-0.50f*room.rightWindow,0.48f,0.68f,0.92f,0.51f};
  constexpr std::array<float,6> brightness{0.78f,0.78f,0.22f,0.46f,0.92f,0.28f};
  const float reflection=room.reflections*(1.0f-0.68f*room.absorption)*(1.0f-0.75f*deck);
  size_t count=0;
  const auto reflect=[&](std::array<float,3>& image,size_t wall) {
   const size_t axis=wall/2;
   image[axis]=2.0f*(wall%2?hi[axis]:lo[axis])-image[axis];
  };
  const auto add=[&](std::array<float,3> image,float strength,float highs) {
   auto& tap=target[count++];
   const float path=distance({image[0],image[1],image[2]},head);
   if (path<direct) return;
   tap.delay=std::clamp((path-direct)*rate/343.0f,1.0f,static_cast<float>(history_.size()-2));
   tap.gain=reflection*strength*(0.65f+direct)/(0.85f+path);
   tap.brightness=highs;
  };
  for (size_t first=0;first<surfaces.size();++first) {
   auto image=p;
   reflect(image,first);
   add(image,surfaces[first],brightness[first]);
   for (size_t second=0;second<surfaces.size();++second) {
    // Same-axis paths need distinct parallel surfaces. A pair of perpendicular planes
    // gives the same image either way, so include it once.
    if (first==second || (first/2!=second/2 && second<first)) continue;
    auto secondImage=image;
    reflect(secondImage,second);
    add(secondImage,0.48f*surfaces[first]*surfaces[second],brightness[first]*brightness[second]);
   }
  }
 }

 float opening=0;
 if (exterior) {
  for (size_t side=0;side<2;++side) {
   const float wall=side?hi[0]:lo[0];
   const auto aperture=Vec3{wall,windowY,windowZ};
   const float outward=std::max(0.0f,(head.x-wall)*(side?1.0f:-1.0f));
   const float range=distance(aperture,head);
   const float facing=std::clamp((outward+0.35f)/(range+0.35f),0.0f,1.0f);
   const float sourceDistance=distance(aperture,source);
   const float fraction=side?room.rightWindow:room.leftWindow;
   opening+=fraction*facing/(1.0f+0.20f*sourceDistance);
  }
  opening=std::clamp(opening,0.0f,1.0f);
 }
 const float closed=exterior?1.0f-opening:0.0f;
 const float low=std::clamp(closed+deck*(1.0f-closed),0.0f,1.0f);
 const float targetLoss=dbToAmplitude(-(room.bodyLoss*closed+9.0f*deck));
 const float openWindows=0.5f*(room.leftWindow+room.rightWindow);
 const float targetWet=exterior?0.0f:0.38f*room.reverb*(1.0f-0.55f*openWindows)*(1.0f-0.32f*deck);
 parameters_.loss=targetLoss;
 parameters_.occlusion=low;
 parameters_.wet=targetWet;
 // Moving the listener only changes the paths, not the material or the
 // dimensions of the speaker's compartment.
 if (rate!=cachedRate_ || room!=cachedRoom_ || exterior!=cachedExterior_ ||
     (room.splitDeck && lowerSpeaker!=(cachedSource_.y<room.deckHeight))) {
  const float body=coefficient(exterior?room.bodyCutoff:830.0f,rate);
  const float damping=coefficient(3300.0f-2500.0f*room.absorption,rate);
  const float brightness=coefficient(8700.0f-4200.0f*room.absorption,rate);
  const float smoothing=coefficient(14.0f,rate);
  const float x=std::max(0.2f,hi[0]-lo[0]), y=std::max(0.2f,hi[1]-lo[1]), z=std::max(0.2f,hi[2]-lo[2]);
  // 4V/S estimates the mean distance between wall strikes. Unequal lines keep
  // the late decay diffuse, and the longest line follows the depth of the cabin.
  const float flight=2.0f*x*y*z/((x*y+x*z+y*z)*343.0f);
  const std::array<float,4> delays{
   0.008f+flight*1.19f,0.013f+flight*1.63f,0.019f+flight*2.11f,
   0.024f+flight*2.57f+z/1372.0f};
  const float decay=std::max(0.035f,room.decayMs*0.001f*(1.0f-0.65f*openWindows));
  parameters_.body=body;
  parameters_.damping=damping;
  parameters_.brightness=brightness;
  parameters_.smoothing=smoothing;
  for (size_t i=0;i<delays.size();++i) {
   parameters_.lineLengths[i]=static_cast<size_t>(delays[i]*rate)|1;
   parameters_.feedback[i]=std::pow(0.001f,delays[i]/decay);
  }
 }
 cachedRoom_=room;
 cachedSource_=source;
 cachedHead_=head;
 cachedExterior_=exterior;
 cachedRate_=rate;
}

void CabinAcoustics::process(std::span<float> samples, const Acoustics& room, const Speaker& speaker,
                             const Pose& pose, float rate) {
 if (!room.enabled) {
  if (!history_.empty()) reset();
  return;
 }
 const size_t size=static_cast<size_t>(rate*0.13f)+4;
 const bool initialize=history_.size()!=size;
 if (initialize) {
  reset();
  history_.resize(size);
 }
 // Camera orientation is handled by the spatial renderer. A turn in place
 // keeps the same acoustic paths and room filter coefficients.
 if (rate!=cachedRate_ || room!=cachedRoom_ || speaker.pos!=cachedSource_ ||
     pose.head!=cachedHead_ || pose.exterior!=cachedExterior_)
  updateParameters(room,speaker.pos,pose.head,pose.exterior,rate);
 const auto& [target, lineLengths, feedback, targetLoss, low, targetWet, body, damping, brightness, smoothing]=parameters_;
 if (targetWet>0 || wet_>1e-6f) for (size_t i=0;i<diffuse_.size();++i) {
  auto& line=diffuse_[i];
  if (line.buffer.size()!=lineLengths[i]) {
   line.buffer.assign(lineLengths[i],0);
   line.cursor=0;
   line.low=0;
  }
 }
 if (initialize) {
  taps_=target;
  loss_=targetLoss;
  occlusion_=low;
  wet_=targetWet;
 }
 // Prepare the audible paths once per block. Outside the cab this list empties
 // after the reflections fade. Stationary paths reuse the delay decomposition.
 struct BlockTap { size_t index, whole; float fraction; bool steady; };
 std::array<BlockTap,24> active;
 size_t activeCount=0;
 for (size_t i=0;i<taps_.size();++i) {
  auto& tap=taps_[i];
  const auto& goal=target[i];
  if (goal.gain==0 && tap.gain<1e-6f) {
   tap.gain=0;
   continue;
  }
  const bool steady=tap.gain==goal.gain && tap.delay==goal.delay;
  if (steady) tap.brightness=goal.brightness;
  const size_t whole=static_cast<size_t>(tap.delay);
  active[activeCount++]={i,whole,tap.delay-whole,steady};
 }
 for (float& sample:samples) {
  damped_+=damping*(sample-damped_);
  bright_+=brightness*(sample-bright_);
  history_[cursor_]={damped_,bright_};
  float early=0;
  for (size_t j=0;j<activeCount;++j) {
   const auto& path=active[j];
   auto& tap=taps_[path.index];
   size_t whole=path.whole;
   float frac=path.fraction;
   if (!path.steady) {
    const auto& goal=target[path.index];
    tap.gain+=smoothing*(goal.gain-tap.gain);
    if (tap.gain<1e-6f) continue;
    if (goal.gain>0) {
     tap.delay+=std::clamp(smoothing*(goal.delay-tap.delay),-0.10f,0.10f);
     tap.brightness=goal.brightness;
    }
    whole=static_cast<size_t>(tap.delay);
    frac=tap.delay-whole;
   }
   const size_t index=cursor_>=whole?cursor_-whole:cursor_+size-whole;
   const auto& next=history_[index];
   const auto& prev=history_[index?index-1:size-1];
   early+=tap.gain*std::lerp(std::lerp(next[0],prev[0],frac),
                            std::lerp(next[1],prev[1],frac),tap.brightness);
  }
  if (++cursor_==size) cursor_=0;
  float late=0;
  wet_+=smoothing*(targetWet-wet_);
  if (wet_>1e-6f) {
   std::array<float,4> delayed{};
   float sum=0;
   for (size_t i=0;i<diffuse_.size();++i) {
    auto& line=diffuse_[i];
    const float value=line.buffer[line.cursor];
    line.low+=damping*(value-line.low);
    delayed[i]=line.low*feedback[i];
    sum+=delayed[i];
    late+=(i==1 || i==2)?-0.84f*value:value;
   }
   for (size_t i=0;i<diffuse_.size();++i) {
    auto& line=diffuse_[i];
    // Orthogonal Householder feedback and frequency-dependent damping.
    line.buffer[line.cursor]=0.5f*damped_+delayed[i]-0.5f*sum;
    if (++line.cursor==line.buffer.size()) line.cursor=0;
   }
  }
  const float dry=sample+early+wet_*late;
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
