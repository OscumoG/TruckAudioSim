// SPDX-License-Identifier: MIT
// Exercises a realistic DMA ring readback path entirely in user mode.
// The ring writer in this demonstration stands in for the Windows audio engine.
// The portable C DMA tap can later be adapted into a separately validated WDK driver.
#include "wavert_dma_tap.h"
#include "driver_wire.hpp"
#include "wav.hpp"
#include "wav_writer.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
int main(int argc,char**argv){
 try {
  if(argc!=5 || std::string(argv[1])!="--input" || std::string(argv[3])!="--output"){
   std::cerr<<"Usage: tas_dma_tap_demo --input source.wav --output tap_loopback.wav\n"
            <<"No driver installed: test DMA producer is simulated in user mode.\n";
   return 2;
  }
  tas::WavReader reader(argv[2]);
  tas::StreamFormat fmt{reader.channels(),reader.sampleRate(),reader.channelMask(),tas::kStreamFloat32,0xC0FFEE};
  tas::validateStreamFormat(fmt);
  tas::WavWriter writer(argv[4],fmt);
  tas_dma_tap tap{};
  if(tas_dma_tap_init(&tap,fmt.channels,fmt.channelMask,fmt.sourceEndpointToken,0)!=TAS_DMA_OK)
   throw std::runtime_error("DMA tap init failed");
  constexpr size_t kRingFrames=1440,kBlock=480;
  const size_t frameBytes=size_t(fmt.channels)*sizeof(float);
  std::vector<uint8_t> dma(kRingFrames*frameBytes,0);
  std::vector<uint8_t> packet(40+960*frameBytes);
  uint64_t submitted=0;
  std::vector<float> input;
  while(true){
   const size_t frames=reader.readFrames(kBlock,input);
   if(!frames)break;
   // Emulate the OS advancing the completed WaveRT buffer, not a second EQ.
   for(size_t f=0;f<frames;++f){
    const size_t offset=(submitted+f)%kRingFrames*frameBytes;
    for(size_t c=0;c<fmt.channels;++c){
     const uint32_t bits=std::bit_cast<uint32_t>(input[f*fmt.channels+c]);
     for(size_t i=0;i<4;++i)dma[offset+c*4+i]=uint8_t(bits>>(8*i));
    }
   }
   submitted+=frames;
   while(tap.next_frame<submitted){
    size_t bytes=0;uint64_t dropped=0;
    const int status=tas_dma_tap_poll(&tap,dma.data(),dma.size(),submitted,
                           packet.data(),packet.size(),&bytes,&dropped);
    if(status!=TAS_DMA_OK||dropped)throw std::runtime_error("DMA tap lost PCM in demo");
    const auto decoded=tas::decodeDriverPacket(std::span<const uint8_t>(packet.data(),bytes));
    writer.write(decoded.samples.data(),decoded.info.frameCount);
   }
  }
  writer.finalize();
  std::cout<<"Portable DMA->TAS1->WAV: "<<submitted<<" frames, "<<fmt.channels
           <<" channels, untouched PCM bits (after WAV input conversion).\n";
 }catch(const std::exception& e){std::cerr<<"DMA tap demo: "<<e.what()<<"\n";return 1;}
 return 0;
}
