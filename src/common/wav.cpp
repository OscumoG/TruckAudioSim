// SPDX-License-Identifier: MIT
#include "wav.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <vector>
namespace tas {
namespace {
uint16_t read16(std::istream& f){unsigned char b[2]{};f.read((char*)b,2);if(!f)throw std::runtime_error("Truncated WAV header");return uint16_t(b[0]|(uint16_t(b[1])<<8));}
uint32_t read32(std::istream& f){unsigned char b[4]{};f.read((char*)b,4);if(!f)throw std::runtime_error("Truncated WAV header");return uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24);}
bool idIs(const char* id,const char* v){return std::memcmp(id,v,4)==0;}
// GUID tail from KSDATAFORMAT_SUBTYPE_PCM / IEEE_FLOAT.
constexpr unsigned char subtypeTail[12]={0x00,0x00,0x10,0x00,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71};
}
WavReader::WavReader(const std::string& path):in_(std::filesystem::path(std::u8string(path.begin(),path.end())),std::ios::binary){
 if(!in_)throw std::runtime_error("Cannot open WAV: "+path);
 char id[4]{};in_.read(id,4);if(!in_||!idIs(id,"RIFF"))throw std::runtime_error("Only RIFF WAV is supported (not RF64)");
 (void)read32(in_);in_.read(id,4);if(!in_||!idIs(id,"WAVE"))throw std::runtime_error("Not a WAVE file");
 bool fmt=false,data=false;
 while(in_){
  in_.read(id,4);if(!in_)break;
  const uint32_t sz=read32(in_);
  const auto start=in_.tellg();
  if(idIs(id,"fmt ")){
   if(fmt || sz<16 || sz>1024)throw std::runtime_error("Invalid WAV fmt chunk");
   const uint16_t tag=read16(in_);
   channels_=read16(in_);sampleRate_=read32(in_);
   const uint32_t averageRate=read32(in_);
   const uint16_t blockAlign=read16(in_);
   bits_=read16(in_);
   if(tag==0xfffe){ // WAVE_FORMAT_EXTENSIBLE
    if(sz<40)throw std::runtime_error("Incomplete WAVEFORMATEXTENSIBLE");
    const uint16_t cb=read16(in_),validBits=read16(in_);
    channelMask_=read32(in_);
    const uint32_t subtype=read32(in_);
    unsigned char tail[12]{};in_.read(reinterpret_cast<char*>(tail),12);
    if(!in_||cb<22||std::memcmp(tail,subtypeTail,12)!=0 || (subtype!=1&&subtype!=3))
     throw std::runtime_error("Unsupported extensible WAV subtype");
    if(validBits!=bits_)throw std::runtime_error("WAV valid bits must equal container bits");
    format_=static_cast<uint16_t>(subtype);
   }else{
    format_=tag;
    channelMask_=(channels_==2)?kStereo:0;
   }
   if(sampleRate_!=48000 || !((format_==1&&bits_==16)||(format_==3&&bits_==32)))
    throw std::runtime_error("Expected 48 kHz PCM16 or float32 WAV");
   const bool ok=(channels_==2&&channelMask_==kStereo)||
    (channels_==3&&channelMask_==kTwoPointOne)||
    (channels_==6&&(channelMask_==kFivePointOneBack||channelMask_==kFivePointOneSide))||
    (channels_==8&&channelMask_==kSevenPointOne);
   if(!ok)throw std::runtime_error("Expected stereo, 2.1, 5.1 or 7.1 with valid channel mask");
   if(blockAlign!=channels_*(bits_/8)||averageRate!=uint32_t(blockAlign)*sampleRate_)
    throw std::runtime_error("WAV blockAlign/byteRate mismatch");
   fmt=true;
  }else if(idIs(id,"data")){
   if(!fmt)throw std::runtime_error("WAV fmt must precede data");
   if(sz % (channels_*(bits_/8)) != 0)
    throw std::runtime_error("WAV data size does not contain whole PCM frames");
   dataRemaining_=sz;data=true;break;
  }
  in_.seekg(start+static_cast<std::streamoff>(sz)+static_cast<std::streamoff>(sz&1));
  if(!in_)throw std::runtime_error("Truncated WAV chunk");
 }
 if(!fmt||!data)throw std::runtime_error("WAV needs fmt and data chunks");
}
size_t WavReader::readFrames(size_t frames,std::vector<float>& dest){
 if(frames>48000*10)throw std::runtime_error("WAV read block too large");
 const size_t bytesPerFrame=channels_*(bits_/8);
 const size_t count=std::min(frames,size_t(dataRemaining_)/bytesPerFrame);
 std::vector<unsigned char> bytes(count*bytesPerFrame);
 if(!bytes.empty() && !in_.read(reinterpret_cast<char*>(bytes.data()),
                              static_cast<std::streamsize>(bytes.size())))
  throw std::runtime_error("Truncated WAV PCM data");
 dataRemaining_-=static_cast<uint32_t>(bytes.size());
 dest.assign(frames*channels_,0.0f);
 for(size_t i=0;i<count*channels_;++i){
  if(format_==1){const uint16_t u=uint16_t(bytes[2*i])|(uint16_t(bytes[2*i+1])<<8);dest[i]=float(static_cast<int16_t>(u))/32768.0f;}
  else{float v=0;std::memcpy(&v,bytes.data()+4*i,4);dest[i]=std::isfinite(v)?std::clamp(v,-1.0f,1.0f):0.0f;}
 }
 return count;
}
}
