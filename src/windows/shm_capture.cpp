// SPDX-License-Identifier: MIT
// Experimental WASAPI endpoint loopback -> shared-memory PCM producer.
// Captures a COPY. This does NOT install, emulate, or replace a virtual driver.
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <ksmedia.h>
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "device_select.hpp"
#include "endpoint_format.hpp"
#include "shared_mapping.hpp"
#include "route_guard.hpp"
#include "wav.hpp"
using Microsoft::WRL::ComPtr;
namespace {
std::atomic<bool> stopRequested{false};
BOOL WINAPI consoleHandler(DWORD event) {
 if (event==CTRL_C_EVENT || event==CTRL_BREAK_EVENT) {stopRequested.store(true);return TRUE;}
 return FALSE;
}
void check(HRESULT hr,const char* action) {
 if(FAILED(hr)) {char msg[180];std::snprintf(msg,sizeof(msg),"%s failed: HRESULT 0x%08lX",action,static_cast<unsigned long>(hr));throw std::runtime_error(msg);}
}
struct ComGuard{ComGuard(){check(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"CoInitializeEx");}~ComGuard(){CoUninitialize();}};
struct Options {bool list=false;std::optional<uint32_t> inputIndex;std::wstring inputId;unsigned seconds=90;};
Options parse(int argc,char**argv) {
 Options o;
 for(int i=1;i<argc;++i) {
  std::string arg=argv[i];
  if(arg=="--help"||arg=="-h") {
   std::cout<<"tas_shm_capture --list-devices\n"
    "tas_shm_capture --input-index N [--seconds 90]\n"
    "tas_shm_capture --input-id ID [--seconds 90]\n"
    "Captures a COPY of an existing endpoint; requires tas_shm_bridge configured\n"
    "for that same source endpoint. Never use the spatial output as the input.\n";
   std::exit(0);
  }
  if(arg=="--list-devices"){o.list=true;continue;}
  if(i+1>=argc)throw std::runtime_error("Missing value for "+arg);
  std::string value=argv[++i];
  if(arg=="--input-id") {o.inputId.assign(value.begin(),value.end());continue;}
  size_t pos=0;unsigned long x=0;
  try{x=std::stoul(value,&pos);}catch(...){throw std::runtime_error("Invalid value for "+arg);}
  if(pos!=value.size())throw std::runtime_error("Invalid integer for "+arg);
  if(arg=="--input-index"){
   if(x>UINT32_MAX)throw std::runtime_error("Input index too large");
   o.inputIndex=static_cast<uint32_t>(x);
  }else if(arg=="--seconds"){
   if(x<1 || x>3600)throw std::runtime_error("Seconds must be 1..3600");
   o.seconds=static_cast<unsigned>(x);
  }else throw std::runtime_error("Unknown argument: "+arg);
 }
 if(!o.list && !o.inputIndex && o.inputId.empty())throw std::runtime_error("Select --input-index or --input-id");
 if(o.inputIndex && !o.inputId.empty())throw std::runtime_error("Choose only one input selector");
 return o;
}
struct CaptureFormat {tas::StreamFormat stream;bool pcm16=false;};
CaptureFormat inspectFormat(const WAVEFORMATEX* mix) {
 if(!mix)throw std::runtime_error("Missing WASAPI mix format");
 uint32_t mask=0;bool pcm16=false,isFloat=false;
 if(mix->wFormatTag==WAVE_FORMAT_EXTENSIBLE){
  if(mix->cbSize<22)throw std::runtime_error("Malformed WAVEFORMATEXTENSIBLE");
  const auto* ext=reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(mix);
  mask=ext->dwChannelMask;
  isFloat=(ext->SubFormat==KSDATAFORMAT_SUBTYPE_IEEE_FLOAT && mix->wBitsPerSample==32 && ext->Samples.wValidBitsPerSample==32);
  pcm16=(ext->SubFormat==KSDATAFORMAT_SUBTYPE_PCM && mix->wBitsPerSample==16 && ext->Samples.wValidBitsPerSample==16);
 }else{
  isFloat=mix->wFormatTag==WAVE_FORMAT_IEEE_FLOAT && mix->wBitsPerSample==32;
  pcm16=mix->wFormatTag==WAVE_FORMAT_PCM && mix->wBitsPerSample==16;
  if(mix->nChannels==2)mask=tas::kStereo;
 }
 if(!isFloat && !pcm16)throw std::runtime_error("Endpoint must use float32 or PCM16");
 const auto accepted=tas::acceptEndpointFormat({mix->nChannels,mix->nSamplesPerSec,mask,
  mix->wBitsPerSample,mix->wBitsPerSample,mix->nBlockAlign,mix->nAvgBytesPerSec,
  pcm16?tas::EndpointSample::PCM16:tas::EndpointSample::Float32});
 return {accepted.stream,accepted.convertPCM16};
}
int run(const Options& opt) {
 ComGuard com;
 ComPtr<IMMDeviceEnumerator> devices;
 check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&devices)),"MMDeviceEnumerator");
 if(opt.list){tas::printRenderDevices(devices.Get());return 0;}
 auto device=tas::selectRenderDevice(devices.Get(),opt.inputIndex,opt.inputId);
 ComPtr<IAudioClient> audio;
 check(device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(audio.GetAddressOf())),"Activate IAudioClient");
 WAVEFORMATEX* rawMix=nullptr;
 check(audio->GetMixFormat(&rawMix),"GetMixFormat");
 std::unique_ptr<WAVEFORMATEX,decltype(&CoTaskMemFree)> mix(rawMix,&CoTaskMemFree);
 auto fmt=inspectFormat(mix.get());
 fmt.stream.sourceEndpointToken=tas::audioEndpointToken(tas::renderDeviceId(device.Get()));
 if(fmt.stream.sourceEndpointToken==0)throw std::runtime_error("Cannot identify source endpoint");
 tas::SharedPcmMapping mapping(false);
 tas::SharedPcmProducerLock producerLock;
 tas::validatePcmProducerFormat(mapping.buffer().format,fmt.stream);
 check(audio->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK,0,0,mix.get(),nullptr),"Initialize loopback");
 ComPtr<IAudioCaptureClient> capture;
 check(audio->GetService(IID_PPV_ARGS(&capture)),"GetService IAudioCaptureClient");
 SetConsoleCtrlHandler(consoleHandler,TRUE);
 check(audio->Start(),"Start WASAPI");
 std::cout<<"Live loopback capture started; source token 0x"<<std::hex<<fmt.stream.sourceEndpointToken<<std::dec
          <<", "<<fmt.stream.channels<<" channels, 48kHz. ORIGINAL OUTPUT IS STILL AUDIBLE. Ctrl+C to end.\n";
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(opt.seconds);
 uint64_t received=0,accepted=0,discontinuities=0;
 try {
  while(!stopRequested.load() && std::chrono::steady_clock::now()<deadline) {
   UINT32 packet=0;check(capture->GetNextPacketSize(&packet),"GetNextPacketSize");
   while(packet) {
    BYTE* bytes=nullptr;UINT32 frames=0;DWORD flags=0;
    check(capture->GetBuffer(&bytes,&frames,&flags,nullptr,nullptr),"GetBuffer");
    try {
     if(flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)++discontinuities;
     const auto channels=static_cast<size_t>(fmt.stream.channels);
     std::vector<float> block(static_cast<size_t>(frames)*channels,0.0f);
     if(!(flags&AUDCLNT_BUFFERFLAGS_SILENT) && frames){
      if(fmt.pcm16){
       const auto* input=reinterpret_cast<const int16_t*>(bytes);
       for(size_t n=0;n<block.size();++n)block[n]=static_cast<float>(input[n])/32768.0f;
      }else{
       const auto* input=reinterpret_cast<const float*>(bytes);
       std::copy_n(input,block.size(),block.begin());
      }
     }
     accepted+=mapping.buffer().push(block,frames);received+=frames;
    } catch(...){capture->ReleaseBuffer(frames);throw;}
    check(capture->ReleaseBuffer(frames),"ReleaseBuffer");
    check(capture->GetNextPacketSize(&packet),"GetNextPacketSize");
   }
   std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
 }catch(...){audio->Stop();throw;}
 check(audio->Stop(),"Stop WASAPI");
 std::cout<<"Captured "<<received<<" frames; delivered "<<accepted
          <<", overflow "<<received-accepted<<", WASAPI discontinuities "<<discontinuities<<"\n";
 return accepted==received?0:2;
}
}
int main(int argc,char**argv){try{return run(parse(argc,argv));}catch(const std::exception&e){std::cerr<<"tas_shm_capture: "<<e.what()<<"\n";return 1;}}
