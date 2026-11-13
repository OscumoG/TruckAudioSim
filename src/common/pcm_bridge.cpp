// SPDX-License-Identifier: MIT
#include "pcm_bridge.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tas {
void validatePcmProducerFormat(const StreamFormat& bridge, const StreamFormat& producer) {
 validateStreamFormat(bridge);
 validateStreamFormat(producer);
 if(bridge.channels!=producer.channels || bridge.sampleRate!=producer.sampleRate ||
    bridge.channelMask!=producer.channelMask || bridge.sampleFormat!=producer.sampleFormat)
  throw std::invalid_argument("PCM producer format mismatches shared bridge");
 if(!bridge.sourceEndpointToken || !producer.sourceEndpointToken ||
    bridge.sourceEndpointToken!=producer.sourceEndpointToken)
  throw std::invalid_argument("PCM source ID missing or mismatched; specify --source-index / --source-id on the bridge");
}

void SharedPcmBridge::initialize(StreamFormat f) {
 ready.store(0,std::memory_order_release);
 validateStreamFormat(f);
 format=f;
 version=kBridgeVersion;
 capacityFrames=kBridgeFrames;
 writeCursor.store(0,std::memory_order_relaxed);
 readCursor.store(0,std::memory_order_relaxed);
 droppedFrames.store(0,std::memory_order_relaxed);
 paddedFrames.store(0,std::memory_order_relaxed);
 ready.store(kBridgeMagic,std::memory_order_release);
}
void SharedPcmBridge::validate() const {
 if(ready.load(std::memory_order_acquire)!=kBridgeMagic ||
    version!=kBridgeVersion || capacityFrames!=kBridgeFrames)
  throw std::runtime_error("Uninitialized or incompatible PCM shared bridge");
 validateStreamFormat(format);
 if(!writeCursor.is_lock_free() || !readCursor.is_lock_free())
  throw std::runtime_error("Bridge requires lock-free 64-bit atomics");
}
uint64_t SharedPcmBridge::bufferedFrames() const {
 const auto w=writeCursor.load(std::memory_order_acquire);
 const auto r=readCursor.load(std::memory_order_acquire);
 return (w>=r && w-r<=kBridgeFrames)?w-r:0;
}
size_t SharedPcmBridge::push(std::span<const float> input,size_t frames) {
 validate();
 if(frames>input.size()/format.channels)throw std::invalid_argument("Short PCM input");
 const auto w=writeCursor.load(std::memory_order_relaxed);
 const auto r=readCursor.load(std::memory_order_acquire);
 if(r>w || w-r>kBridgeFrames)throw std::runtime_error("Corrupt PCM bridge cursors");
 const size_t n=std::min<uint64_t>(frames,kBridgeFrames-(w-r));
 const auto channels=static_cast<size_t>(format.channels);
 for(size_t f=0;f<n;++f){
  const size_t at=((w+f)%kBridgeFrames)*kBridgeChannels;
  for(size_t ch=0;ch<channels;++ch){
   const float sample=input[f*channels+ch];
   samples[at+ch]=std::isfinite(sample)?std::clamp(sample,-1.0f,1.0f):0.0f;
  }
 }
 writeCursor.store(w+n,std::memory_order_release);
 if(frames>n)droppedFrames.fetch_add(frames-n,std::memory_order_relaxed);
 return n;
}
size_t SharedPcmBridge::pop(std::span<float> output,size_t frames) {
 validate();
 if(frames>output.size()/format.channels)throw std::invalid_argument("Short PCM output");
 const auto r=readCursor.load(std::memory_order_relaxed);
 const auto w=writeCursor.load(std::memory_order_acquire);
 if(r>w || w-r>kBridgeFrames)throw std::runtime_error("Corrupt PCM bridge cursors");
 const size_t n=std::min<uint64_t>(frames,w-r);
 const auto channels=static_cast<size_t>(format.channels);
 for(size_t f=0;f<n;++f){
  const size_t at=((r+f)%kBridgeFrames)*kBridgeChannels;
  for(size_t ch=0;ch<channels;++ch)output[f*channels+ch]=samples[at+ch];
 }
 std::fill(output.begin()+n*channels,output.begin()+frames*channels,0.0f);
 readCursor.store(r+n,std::memory_order_release);
 if(frames>n)paddedFrames.fetch_add(frames-n,std::memory_order_relaxed);
 return n;
}
}
