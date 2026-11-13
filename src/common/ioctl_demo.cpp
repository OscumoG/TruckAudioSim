// SPDX-License-Identifier: MIT
// Full, portable PCM flow: WAV -> simulated WaveRT DMA -> TAS1 FIFO ->
// METHOD_BUFFERED IOCTL dispatch -> TAS1 decoder -> WAV.
// This does NOT emulate an OS audio endpoint and does not install a driver.
#include "tas_stream_adapter.h"
#include "tas_ioctl_dispatch.h"
#include "driver_wire.hpp"
#include "wav.hpp"
#include "wav_writer.hpp"
#include <bit>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <array>
int main(int argc,char**argv){
 try {
  if(argc!=5 || std::string(argv[1])!="--input" || std::string(argv[3])!="--output"){
   std::cerr<<"Usage: tas_ioctl_demo --input source.wav --output ioctl_loopback.wav\n";
   return 2;
  }
  tas::WavReader reader(argv[2]);
  constexpr uint32_t token=0xC0FFEE;
  const tas::StreamFormat fmt{reader.channels(),reader.sampleRate(),reader.channelMask(),tas::kStreamFloat32,token};
  tas::validateStreamFormat(fmt);
  tas::WavWriter writer(argv[4],fmt);
  tas_dma_tap tap{};
  if(tas_dma_tap_init(&tap,fmt.channels,fmt.channelMask,token,0)!=TAS_DMA_OK)
   throw std::runtime_error("DMA tap initialization failed");
  constexpr size_t ringFrames=1440,blockFrames=480;
  const size_t frameBytes=size_t(fmt.channels)*sizeof(float);
  std::vector<uint8_t> dma(ringFrames*frameBytes,0);
  const size_t stride=TAS_FIFO_HEADER_BYTES+TAS_FIFO_MAX_FRAMES*frameBytes;
  std::vector<uint8_t> fifoStorage(3*stride,0),staging(stride),out(stride);
  std::array<size_t,3> lengths{};
  tas_packet_fifo fifo{};
  if(tas_packet_fifo_init(&fifo,fifoStorage.data(),fifoStorage.size(),stride,
                          lengths.data(),3,fmt.channels,fmt.channelMask,token)!=TAS_FIFO_OK)
   throw std::runtime_error("Cannot create bounded packet queue");
  uint64_t submitted=0,received=0;
  std::vector<float> input;
  while(true){
   const size_t frames=reader.readFrames(blockFrames,input);
   if(!frames)break;
   // Only the test producer writes to this completed DMA snapshot.
   for(size_t f=0;f<frames;++f){
    const size_t offset=(submitted+f)%ringFrames*frameBytes;
    for(size_t c=0;c<fmt.channels;++c){
     const uint32_t bits=std::bit_cast<uint32_t>(input[f*fmt.channels+c]);
     for(size_t i=0;i<4;++i)dma[offset+c*4+i]=uint8_t(bits>>(8*i));
    }
   }
   submitted+=frames;
   while(tap.next_frame<submitted){
    uint64_t lost=0;
    const int result=tas_stream_adapter_poll(&tap,&fifo,dma.data(),dma.size(),
                    submitted,staging.data(),staging.size(),&lost);
    if(result!=TAS_ADAPTER_OK || lost)
     throw std::runtime_error("DMA -> FIFO failed or lost frames");
   }
   while(tas_packet_fifo_pending(&fifo)){
    size_t bytes=0;
    const int result=tas_ioctl_dispatch(&fifo,TAS_IOCTL_GET_PACKET,0,
                                       out.data(),out.size(),&bytes);
    if(result!=TAS_IOCTL_SUCCESS)throw std::runtime_error("IOCTL packet read failed");
    const auto decoded=tas::decodeDriverPacket(std::span<const uint8_t>(out.data(),bytes));
    if(decoded.info.firstFrame!=received)throw std::runtime_error("Frame sequence discontinuity");
    if(decoded.info.format.channelMask!=fmt.channelMask || decoded.info.format.channels!=fmt.channels ||
       decoded.info.format.sourceEndpointToken!=token)
     throw std::runtime_error("Channel identity mismatch");
    writer.write(decoded.samples.data(),decoded.info.frameCount);
    received+=decoded.info.frameCount;
   }
  }
  writer.finalize();
  if(received!=submitted)throw std::runtime_error("Inconsistent PCM frame count");
  std::cout<<"WAV -> WaveRT ring -> FIFO -> buffered IOCTL -> WAV: "<<received
           <<" frames, "<<fmt.channels<<" channels; no EQ.\n";
 } catch(const std::exception& e){std::cerr<<"IOCTL demo: "<<e.what()<<"\n";return 1;}
 return 0;
}
