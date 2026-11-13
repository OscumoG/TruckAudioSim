// SPDX-License-Identifier: MIT
#include "wav_writer.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>
namespace tas {
namespace {
void le16(std::ostream& o,uint16_t v){char b[2]{char(v&255),char(v>>8)};o.write(b,2);}
void le32(std::ostream& o,uint32_t v){char b[4]{char(v&255),char((v>>8)&255),char((v>>16)&255),char((v>>24)&255)};o.write(b,4);}
}
WavWriter::WavWriter(const std::string& name,const StreamFormat& fmt):out_(name,std::ios::binary),format_(fmt){
 validateStreamFormat(fmt);
 if(!out_)throw std::runtime_error("Cannot write diagnostic WAV: "+name);
 out_.write("RIFF",4);le32(out_,0);out_.write("WAVE",4);
 out_.write("fmt ",4);le32(out_,40);
 le16(out_,0xfffe);le16(out_,fmt.channels);le32(out_,fmt.sampleRate);
 le32(out_,fmt.sampleRate*fmt.channels*4);le16(out_,fmt.channels*4);le16(out_,32);
 le16(out_,22);le16(out_,32);le32(out_,fmt.channelMask);
 le32(out_,3); // KSDATAFORMAT_SUBTYPE_IEEE_FLOAT GUID
 constexpr unsigned char tail[]={0x00,0x00,0x10,0x00,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71};
 out_.write(reinterpret_cast<const char*>(tail),sizeof(tail));
 out_.write("data",4);le32(out_,0);
 if(!out_)throw std::runtime_error("Cannot write WAV header");
}
void WavWriter::write(const float* samples,size_t frames){
 if(closed_)throw std::runtime_error("WAV already finalized");
 if(frames==0)return;
 if(!samples)throw std::invalid_argument("Null samples");
 constexpr uint64_t maxBytes=uint64_t(UINT32_MAX)-72;
 const uint64_t totalBytes=(framesWritten_+frames)*format_.channels*4ull;
 if(totalBytes>maxBytes)throw std::runtime_error("WAV too long for RIFF; stop recording sooner");
 // Encode a full block before writing: one disk write avoids per-sample I/O stalls.
 // Little-endian PCM float32 regardless of host endianness.
 std::vector<char> block(frames*format_.channels*4);
 for(size_t n=0;n<frames*format_.channels;++n){
  float sample=std::isfinite(samples[n])?std::clamp(samples[n],-1.0f,1.0f):0.0f;
  uint32_t bits=0;std::memcpy(&bits,&sample,4);
  for(size_t byte=0;byte<4;++byte)block[4*n+byte]=char((bits>>(8*byte))&255);
 }
 out_.write(block.data(),static_cast<std::streamsize>(block.size()));
 if(!out_)throw std::runtime_error("Error while writing WAV audio");
 framesWritten_+=frames;
}
void WavWriter::finalize(){
 if(closed_)return;
 const auto dataSize=static_cast<uint32_t>(framesWritten_*format_.channels*4ull);
 // Header layout: RIFF(12), fmt chunk(8+40)=60, data tag(4)=64, size at 64, samples start at 68.
 // Correct RIFF total size = dataSize + 60, data-size offset = 64.
 out_.seekp(4);le32(out_,dataSize+60);
 out_.seekp(64);le32(out_,dataSize);
 out_.flush();
 if(!out_)throw std::runtime_error("Error finalizing WAV");
 out_.close();closed_=true;
}
WavWriter::~WavWriter(){if(!closed_){try{finalize();}catch(...){}}}
}
