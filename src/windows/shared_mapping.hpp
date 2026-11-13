// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>
#include <stdexcept>
#include <new>
#include <string>
#include "pcm_bridge.hpp"
namespace tas {
constexpr wchar_t kSharedPcmName[]=L"Local\\TruckAudioSim_UserModeBridge_v1";
// Map a local-session test buffer. This is NOT a driver or a kernel interface.
// A named mutex prevents two unrelated producer programs writing to the
// single-producer shared ring simultaneously. Acquired for producer lifetime.
class SharedPcmProducerLock {
 HANDLE handle_=nullptr;
public:
 SharedPcmProducerLock() {
  handle_=CreateMutexW(nullptr,FALSE,L"Local\\TruckAudioSim_PcmProducer_v1");
  if(!handle_)throw std::runtime_error("CreateMutexW failed for PCM producer");
  const DWORD status=WaitForSingleObject(handle_,0);
  if(status!=WAIT_OBJECT_0 && status!=WAIT_ABANDONED){CloseHandle(handle_);handle_=nullptr;
   throw std::runtime_error("Another shared PCM producer is active");}
 }
 ~SharedPcmProducerLock(){if(handle_){ReleaseMutex(handle_);CloseHandle(handle_);}}
 SharedPcmProducerLock(const SharedPcmProducerLock&)=delete;
 SharedPcmProducerLock& operator=(const SharedPcmProducerLock&)=delete;
};
class SharedPcmMapping {
 HANDLE mapping_=nullptr;
 SharedPcmBridge* buffer_=nullptr;
public:
 SharedPcmMapping(bool create,StreamFormat format={}) {
  if(create){
   mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,
            static_cast<DWORD>(sizeof(SharedPcmBridge)),kSharedPcmName);
   if(!mapping_)throw std::runtime_error("CreateFileMappingW failed");
   if(GetLastError()==ERROR_ALREADY_EXISTS){
    CloseHandle(mapping_);mapping_=nullptr;
    throw std::runtime_error("Bridge already exists; stop the previous bridge first");
   }
  }else{
   mapping_=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,kSharedPcmName);
   if(!mapping_)throw std::runtime_error("Start tas_shm_bridge first");
  }
  void* p=MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,0,0,sizeof(SharedPcmBridge));
  if(!p){CloseHandle(mapping_);mapping_=nullptr;throw std::runtime_error("MapViewOfFile failed");}
  buffer_=static_cast<SharedPcmBridge*>(p);
  try{
   if(create){new(buffer_) SharedPcmBridge();buffer_->initialize(format);}
   buffer_->validate();
  }catch(...){UnmapViewOfFile(p);CloseHandle(mapping_);mapping_=nullptr;buffer_=nullptr;throw;}
 }
 ~SharedPcmMapping(){if(buffer_)UnmapViewOfFile(buffer_);if(mapping_)CloseHandle(mapping_);}
 SharedPcmMapping(const SharedPcmMapping&)=delete;
 SharedPcmMapping& operator=(const SharedPcmMapping&)=delete;
 SharedPcmBridge& buffer(){return *buffer_;}
};
}
