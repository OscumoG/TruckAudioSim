// SPDX-License-Identifier: MIT
#include "wavert_model.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tas {
WaveRtModel::WaveRtModel(StreamFormat fmt,size_t cap):format_(fmt),capacity_(cap),ring_(cap*fmt.channels,0.0f){
 validateStreamFormat(fmt);
 if(cap==0||cap>480000)throw std::invalid_argument("Invalid WaveRT history capacity");
}
uint64_t WaveRtModel::nextFrame() const{
 std::lock_guard lock(mutex_);return nextFrame_;
}
void WaveRtModel::submit(const DriverPacketInfo& info,std::span<const float> pcm){
 // Exact validation against the portable on-wire ABI, including the frame limit.
 (void)encodeDriverHeader(info);
 if(info.format.channels!=format_.channels||info.format.channelMask!=format_.channelMask||
    info.format.sampleRate!=format_.sampleRate||info.format.sampleFormat!=format_.sampleFormat||
    info.format.sourceEndpointToken!=format_.sourceEndpointToken)
  throw std::invalid_argument("WaveRT render format/source mismatch");
 if(pcm.size()!=size_t(info.frameCount)*format_.channels)
  throw std::invalid_argument("WaveRT render block size mismatch");
 for(float sample:pcm)if(!std::isfinite(sample))
  throw std::invalid_argument("Invalid render sample");
 std::lock_guard lock(mutex_);
 if(info.firstFrame<nextFrame_)throw std::invalid_argument("Overlapping/out-of-order render frames");
 if(info.firstFrame>UINT64_MAX-info.frameCount)throw std::invalid_argument("Render counter overflow");
 // Any missing timestamp range is silence. Large gaps clear stale history.
 if(info.firstFrame-nextFrame_>=capacity_){
  std::fill(ring_.begin(),ring_.end(),0.0f);
  nextFrame_=info.firstFrame;
  oldestFrame_=nextFrame_;
 }else{
  for(uint64_t f=nextFrame_;f<info.firstFrame;++f){
   const auto offset=(f%capacity_)*format_.channels;
   std::fill_n(ring_.data()+offset,format_.channels,0.0f);
  }
  nextFrame_=info.firstFrame;
 }
 for(size_t f=0;f<info.frameCount;++f){
  const auto offset=((nextFrame_+f)%capacity_)*format_.channels;
  std::copy_n(pcm.data()+f*format_.channels,format_.channels,ring_.data()+offset);
 }
 nextFrame_+=info.frameCount;
 oldestFrame_=std::max(oldestFrame_, (nextFrame_>capacity_)?nextFrame_-capacity_:0);
}
LoopbackReadResult WaveRtModel::capture(uint64_t first,size_t frames,std::span<float> out) const{
 if(frames>out.size()/format_.channels)throw std::invalid_argument("Loopback buffer too small");
 if(first>UINT64_MAX-frames)throw std::invalid_argument("Loopback counter overflow");
 std::lock_guard lock(mutex_);
 LoopbackReadResult result{};
 for(size_t f=0;f<frames;++f){
  const uint64_t at=first+f;
  auto* sample=out.data()+f*format_.channels;
  if(at<oldestFrame_){
   std::fill_n(sample,format_.channels,0.0f);++result.overwrittenFrames;
  }else if(at>=nextFrame_){
   std::fill_n(sample,format_.channels,0.0f);++result.notYetAvailableFrames;
  }else{
   std::copy_n(ring_.data()+(at%capacity_)*format_.channels,format_.channels,sample);
   ++result.validFrames;
  }
 }
 return result;
}
} // namespace tas
