// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace tas {
// Single-producer, single-consumer, same-process lock-free PCM frame ring.
// Fixed capacity + channel count: writes/reads never allocate or block.
// Next step: use the same *protocol concepts* for a signed virtual-device
// transport; this class is NOT a kernel driver or shared-memory IPC.
class AudioFrameRing {
  const size_t capacity_,channels_;
  std::vector<float> samples_;
  alignas(64) std::atomic<uint64_t> writeFrames_{0};
  alignas(64) std::atomic<uint64_t> readFrames_{0};
public:
  AudioFrameRing(size_t capacityFrames,size_t channels)
   :capacity_(capacityFrames),channels_(channels),samples_(capacityFrames*channels,0.0f){
    if(capacityFrames==0||capacityFrames>480000||!(channels==2||channels==3||channels==6||channels==8))
      throw std::invalid_argument("Ring requires 1..480000 frames and 2/3/6/8 channels");
  }
  size_t channels()const noexcept {return channels_;}
  size_t capacityFrames()const noexcept {return capacity_;}
  size_t availableFrames()const noexcept {
    const auto w=writeFrames_.load(std::memory_order_acquire);
    const auto r=readFrames_.load(std::memory_order_acquire);
    return static_cast<size_t>(w-r);
  }
  // Returns accepted frames; excess frames are discarded (no overwrites).
  size_t write(const float* input,size_t frames){
    if(frames==0)return 0;
    if(!input)throw std::invalid_argument("Null input");
    const auto w=writeFrames_.load(std::memory_order_relaxed);
    const auto r=readFrames_.load(std::memory_order_acquire);
    const size_t actual=std::min(frames,capacity_-static_cast<size_t>(w-r));
    for(size_t f=0;f<actual;++f)
      std::copy_n(input+f*channels_,channels_,samples_.data()+((w+f)%capacity_)*channels_);
    writeFrames_.store(w+actual,std::memory_order_release);
    return actual;
  }
  // Always writes requested output frames; pads underruns with silence.
  size_t read(float* output,size_t frames){
    if(frames==0)return 0;
    if(!output)throw std::invalid_argument("Null output");
    const auto r=readFrames_.load(std::memory_order_relaxed);
    const auto w=writeFrames_.load(std::memory_order_acquire);
    const size_t actual=std::min(frames,static_cast<size_t>(w-r));
    for(size_t f=0;f<actual;++f)
      std::copy_n(samples_.data()+((r+f)%capacity_)*channels_,channels_,output+f*channels_);
    std::fill(output+actual*channels_,output+frames*channels_,0.0f);
    readFrames_.store(r+actual,std::memory_order_release);
    return actual;
  }
};
}
