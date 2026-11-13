// SPDX-License-Identifier: MIT
#include "driver_wire.hpp"
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tas {
namespace {
void put16(std::span<uint8_t> b,size_t pos,uint16_t v){
 b[pos]=uint8_t(v);b[pos+1]=uint8_t(v>>8);
}
void put32(std::span<uint8_t> b,size_t pos,uint32_t v){
 for(size_t i=0;i<4;++i)b[pos+i]=uint8_t(v>>(i*8));
}
void put64(std::span<uint8_t> b,size_t pos,uint64_t v){
 for(size_t i=0;i<8;++i)b[pos+i]=uint8_t(v>>(i*8));
}
uint16_t get16(std::span<const uint8_t> b,size_t pos){
 return uint16_t(uint16_t(b[pos])|uint16_t(uint16_t(b[pos+1])<<8));
}
uint32_t get32(std::span<const uint8_t> b,size_t pos){
 uint32_t v=0;for(size_t i=0;i<4;++i)v|=uint32_t(b[pos+i])<<(i*8);return v;
}
uint64_t get64(std::span<const uint8_t> b,size_t pos){
 uint64_t v=0;for(size_t i=0;i<8;++i)v|=uint64_t(b[pos+i])<<(i*8);return v;
}
void validateInfo(const DriverPacketInfo& info){
 validateStreamFormat(info.format);
 if(info.format.sampleFormat!=kStreamFloat32 || info.format.sampleRate!=48000)
  throw std::invalid_argument("Driver packet requires 48k float32 PCM");
 if(info.frameCount==0||info.frameCount>kMaxDriverPacketFrames)
  throw std::invalid_argument("Invalid driver packet length");
 if(info.flags!=0)throw std::invalid_argument("Unsupported driver packet flags");
 if(info.firstFrame>UINT64_MAX-info.frameCount)
  throw std::invalid_argument("Driver frame counter overflow");
}
}
DriverHeaderBytes encodeDriverHeader(const DriverPacketInfo& info){
 validateInfo(info);
 DriverHeaderBytes bytes{};
 put32(bytes,0,kDriverPacketMagic);
 put16(bytes,4,kDriverPacketVersion);
 put16(bytes,6,static_cast<uint16_t>(kDriverHeaderBytes));
 put32(bytes,8,info.format.sampleRate);
 put32(bytes,12,info.format.channelMask);
 put64(bytes,16,info.firstFrame);
 put32(bytes,24,info.frameCount);
 put16(bytes,28,info.format.channels);
 put16(bytes,30,static_cast<uint16_t>(kDriverPacketFloat32));
 put32(bytes,32,info.format.sourceEndpointToken);
 put32(bytes,36,info.flags);
 return bytes;
}
DriverPacketInfo decodeDriverHeader(std::span<const uint8_t> bytes){
 if(bytes.size()<kDriverHeaderBytes)throw std::invalid_argument("Truncated driver header");
 if(get32(bytes,0)!=kDriverPacketMagic || get16(bytes,4)!=kDriverPacketVersion ||
    get16(bytes,6)!=kDriverHeaderBytes)
  throw std::invalid_argument("Unknown driver packet header/version");
 DriverPacketInfo info;
 info.format.sampleRate=get32(bytes,8);
 info.format.channelMask=get32(bytes,12);
 info.firstFrame=get64(bytes,16);
 info.frameCount=get32(bytes,24);
 info.format.channels=get16(bytes,28);
 info.format.sampleFormat=get16(bytes,30);
 info.format.sourceEndpointToken=get32(bytes,32);
 info.flags=get32(bytes,36);
 validateInfo(info);
 return info;
}
std::vector<uint8_t> encodeDriverPacket(const DriverPacketInfo& info,
                                         std::span<const float> samples){
 const auto header=encodeDriverHeader(info);
 const size_t count=size_t(info.frameCount)*info.format.channels;
 if(samples.size()!=count)throw std::invalid_argument("Driver PCM sample count mismatch");
 std::vector<uint8_t> bytes(kDriverHeaderBytes+count*sizeof(float));
 for(size_t i=0;i<kDriverHeaderBytes;++i)bytes[i]=header[i];
 for(size_t i=0;i<count;++i){
  if(!std::isfinite(samples[i]))throw std::invalid_argument("Non-finite driver PCM sample");
  const auto bits=std::bit_cast<uint32_t>(samples[i]);
  put32(bytes,kDriverHeaderBytes+i*4,bits);
 }
 return bytes;
}
DecodedDriverPacket decodeDriverPacket(std::span<const uint8_t> bytes){
 const auto info=decodeDriverHeader(bytes);
 const size_t count=size_t(info.frameCount)*info.format.channels;
 if(bytes.size()!=kDriverHeaderBytes+count*4)
  throw std::invalid_argument("Driver PCM packet payload length mismatch");
 DecodedDriverPacket decoded{info,std::vector<float>(count)};
 for(size_t i=0;i<count;++i){
  const auto sample=std::bit_cast<float>(get32(bytes,kDriverHeaderBytes+i*4));
  if(!std::isfinite(sample))throw std::invalid_argument("Non-finite driver PCM payload");
  decoded.samples[i]=sample;
 }
 return decoded;
}
} // namespace tas
