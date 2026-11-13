// SPDX-License-Identifier: MIT
// Real runnable C boundary tests; no Windows driver is installed.
#include "tas_device_session.h"
#include "driver_wire.hpp"
#include "wav.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

static tas::StreamFormat format(int channels) {
 uint32_t mask=channels==2?tas::kStereo:channels==3?tas::kTwoPointOne:
               channels==6?tas::kFivePointOneSide:tas::kSevenPointOne;
 return {static_cast<uint16_t>(channels),48000,mask,tas::kStreamFloat32,12345};
}
static void test_mode(int channels) {
 const auto f=format(channels);
 const size_t stride=TAS_FIFO_HEADER_BYTES+TAS_FIFO_MAX_FRAMES*f.channels*4u;
 std::vector<uint8_t> data(4*stride,0);
 std::array<size_t,4> lengths{};
 tas_packet_fifo q{};
 assert(tas_packet_fifo_init(&q,data.data(),data.size(),stride,lengths.data(),4,
                             f.channels,f.channelMask,f.sourceEndpointToken)==TAS_FIFO_OK);
 tas_device_session s{};
 assert(tas_session_init(&s,&q)==TAS_SESSION_OK);
 std::vector<float> samples(128*channels);
 for(size_t i=0;i<samples.size();++i)samples[i]=float(i%channels+1)*0.01f;
 auto pkt=tas::encodeDriverPacket({f,100,128,0},samples);
 std::vector<uint8_t> out(stride,0xE5);
 size_t received=999;
 // Access before open denied; no retained PCM when nobody is listening.
 assert(tas_session_push(&s,pkt.data(),pkt.size())==TAS_SESSION_DENIED);
 assert(tas_session_read(&s,111,TAS_IOCTL_GET_PACKET,0,out.data(),out.size(),&received)==TAS_SESSION_DENIED);
 assert(received==0);
 assert(tas_session_open(&s,111)==TAS_SESSION_OK);
 const auto generation=s.generation;
 assert(generation!=0);
 assert(tas_session_open(&s,111)==TAS_SESSION_BUSY); // even same handle not re-opened
 assert(tas_session_open(&s,222)==TAS_SESSION_BUSY);
 assert(tas_session_push(&s,pkt.data(),pkt.size())==TAS_SESSION_OK);
 assert(tas_packet_fifo_pending(&q)==1);
 assert(tas_session_read(&s,222,TAS_IOCTL_GET_PACKET,0,out.data(),out.size(),&received)==TAS_SESSION_DENIED);
 assert(received==0 && tas_packet_fifo_pending(&q)==1);
 assert(tas_session_close(&s,222)==TAS_SESSION_DENIED);
 assert(tas_packet_fifo_pending(&q)==1);
 assert(tas_session_read(&s,111,0xffffffff,0,out.data(),out.size(),&received)==TAS_SESSION_BAD_CONTROL);
 assert(received==0 && tas_packet_fifo_pending(&q)==1);
 assert(tas_session_read(&s,111,TAS_IOCTL_GET_PACKET,1,out.data(),out.size(),&received)==TAS_SESSION_BAD_ARGUMENT);
 assert(received==0 && tas_packet_fifo_pending(&q)==1);
 assert(tas_session_read(&s,111,TAS_IOCTL_GET_PACKET,0,out.data(),pkt.size()-1,&received)==TAS_SESSION_BUFFER_SMALL);
 assert(received==0 && tas_packet_fifo_pending(&q)==1);
 assert(tas_session_read(&s,111,TAS_IOCTL_GET_PACKET,0,out.data(),out.size(),&received)==TAS_SESSION_OK);
 assert(received==pkt.size() && std::memcmp(out.data(),pkt.data(),received)==0);
 assert(tas_packet_fifo_pending(&q)==0);
 auto parsed=tas::decodeDriverPacket(std::span<const uint8_t>(out.data(),received));
 assert(parsed.samples==samples);
 assert(tas_session_read(&s,111,TAS_IOCTL_GET_PACKET,0,out.data(),out.size(),&received)==TAS_SESSION_EMPTY);
 assert(received==0);
 // Open session, enqueue different PCM and verify cleanup zeroizes all slots.
 auto second=tas::encodeDriverPacket({f,228,128,0},samples);
 assert(tas_session_push(&s,second.data(),second.size())==TAS_SESSION_OK);
 assert(tas_session_close(&s,111)==TAS_SESSION_OK);
 assert(tas_session_close(&s,111)==TAS_SESSION_OK); // WDM CLEANUP -> CLOSE
 assert(tas_packet_fifo_pending(&q)==0);
 for(uint8_t b:data)assert(b==0);
 assert(tas_session_push(&s,pkt.data(),pkt.size())==TAS_SESSION_DENIED);
 assert(tas_session_open(&s,222)==TAS_SESSION_OK);
 // A delayed IRP_MJ_CLOSE for the first handle must NOT revoke the new lease.
 assert(tas_session_close(&s,111)==TAS_SESSION_DENIED);
 assert(s.reader_active && s.reader_key==222);
 assert(s.generation==generation+1);
 assert(tas_session_read(&s,222,TAS_IOCTL_GET_PACKET,0,out.data(),out.size(),&received)==TAS_SESSION_EMPTY);
 // No audio from the previous client's session is exposed.
 assert(received==0);
 assert(tas_session_push(&s,pkt.data(),pkt.size())==TAS_SESSION_OK);
 tas_session_shutdown(&s);
 assert(tas_packet_fifo_pending(&q)==0);
 assert(tas_session_read(&s,222,TAS_IOCTL_GET_PACKET,0,out.data(),out.size(),&received)==TAS_SESSION_DENIED);
 for(uint8_t b:data)assert(b==0);
}
int main(){
 test_mode(2);test_mode(3);test_mode(6);test_mode(8);
 std::cout << "Driver control session: 2/3/6/8 channels, single-reader lease, cleanup, scrub, IOCTL guard OK\n";
}
