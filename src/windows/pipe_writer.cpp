// SPDX-License-Identifier: MIT
#include "pipe_writer.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>
namespace tas {
void PipeWriter::writeAll(const void* ptr,size_t bytes){
 auto* source=static_cast<const unsigned char*>(ptr);
 while(bytes){
  const auto want=static_cast<DWORD>(std::min<size_t>(bytes,65536));
  DWORD sent=0;
  if(!WriteFile(pipe_,source,want,&sent,nullptr)||!sent)throw std::runtime_error("PCM pipe write failed");
  bytes-=sent;source+=sent;
 }
}
PipeWriter::PipeWriter(StreamFormat fmt):format_(fmt){
 validateStreamFormat(fmt);
 constexpr wchar_t path[]=L"\\\\.\\pipe\\TruckAudioSimPcm";
 if(!WaitNamedPipeW(path,20000))throw std::runtime_error("Start tas_spatial --live before capture");
 pipe_=CreateFileW(path,GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(pipe_==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot connect to local PCM pipe");
 try{const auto header=encodeStreamHeader(format_);writeAll(header.data(),header.size());}
 catch(...){CloseHandle(pipe_);pipe_=INVALID_HANDLE_VALUE;throw;}
}
PipeWriter::~PipeWriter(){if(pipe_!=INVALID_HANDLE_VALUE)CloseHandle(pipe_);}
void PipeWriter::frames(const float* interleaved,uint32_t frameCount,bool silent){
 if(!interleaved && !silent && frameCount)throw std::runtime_error("Null PCM frame pointer");
 uint32_t offset=0;
 while(offset<frameCount){
  const uint32_t n=std::min<uint32_t>(frameCount-offset,kMaxStreamPacketFrames);
  std::vector<float> pcm(static_cast<size_t>(n)*format_.channels,0.0f);
  if(!silent){
   for(size_t i=0;i<pcm.size();++i){
    const float value=interleaved[static_cast<size_t>(offset)*format_.channels+i];
    pcm[i]=std::isfinite(value)?std::clamp(value,-1.0f,1.0f):0.0f;
   }
  }
  const auto packet=encodePacketFrames(n);
  writeAll(packet.data(),packet.size());
  writeAll(pcm.data(),pcm.size()*sizeof(float));
  offset+=n;
 }
}
void PipeWriter::finish(){
 if(pipe_==INVALID_HANDLE_VALUE)return;
 const auto terminator=encodePacketFrames(0);
 writeAll(terminator.data(),terminator.size());
 FlushFileBuffers(pipe_);
}
}
