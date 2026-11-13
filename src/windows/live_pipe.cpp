// SPDX-License-Identifier: MIT
#include "live_pipe.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace tas {
namespace {
std::wstring pipePath(const std::wstring& name){
 if(name.empty()||name.size()>80)throw std::runtime_error("Invalid pipe name");
 for(auto c:name)if(!((c>=L'A'&&c<=L'Z')||(c>=L'a'&&c<=L'z')||
                        (c>=L'0'&&c<=L'9')||c==L'_'||c==L'-'))
  throw std::runtime_error("Pipe name must contain ASCII letters, digits, - or _ only");
 return L"\\\\.\\pipe\\"+name;
}
void readAll(HANDLE pipe,void* data,size_t n){
 auto* dst=static_cast<unsigned char*>(data);
 while(n){
  DWORD amount=0;
  if(!ReadFile(pipe,dst,static_cast<DWORD>(std::min<size_t>(n,65536)),&amount,nullptr)||amount==0)
   throw std::runtime_error("Stream ended early or pipe ReadFile failed");
  n-=amount;dst+=amount;
 }
}
}
LivePipeReceiver::~LivePipeReceiver(){
 cancel_.store(true,std::memory_order_release);
 if(worker_.joinable()){
  CancelSynchronousIo(worker_.native_handle());
  worker_.join();
 }
 if(pipe_!=INVALID_HANDLE_VALUE){DisconnectNamedPipe(pipe_);CloseHandle(pipe_);}
}
void LivePipeReceiver::accept(const std::wstring& name){
 if(pipe_!=INVALID_HANDLE_VALUE)throw std::runtime_error("Pipe server already started");
 const std::wstring path=pipePath(name);
 pipe_=CreateNamedPipeW(path.c_str(),PIPE_ACCESS_INBOUND,
         PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,
         1,0,65536,0,nullptr);
 if(pipe_==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create Windows named pipe");
 std::wcout<<L"Awaiting local PCM producer on "<<path<<L" ...\n";
 if(!ConnectNamedPipe(pipe_,nullptr)&&GetLastError()!=ERROR_PIPE_CONNECTED)
  throw std::runtime_error("ConnectNamedPipe failed");
 StreamHeaderBytes bytes{};readAll(pipe_,bytes.data(),bytes.size());
 format_=decodeStreamHeader(bytes);
 ring_=std::make_unique<AudioFrameRing>(24000,format_.channels); // 500 ms at 48k
 std::cout<<"Live stream connected: "<<format_.channels<<" channels at 48000 Hz\n";
 worker_=std::thread([this]{pump();});
}
void LivePipeReceiver::pump(){
 try{
  while(!cancel_.load(std::memory_order_acquire)){
   unsigned char countBytes[4]{};readAll(pipe_,countBytes,4);
   const uint32_t count=decodePacketFrames(countBytes);
   if(count==0)break; // explicit clean end marker
   std::vector<float> payload(size_t(count)*format_.channels);
   readAll(pipe_,payload.data(),payload.size()*sizeof(float));
   for(float& sample:payload){if(!std::isfinite(sample))sample=0.0f;else sample=std::clamp(sample,-1.0f,1.0f);}
   size_t written=0;
   while(written<count&&!cancel_.load(std::memory_order_acquire)){
    written+=ring_->write(payload.data()+written*format_.channels,count-written);
    if(written<count)std::this_thread::sleep_for(std::chrono::milliseconds(2));
   }
  }
 }catch(const std::exception& e){if(!cancel_.load())error_=e.what();}
 finished_.store(true,std::memory_order_release);
}
size_t LivePipeReceiver::readFrames(size_t frames,std::vector<float>& samples){
 if(!ring_)throw std::runtime_error("Live pipe not initialized");
 samples.resize(frames*format_.channels);
 return ring_->read(samples.data(),frames);
}
}
