// SPDX-License-Identifier: MIT
#include "stream_protocol.hpp"
#include "wav.hpp"
#include <stdexcept>
namespace tas {
namespace {
void put16(StreamHeaderBytes& b,size_t i,uint16_t x){b[i]=uint8_t(x);b[i+1]=uint8_t(x>>8);}
void put32(StreamHeaderBytes& b,size_t i,uint32_t x){for(int j=0;j<4;++j)b[i+j]=uint8_t(x>>(8*j));}
uint16_t get16(std::span<const uint8_t> b,size_t i){return uint16_t(b[i])|uint16_t(b[i+1])<<8;}
uint32_t get32(std::span<const uint8_t> b,size_t i){uint32_t x=0;for(int j=0;j<4;++j)x|=uint32_t(b[i+j])<<(8*j);return x;}
}
PacketLengthBytes encodePacketFrames(uint32_t frames){
 if(frames>kMaxStreamPacketFrames)throw std::runtime_error("PCM chunk too large");
 return {uint8_t(frames),uint8_t(frames>>8),uint8_t(frames>>16),uint8_t(frames>>24)};
}
uint32_t decodePacketFrames(std::span<const uint8_t> bytes){
 if(bytes.size()!=4)throw std::runtime_error("Invalid PCM chunk length");
 const uint32_t frames=get32(bytes,0);
 if(frames>kMaxStreamPacketFrames)throw std::runtime_error("PCM chunk exceeds 100ms");
 return frames;
}
void validateStreamFormat(const StreamFormat& f){
 bool supported=(f.channels==2&&f.channelMask==kStereo)
  ||(f.channels==3&&f.channelMask==kTwoPointOne)
  ||(f.channels==6&&(f.channelMask==kFivePointOneSide||f.channelMask==kFivePointOneBack))
  ||(f.channels==8&&f.channelMask==kSevenPointOne);
 if(!supported||f.sampleRate!=kStreamRate||f.sampleFormat!=kStreamFloat32)
  throw std::runtime_error("Unsupported live stream: expected float32/48k with valid 2.0,2.1,5.1,7.1 mask");
}
StreamHeaderBytes encodeStreamHeader(const StreamFormat& f){
 validateStreamFormat(f);
 StreamHeaderBytes b{};b[0]='T';b[1]='A';b[2]='S';b[3]='3';
 put16(b,4,1);put16(b,6,f.channels);put32(b,8,f.sampleRate);
 put32(b,12,f.channelMask);put32(b,16,f.sampleFormat);put32(b,20,f.sourceEndpointToken);
 return b;
}
StreamFormat decodeStreamHeader(std::span<const uint8_t> b){
 if(b.size()!=kStreamHeaderSize||b[0]!='T'||b[1]!='A'||b[2]!='S'||b[3]!='3'||get16(b,4)!=1)
  throw std::runtime_error("Invalid or unsupported TruckAudioSim stream header");
 StreamFormat f{get16(b,6),get32(b,8),get32(b,12),get32(b,16),get32(b,20)};
 validateStreamFormat(f);return f;
}
}
