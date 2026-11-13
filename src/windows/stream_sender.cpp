// SPDX-License-Identifier: MIT
// Streams a WAV through the same IPC path a future virtual audio driver will use.
// This tool does not capture Spotify and does not install drivers.
#include <windows.h>
#include "wav.hpp"
#include "stream_protocol.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
namespace {
void writeAll(HANDLE pipe,const void* data,size_t bytes){
 auto p=static_cast<const unsigned char*>(data);
 while(bytes){
  DWORD n=0;
  if(!WriteFile(pipe,p,static_cast<DWORD>(std::min<size_t>(bytes,65536)),&n,nullptr)||!n)
    throw std::runtime_error("WriteFile to audio pipe failed");
  p+=n;bytes-=n;
 }
}
void countPacket(HANDLE pipe,uint32_t frames){
 const auto packet=tas::encodePacketFrames(frames);
 writeAll(pipe,packet.data(),packet.size());
}
std::wstring pipePath(const std::wstring& name){
 if(name.empty()||name.size()>80)throw std::runtime_error("Invalid pipe name");
 for(auto c:name)if(!((c>=L'A'&&c<=L'Z')||(c>=L'a'&&c<=L'z')||
                        (c>=L'0'&&c<=L'9')||c==L'_'||c==L'-'))throw std::runtime_error("Invalid pipe name");
 return L"\\\\.\\pipe\\"+name;
}
}
int main(int argc,char** argv){
 try{
  if(argc!=3||std::string(argv[1])!="--wav"){
   std::cout<<"Usage: tas_stream_sender.exe --wav file_48k.wav\n"
               "First start tas_spatial.exe --preset profile.json --live\n";
   return 1;
  }
  tas::WavReader reader(argv[2]);
  tas::StreamFormat fmt{reader.channels(),reader.sampleRate(),reader.channelMask(),tas::kStreamFloat32};
  auto wire=tas::encodeStreamHeader(fmt);
  const auto path=pipePath(L"TruckAudioSimPcm");
  if(!WaitNamedPipeW(path.c_str(),30000))throw std::runtime_error("Audio pipe unavailable (start tas_spatial --live first)");
  HANDLE pipe=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(pipe==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot connect to audio pipe");
  try{
   writeAll(pipe,wire.data(),wire.size());
   const auto begin=std::chrono::steady_clock::now();
   uint64_t total=0;
   for(;;){
    std::vector<float> block;
    const size_t frames=reader.readFrames(480,block);
    if(!frames)break;
    countPacket(pipe,static_cast<uint32_t>(frames));
    writeAll(pipe,block.data(),frames*fmt.channels*sizeof(float));
    total+=frames;
    std::this_thread::sleep_until(begin+std::chrono::nanoseconds((total*1000000000ULL)/fmt.sampleRate));
   }
   countPacket(pipe,0);
   FlushFileBuffers(pipe);
   std::cout<<"Sent "<<total<<" frames.\n";
  }catch(...){CloseHandle(pipe);throw;}
  CloseHandle(pipe);
  return 0;
 }catch(const std::exception& e){std::cerr<<"Sender error: "<<e.what()<<"\n";return 1;}
}
