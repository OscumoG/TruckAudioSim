// SPDX-License-Identifier: MIT
#include "tas_packet_fifo.h"
#include "wavert_dma_tap.h"
#include "driver_wire.hpp"
#include "wav.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

using namespace tas;

static StreamFormat format(uint16_t n) {
 uint32_t mask=n==2?kStereo:n==3?kTwoPointOne:n==6?kFivePointOneSide:kSevenPointOne;
 return {n,48000,mask,kStreamFloat32,77};
}
static std::vector<uint8_t> makePacket(StreamFormat fmt,uint64_t first,uint32_t frames){
 std::vector<float> samples((size_t)frames*fmt.channels);
 for(uint32_t f=0;f<frames;++f)for(uint32_t c=0;c<fmt.channels;++c)
  samples[(size_t)f*fmt.channels+c]=float((first+f)%2000)/4000.0f+float(c)/20.0f;
 return encodeDriverPacket({fmt,first,frames,0},samples);
}
static void test_format(uint16_t channels){
 const auto fmt=format(channels);
 const size_t stride=TAS_FIFO_HEADER_BYTES+TAS_FIFO_MAX_FRAMES*channels*4u;
 std::vector<uint8_t> data(3*stride,0);
 std::vector<size_t> lengths(3,0);
 tas_packet_fifo q{};
 assert(tas_packet_fifo_init(&q,data.data(),data.size(),stride,lengths.data(),3,
                             fmt.channels,fmt.channelMask,fmt.sourceEndpointToken)==TAS_FIFO_OK);
 assert(tas_packet_fifo_pending(&q)==0);
 auto p1=makePacket(fmt,0,480),p2=makePacket(fmt,480,480),p3=makePacket(fmt,960,480);
 auto p4=makePacket(fmt,1440,480);
 assert(tas_packet_fifo_push(&q,p1.data(),p1.size())==TAS_FIFO_OK);
 assert(tas_packet_fifo_push(&q,p2.data(),p2.size())==TAS_FIFO_OK);
 assert(tas_packet_fifo_push(&q,p3.data(),p3.size())==TAS_FIFO_OK);
 assert(tas_packet_fifo_pending(&q)==3);
 std::vector<uint8_t> out(stride,0);size_t written=999;
 assert(tas_packet_fifo_pop(&q,out.data(),10,&written)==TAS_FIFO_OUTPUT_SMALL);
 assert(written==0 && tas_packet_fifo_pending(&q)==3);
 // Realtime FIFO discards the oldest packet when the producer runs ahead.
 assert(tas_packet_fifo_push(&q,p4.data(),p4.size())==TAS_FIFO_OK);
 assert(q.dropped_packets==1 && tas_packet_fifo_pending(&q)==3);
 for(const auto& expected: {p2,p3,p4}){
  assert(tas_packet_fifo_pop(&q,out.data(),out.size(),&written)==TAS_FIFO_OK);
  assert(written==expected.size());
  assert(std::memcmp(out.data(),expected.data(),written)==0);
  auto decoded=decodeDriverPacket(std::span<const uint8_t>(out.data(),written));
  assert(decoded.info.format.channels==channels);
 }
 assert(tas_packet_fifo_pop(&q,out.data(),out.size(),&written)==TAS_FIFO_EMPTY);
 // Valid gaps must be allowed; overlapping frames never may be admitted.
 const auto late=makePacket(fmt,3500,20);
 assert(tas_packet_fifo_push(&q,late.data(),late.size())==TAS_FIFO_OK);
 const auto overlap=makePacket(fmt,3510,20);
 assert(tas_packet_fifo_push(&q,overlap.data(),overlap.size())==TAS_FIFO_OUT_OF_ORDER);
 auto bad=makePacket(fmt,3520,20);
 bad[0]=0x00;assert(tas_packet_fifo_push(&q,bad.data(),bad.size())==TAS_FIFO_BAD_FORMAT);
 bad=makePacket(fmt,3520,20);bad[32]^=0x01;
 assert(tas_packet_fifo_push(&q,bad.data(),bad.size())==TAS_FIFO_BAD_FORMAT);
 bad=makePacket(fmt,3520,20);bad[40]=0;bad[41]=0;bad[42]=0xC0;bad[43]=0x7F;
 assert(tas_packet_fifo_push(&q,bad.data(),bad.size())==TAS_FIFO_BAD_FORMAT);
 assert(tas_packet_fifo_push(&q,p4.data(),p4.size()-1)==TAS_FIFO_BAD_FORMAT);
 assert(tas_packet_fifo_pending(&q)==1);
 tas_packet_fifo_clear(&q);
 assert(q.dropped_packets==0 && tas_packet_fifo_pending(&q)==0);
 assert(tas_packet_fifo_push(&q,p1.data(),p1.size())==TAS_FIFO_OK);
}
static void test_integration(){
 constexpr uint16_t channels=8;
 const auto fmt=format(channels);
 const size_t frame_bytes=channels*4u;
 constexpr uint64_t frames=480;
 std::vector<uint8_t> ring(frames*frame_bytes);
 auto samples=makePacket(fmt,0,480);
 std::memcpy(ring.data(),samples.data()+40,ring.size());
 tas_dma_tap tap{};
 assert(tas_dma_tap_init(&tap,channels,fmt.channelMask,fmt.sourceEndpointToken,0)==TAS_DMA_OK);
 std::array<uint8_t,TAS_FIFO_MAX_PACKET_BYTES> packet{};
 size_t count=0;uint64_t skipped=0;
 assert(tas_dma_tap_poll(&tap,ring.data(),ring.size(),480,packet.data(),packet.size(),&count,&skipped)==TAS_DMA_OK);
 const size_t stride=40+960*frame_bytes;
 std::vector<uint8_t> data(2*stride),out(stride);
 std::vector<size_t> lengths(2);
 tas_packet_fifo q{};
 assert(tas_packet_fifo_init(&q,data.data(),data.size(),stride,lengths.data(),2,
                             channels,fmt.channelMask,fmt.sourceEndpointToken)==TAS_FIFO_OK);
 assert(tas_packet_fifo_push(&q,packet.data(),count)==TAS_FIFO_OK);
 size_t got=0;
 assert(tas_packet_fifo_pop(&q,out.data(),out.size(),&got)==TAS_FIFO_OK);
 assert(got==count && std::memcmp(packet.data(),out.data(),got)==0);
 const auto decoded=decodeDriverPacket(std::span<const uint8_t>(out.data(),got));
 assert(decoded.info.frameCount==480 && decoded.samples.size()==480*8);
}
static void test_guards(){
 tas_packet_fifo q{};
 std::array<uint8_t,80000> bytes{};
 std::array<size_t,2> lengths{};
 assert(tas_packet_fifo_init(&q,bytes.data(),bytes.size(),30000,lengths.data(),2,8,kSevenPointOne,77)==TAS_FIFO_BAD_ARGUMENT);
 assert(tas_packet_fifo_init(&q,bytes.data(),bytes.size(),30760,lengths.data(),2,8,kSevenPointOne,77)==TAS_FIFO_OK);
 assert(tas_packet_fifo_push(&q,nullptr,0)==TAS_FIFO_BAD_ARGUMENT);
 assert(tas_packet_fifo_pending(&q)==0);
}
int main(){
 test_format(2);test_format(3);test_format(6);test_format(8);
 test_integration();test_guards();
 std::cout<<"Packet FIFO: 2.0/2.1/5.1/7.1, strict TAS1, drop-oldest, exact PCM OK\n";
}
