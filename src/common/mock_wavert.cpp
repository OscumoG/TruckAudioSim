// SPDX-License-Identifier: MIT
// Demonstrates a render->loopback path using ORIGINAL PCM (no synthesized tone).
// Important: THIS IS USER-MODE EMULATION, not an actual Windows audio device.
#include "driver_wire.hpp"
#include "wav.hpp"
#include "wav_writer.hpp"
#include "wavert_model.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
int main(int argc,char**argv){
 try{
  if(argc!=5||std::string(argv[1])!="--input"||std::string(argv[3])!="--output"){
   std::cerr<<"Usage: tas_mock_wavert --input source.wav --output loopback.wav\n"
     "User-mode simulation only: does not install a Windows audio device.\n";
   return 2;
  }
  tas::WavReader reader(argv[2]);
  tas::StreamFormat fmt{reader.channels(),reader.sampleRate(),reader.channelMask(),tas::kStreamFloat32,0x1001};
  tas::validateStreamFormat(fmt);
  tas::WavWriter writer(argv[4],fmt);
  tas::WaveRtModel device(fmt,8192);
  constexpr uint32_t kBlock=480;
  uint64_t cursor=0;
  std::vector<float> source,captured(kBlock*fmt.channels);
  while(true){
   const size_t frames=reader.readFrames(kBlock,source);
   if(!frames)break;
   tas::DriverPacketInfo info{fmt,cursor,static_cast<uint32_t>(frames),0};
   const auto serialized=tas::encodeDriverPacket(info,std::span<const float>(source.data(),frames*fmt.channels));
   const auto packet=tas::decodeDriverPacket(serialized);
   device.submit(packet.info,packet.samples);
   const auto result=device.capture(cursor,frames,
            std::span<float>(captured.data(),frames*fmt.channels));
   if(result.validFrames!=frames||result.overwrittenFrames||result.notYetAvailableFrames)
    throw std::runtime_error("Mock WaveRT PCM was lost");
   writer.write(captured.data(),frames);
   cursor+=frames;
  }
  writer.finalize();
  std::cout<<"Mock WaveRT loopback: "<<cursor<<" frames, "<<fmt.channels
           <<" channels, 48 kHz, channel mask 0x"<<std::hex<<fmt.channelMask
           <<std::dec<<", no added EQ/tones\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<"tas_mock_wavert: "<<e.what()<<"\n";return 1;}
}
