// SPDX-License-Identifier: MIT
#include "wavert_dma_tap.h"
#include "driver_wire.hpp"
#include "wav.hpp"
#include <cassert>
#include <bit>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>
#include <array>
using namespace tas;

static std::vector<uint8_t> pcm(uint64_t first,uint32_t frames,uint16_t n){
 std::vector<uint8_t> b(size_t(frames)*n*4);
 for(uint32_t f=0;f<frames;++f)for(uint32_t c=0;c<n;++c){
  const float v=float((first+f)%193)/256.0f+float(c)/32.0f;
  const uint32_t bits=std::bit_cast<uint32_t>(v);
  for(unsigned i=0;i<4;++i)b[(size_t(f)*n+c)*4+i]=uint8_t(bits>>(8*i));
 }
 return b;
}
static void place(std::vector<uint8_t>& ring,const std::vector<uint8_t>& b,
                  uint64_t first,uint16_t n){
 const size_t frame_bytes=size_t(n)*4, capacity=ring.size()/frame_bytes;
 for(size_t f=0;f<b.size()/frame_bytes;++f){
  const size_t off=((first+f)%capacity)*frame_bytes;
  std::memcpy(ring.data()+off,b.data()+f*frame_bytes,frame_bytes);
 }
}
static void verify(uint16_t channels,uint32_t mask){
 tas_dma_tap state{};
 assert(tas_dma_tap_init(&state,channels,mask,0xA0C0,0)==TAS_DMA_OK);
 const size_t frame_bytes=size_t(channels)*4;
 std::vector<uint8_t> ring(700*frame_bytes,0),out(40+960*frame_bytes);
 const auto first=pcm(0,480,channels); place(ring,first,0,channels);
 size_t bytes=0;uint64_t skipped=99;
 assert(tas_dma_tap_poll(&state,ring.data(),ring.size(),480,out.data(),out.size(),&bytes,&skipped)==TAS_DMA_OK);
 assert(skipped==0 && bytes==40+first.size() && state.next_frame==480);
 const auto decoded=decodeDriverPacket(std::span<const uint8_t>(out.data(),bytes));
 assert(decoded.info.firstFrame==0 && decoded.info.frameCount==480);
 assert(decoded.info.format.channelMask==mask && decoded.info.format.channels==channels);
 assert(std::memcmp(out.data()+40,first.data(),first.size())==0);
 assert(tas_dma_tap_poll(&state,ring.data(),ring.size(),480,out.data(),out.size(),&bytes,&skipped)==TAS_DMA_EMPTY);
 assert(bytes==0 && skipped==0);
 // Ring wrap: next 480 frames overlap ring physical end, preserving sample order.
 const auto second=pcm(480,480,channels);place(ring,second,480,channels);
 assert(tas_dma_tap_poll(&state,ring.data(),ring.size(),960,out.data(),out.size(),&bytes,&skipped)==TAS_DMA_OK);
 assert(std::memcmp(out.data()+40,second.data(),second.size())==0);
 assert(state.next_frame==960);
 // Producer races ahead > ring capacity. Oldest 100 frames are lost.
 const auto late=pcm(960,800,channels);place(ring,late,960,channels);
 assert(tas_dma_tap_poll(&state,ring.data(),ring.size(),1760,out.data(),out.size(),&bytes,&skipped)==TAS_DMA_OK);
 assert(skipped==100 && state.lost_frames==100);
 assert(state.next_frame==1760);
 assert(bytes==40+700*frame_bytes);
 assert(std::memcmp(out.data()+40,late.data()+100*frame_bytes,700*frame_bytes)==0);
 // Invalid counter, buffer and sample never advance state.
 const auto saved=state.next_frame;
 assert(tas_dma_tap_poll(&state,ring.data(),ring.size(),1759,out.data(),out.size(),&bytes,&skipped)==TAS_DMA_COUNTER_REWOUND);
 assert(state.next_frame==saved);
 const auto next=pcm(1760,10,channels);place(ring,next,1760,channels);
 assert(tas_dma_tap_poll(&state,ring.data(),ring.size(),1770,out.data(),40+frame_bytes-1,&bytes,&skipped)==TAS_DMA_OUTPUT_SMALL);
 assert(state.next_frame==saved);
 // NaN should be rejected without altering any cursor or publishing PCM.
 const size_t at=(1760%700)*frame_bytes;
 ring[at+2]=0xC0;ring[at+3]=0x7F; // 0x7FC00000 quiet NaN
 assert(tas_dma_tap_poll(&state,ring.data(),ring.size(),1770,out.data(),out.size(),&bytes,&skipped)==TAS_DMA_NONFINITE_SAMPLE);
 assert(state.next_frame==saved);
 place(ring,next,1760,channels);
 assert(tas_dma_tap_poll(&state,ring.data(),ring.size(),1770,out.data(),out.size(),&bytes,&skipped)==TAS_DMA_OK);
 assert(state.next_frame==1770);
}
int main(){
 verify(2,kStereo);verify(3,kTwoPointOne);
 verify(6,kFivePointOneBack);verify(6,kFivePointOneSide);verify(8,kSevenPointOne);
 tas_dma_tap bad{};
 assert(tas_dma_tap_init(&bad,7,kSevenPointOne,0,0)==TAS_DMA_BAD_FORMAT);
 assert(tas_dma_tap_init(nullptr,2,kStereo,0,0)==TAS_DMA_BAD_ARGUMENT);
 std::cout << "WaveRT DMA extractor: 2/3/6/8ch, wrap, overrun, NaN, ABI TAS1 OK\n";
}
