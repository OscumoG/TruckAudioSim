// SPDX-License-Identifier: MIT
#include "tas_ioctl_dispatch.h"
#include "driver_wire.hpp"
#include "wav.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

using namespace tas;
static StreamFormat fmt(int n){
 uint32_t mask=n==2?kStereo:n==3?kTwoPointOne:n==6?kFivePointOneSide:kSevenPointOne;
 return {static_cast<uint16_t>(n),48000,mask,kStreamFloat32,919};
}
static void oneMode(int channels){
 const auto f=fmt(channels);
 const size_t stride=TAS_FIFO_HEADER_BYTES+TAS_FIFO_MAX_FRAMES*f.channels*4u;
 std::vector<uint8_t> storage(3*stride,0), output(stride+16,0xA5);
 std::array<size_t,3> lengths{};
 tas_packet_fifo q{};
 assert(tas_packet_fifo_init(&q,storage.data(),storage.size(),stride,lengths.data(),3,
                              f.channels,f.channelMask,f.sourceEndpointToken)==TAS_FIFO_OK);
 std::vector<float> samples(96*channels);
 for(int i=0;i<96;++i)for(int c=0;c<channels;++c)
   samples[static_cast<size_t>(i)*channels+c]=float(c+1)*0.02f+float(i)*0.0001f;
 const auto packet=encodeDriverPacket({f,40,96,0},samples);
 assert(tas_packet_fifo_push(&q,packet.data(),packet.size())==TAS_FIFO_OK);
 size_t returned=333;
 // Callers cannot smuggle input, read arbitrary IOCTLs or advance on failure.
 assert(tas_ioctl_dispatch(&q,0xDEADBEEF,0,output.data(),output.size(),&returned)==TAS_IOCTL_BAD_CONTROL_CODE);
 assert(returned==0);
 assert(tas_ioctl_dispatch(&q,TAS_IOCTL_GET_PACKET,1,output.data(),output.size(),&returned)==TAS_IOCTL_INVALID_REQUEST);
 assert(tas_packet_fifo_pending(&q)==1);
 assert(tas_ioctl_dispatch(&q,TAS_IOCTL_GET_PACKET,0,output.data(),packet.size()-1,&returned)==TAS_IOCTL_BUFFER_TOO_SMALL);
 assert(returned==0 && tas_packet_fifo_pending(&q)==1);
 assert(tas_ioctl_dispatch(&q,TAS_IOCTL_GET_PACKET,0,nullptr,packet.size(),&returned)==TAS_IOCTL_BUFFER_TOO_SMALL);
 assert(tas_packet_fifo_pending(&q)==1);
 // Deliberately offset so the adapter doesn't depend on aligned payload pointers.
 assert(tas_ioctl_dispatch(&q,TAS_IOCTL_GET_PACKET,0,output.data()+1,output.size()-1,&returned)==TAS_IOCTL_SUCCESS);
 assert(returned==packet.size() && std::memcmp(output.data()+1,packet.data(),packet.size())==0);
 for(size_t i=packet.size()+1;i<output.size();++i)assert(output[i]==0xA5);
 // After a successful read, the corresponding kernel-side slot is scrubbed.
 for(size_t i=0;i<stride;++i) assert(storage[i]==0);
 auto decoded=decodeDriverPacket(std::span<const uint8_t>(output.data()+1,returned));
 assert(decoded.info.firstFrame==40 && decoded.samples==samples);
 assert(tas_ioctl_dispatch(&q,TAS_IOCTL_GET_PACKET,0,output.data(),output.size(),&returned)==TAS_IOCTL_EMPTY);
 assert(returned==0);
 assert(tas_ioctl_dispatch(&q,TAS_IOCTL_GET_PACKET,0,output.data(),output.size(),nullptr)==TAS_IOCTL_INVALID_REQUEST);
 // A cleared queue does not expose stale data.
 tas_packet_fifo_clear(&q);
 assert(tas_ioctl_dispatch(&q,TAS_IOCTL_GET_PACKET,0,output.data(),output.size(),&returned)==TAS_IOCTL_EMPTY);
}
int main(){
 oneMode(2);oneMode(3);oneMode(6);oneMode(8);
 assert(TAS_IOCTL_GET_PACKET==0x00226000u);
 std::cout<<"IOCTL simulation: 2/3/6/8 channels, buffer length, empty queue, no data disclosure OK\n";
}
