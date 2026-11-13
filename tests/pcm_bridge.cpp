// SPDX-License-Identifier: MIT
#include "pcm_bridge.hpp"
#include "wav.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace tas;
static void basic(uint16_t channels,uint32_t mask){
 auto ring=std::make_unique<SharedPcmBridge>();
 ring->initialize(StreamFormat{channels,48000,mask,kStreamFloat32,17});
 ring->validate();
 std::vector<float> src(480*channels),out(480*channels);
 for(size_t f=0;f<480;++f)for(size_t c=0;c<channels;++c)
  src[f*channels+c]=float(c+1)/float(channels+1);
 assert(ring->push(src,480)==480);
 assert(ring->bufferedFrames()==480);
 assert(ring->pop(out,480)==480);
 assert(out==src);
 assert(ring->bufferedFrames()==0);
 assert(ring->pop(out,480)==0);
 for(auto v:out)assert(v==0);
 assert(ring->paddedFrames.load()==480);
 // Check wraparound and channel order on a ring with non-divisible block sizes.
 std::vector<float> block(317*channels),read(317*channels);
 for(size_t cycle=0;cycle<100;++cycle){
  for(size_t f=0;f<317;++f)for(size_t c=0;c<channels;++c)
   block[f*channels+c]=float((cycle+f+c)%37)/40.f;
  assert(ring->push(block,317)==317);
  assert(ring->pop(read,317)==317);
  assert(read==block);
 }
 std::vector<float> full(kBridgeFrames*channels,0.35f);
 assert(ring->push(full,kBridgeFrames)==kBridgeFrames);
 assert(ring->push(full,100)==0);
 assert(ring->droppedFrames.load()==100);
 std::vector<float> inspect(kBridgeFrames*channels);
 assert(ring->pop(inspect,kBridgeFrames)==kBridgeFrames);
 assert(inspect==full);
 std::vector<float> poison(channels,1.5f);
 poison[0]=std::numeric_limits<float>::quiet_NaN();
 assert(ring->push(poison,1)==1);
 std::vector<float> clean(channels);
 assert(ring->pop(clean,1)==1);
 assert(clean[0]==0);
 for(size_t c=1;c<channels;++c)assert(clean[c]==1.0f);
}
static void stress(){
 auto ring=std::make_unique<SharedPcmBridge>();
 ring->initialize(StreamFormat{8,48000,kSevenPointOne,kStreamFloat32,0});
 constexpr int blocks=3000,frames=32;
 std::thread writer([&]{
  for(int i=0;i<blocks;++i){
   std::vector<float> pcm(frames*8);
   for(int f=0;f<frames;++f)for(int c=0;c<8;++c)
    pcm[f*8+c]=float((i*frames+f)%1000)/1024.f;
   while(ring->bufferedFrames()>kBridgeFrames-frames)std::this_thread::yield();
   assert(ring->push(pcm,frames)==frames);
  }
 });
 size_t obtained=0;
 std::vector<float> out(frames*8);
 while(obtained<blocks*frames){
  if(ring->bufferedFrames()<frames){std::this_thread::yield();continue;}
  assert(ring->pop(out,frames)==frames);
  for(int f=0;f<frames;++f)for(int c=0;c<8;++c)
   assert(out[f*8+c]==float((obtained+f)%1000)/1024.f);
  obtained+=frames;
 }
 writer.join();
 assert(ring->droppedFrames.load()==0);
}
int main(){
 basic(2,kStereo);basic(3,kTwoPointOne);
 basic(6,kFivePointOneSide);basic(6,kFivePointOneBack);basic(8,kSevenPointOne);
 stress();
 std::cout<<"PCM shared bridge: 2.0/2.1/5.1/7.1, wrap, xrun, multithread OK\n";
}
