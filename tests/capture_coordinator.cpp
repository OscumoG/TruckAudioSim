// SPDX-License-Identifier: MIT
// Portable tests for the PASSIVE_LEVEL WaveRT producer state machine.
#include "tas_capture_coordinator.h"
#include "driver_wire.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>

static uint32_t maskFor(unsigned n) {
 switch(n){case 2:return 0x3u; case 3:return 0xBu;
           case 6:return 0x60Fu; case 8:return 0x63Fu;}
 return 0;
}
static float value(uint64_t frame,unsigned channel) {
 return float(int(frame%256)-128)*0.001f + float(channel)*0.00001f;
}
static void writeRing(std::vector<float>& ring, uint64_t from,uint64_t to,unsigned n) {
 const size_t ringFrames=ring.size()/n;
 for(uint64_t frame=from; frame<to; ++frame){
  for(unsigned k=0;k<n;++k)ring[(frame%ringFrames)*n+k]=value(frame,k);
 }
}
static std::vector<uint8_t> pop(tas_packet_fifo* fifo){
 std::vector<uint8_t> packet(TAS_FIFO_MAX_PACKET_BYTES);
 size_t len=0;
 assert(tas_packet_fifo_pop(fifo,packet.data(),packet.size(),&len)==TAS_FIFO_OK);
 packet.resize(len);
 return packet;
}
static void mode(unsigned n){
 const uint32_t mask=maskFor(n);
 const size_t stride=TAS_FIFO_HEADER_BYTES+TAS_FIFO_MAX_FRAMES*n*4u;
 std::vector<uint8_t> storage(stride*4,0);
 std::array<size_t,4> lengths{};
 tas_packet_fifo q{};
 assert(tas_packet_fifo_init(&q,storage.data(),storage.size(),stride,
      lengths.data(),4,(uint16_t)n,mask,314159)==TAS_FIFO_OK);
 tas_device_session s{};
 assert(tas_session_init(&s,&q)==TAS_SESSION_OK);
 tas_capture_coordinator c{};
 assert(tas_capture_init(&c,&s,(uint16_t)n,mask,314159)==TAS_CAPTURE_OK);
 // A mismatched source mask must be rejected.
 tas_capture_coordinator wrong{};
 assert(tas_capture_init(&wrong,&s,(uint16_t)n,0x3u,314159)==(n==2?TAS_CAPTURE_OK:TAS_CAPTURE_INVALID));
 std::vector<float> ring(512*n,0.0f);
 std::vector<uint8_t> staging(stride,0);
 unsigned written=99;
 uint64_t skipped=99;
 assert(tas_capture_start(&c,0)==TAS_CAPTURE_OK);
 writeRing(ring,0,100,n);
 assert(tas_capture_poll(&c,ring.data(),ring.size()*4,100,staging.data(),staging.size(),4,&written,&skipped)==TAS_CAPTURE_NO_READER);
 assert(written==0 && skipped==0 && q.write_index==q.read_index);
 // New reader cannot receive audio from before it joined.
 assert(tas_session_open(&s,101)==TAS_SESSION_OK);
 writeRing(ring,100,150,n);
 assert(tas_capture_poll(&c,ring.data(),ring.size()*4,150,staging.data(),staging.size(),4,&written,&skipped)==TAS_CAPTURE_RESET);
 assert(written==0 && tas_packet_fifo_pending(&q)==0);
 writeRing(ring,150,185,n);
 assert(tas_capture_poll(&c,ring.data(),ring.size()*4,185,staging.data(),staging.size(),4,&written,&skipped)==TAS_CAPTURE_OK);
 assert(written==1 && skipped==0 && tas_packet_fifo_pending(&q)==1);
 auto packet=pop(&q);
 const auto decoded=tas::decodeDriverPacket(packet);
 assert(decoded.info.firstFrame==150 && decoded.info.frameCount==35);
 for(unsigned i=0;i<35;++i)
  for(unsigned k=0;k<n;++k)
   assert(decoded.samples[size_t(i)*n+k]==value(150+i,k));
 // A delayed old session may not access PCM after current lease changes.
 assert(tas_session_close(&s,101)==TAS_SESSION_OK);
 writeRing(ring,185,210,n);
 assert(tas_capture_poll(&c,ring.data(),ring.size()*4,210,staging.data(),staging.size(),4,&written,&skipped)==TAS_CAPTURE_NO_READER);
 assert(tas_packet_fifo_pending(&q)==0);
 assert(tas_session_open(&s,202)==TAS_SESSION_OK);
 assert(tas_capture_poll(&c,ring.data(),ring.size()*4,210,staging.data(),staging.size(),4,&written,&skipped)==TAS_CAPTURE_RESET);
 writeRing(ring,210,235,n);
 assert(tas_capture_poll(&c,ring.data(),ring.size()*4,235,staging.data(),staging.size(),4,&written,&skipped)==TAS_CAPTURE_OK);
 auto packet2=pop(&q);
 assert(tas::decodeDriverPacket(packet2).info.firstFrame==210);
 // A device stream restart resets frame counter: do not replay obsolete DMA.
 assert(tas_capture_poll(&c,ring.data(),ring.size()*4,0,staging.data(),staging.size(),4,&written,&skipped)==TAS_CAPTURE_RESET);
 writeRing(ring,0,50,n);
 assert(tas_capture_poll(&c,ring.data(),ring.size()*4,50,staging.data(),staging.size(),4,&written,&skipped)==TAS_CAPTURE_OK);
 assert(tas::decodeDriverPacket(pop(&q)).info.firstFrame==0);
 // Long lag: skip overwritten frames, bounded production and report gaps.
 writeRing(ring,50,1600,n);
 assert(tas_capture_poll(&c,ring.data(),ring.size()*4,1600,staging.data(),staging.size(),1,&written,&skipped)==TAS_CAPTURE_OK);
 assert(written==1 && skipped==1038);
 assert(c.frames_overwritten==1038);
 auto late=tas::decodeDriverPacket(pop(&q));
 assert(late.info.firstFrame==1088 && late.info.frameCount==512);
 for(unsigned k=0;k<n;++k)assert(late.samples[k]==value(1088,k));
 // Detect nonfinite PCM and securely discard the queue.
 writeRing(ring,1600,1610,n);
 ring[(1600%512)*n]=INFINITY;
 assert(tas_capture_poll(&c,ring.data(),ring.size()*4,1610,staging.data(),staging.size(),4,&written,&skipped)==TAS_CAPTURE_BAD_PCM);
 assert(tas_packet_fifo_pending(&q)==0);
 for(uint8_t b:storage)assert(b==0);
 // The stopped stream must not enqueue PCM.
 tas_capture_stop(&c);
 assert(tas_capture_poll(&c,ring.data(),ring.size()*4,1800,staging.data(),staging.size(),4,&written,&skipped)==TAS_CAPTURE_IDLE);
 assert(written==0);
 assert(c.packets_emitted>=4 && c.reset_count>=3);
 assert(tas_session_close(&s,202)==TAS_SESSION_OK);
}
int main(){for(unsigned n:{2u,3u,6u,8u})mode(n);
 std::cout << "PASSIVE capture: source gating, session rotation, restart, ring overflow, 2/3/6/8 ch PCM fidelity OK\n";
}
