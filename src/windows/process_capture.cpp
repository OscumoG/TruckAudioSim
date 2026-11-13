// SPDX-License-Identifier: MIT
// Experimental process-specific loopback capture for Windows 11.
// Microsoft ApplicationLoopback API captures a COPY, not an exclusive output.
// This utility never injects into, modifies or terminates another process.
#include <windows.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <propidl.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "stream_protocol.hpp"
#include "wav.hpp"
using Microsoft::WRL::ComPtr;
namespace {
std::atomic<bool> interrupted{false};
BOOL WINAPI consoleControl(DWORD kind){
 if(kind==CTRL_C_EVENT||kind==CTRL_BREAK_EVENT){interrupted.store(true);return TRUE;}
 return FALSE;
}
void check(HRESULT hr,const char* why){
 if(FAILED(hr)){char msg[192];std::snprintf(msg,sizeof(msg),"%s: 0x%08lX",why,static_cast<unsigned long>(hr));throw std::runtime_error(msg);}
}
struct CoScope{
 CoScope(){check(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"CoInitializeEx");}
 ~CoScope(){CoUninitialize();}
};
struct EventHandle{
 HANDLE h=nullptr;
 explicit EventHandle(bool manual=false){h=CreateEventW(nullptr,manual?TRUE:FALSE,FALSE,nullptr);if(!h)throw std::runtime_error("CreateEventW failed");}
 ~EventHandle(){if(h)CloseHandle(h);}
 EventHandle(const EventHandle&)=delete;EventHandle& operator=(const EventHandle&)=delete;
};
// A genuine COM callback: the async activator can hold its reference until completion.
class ActivationHandler final:public IActivateAudioInterfaceCompletionHandler{
 std::atomic<ULONG> refs_{1};
 ComPtr<IUnknown> freeThreadedMarshaler_;
 EventHandle done_{true};
 HRESULT result_=E_PENDING;
 ComPtr<IAudioClient> client_;
 public:
 ActivationHandler(){check(CoCreateFreeThreadedMarshaler(static_cast<IUnknown*>(this),
                              freeThreadedMarshaler_.GetAddressOf()),
                              "CoCreateFreeThreadedMarshaler");}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out)override{
  if(!out)return E_POINTER;
  *out=nullptr;
  if(iid==__uuidof(IUnknown)||iid==__uuidof(IActivateAudioInterfaceCompletionHandler)){
   *out=static_cast<IActivateAudioInterfaceCompletionHandler*>(this);AddRef();return S_OK;
  }
  return freeThreadedMarshaler_?freeThreadedMarshaler_->QueryInterface(iid,out):E_NOINTERFACE;
 }
 ULONG STDMETHODCALLTYPE AddRef()override{return refs_.fetch_add(1)+1;}
 ULONG STDMETHODCALLTYPE Release()override{const ULONG left=refs_.fetch_sub(1)-1;if(!left)delete this;return left;}
 HRESULT STDMETHODCALLTYPE ActivateCompleted(IActivateAudioInterfaceAsyncOperation* operation)override{
  ComPtr<IUnknown> audio;
  HRESULT activation=E_UNEXPECTED;
  HRESULT hr=operation?operation->GetActivateResult(&activation,&audio):E_POINTER;
  if(SUCCEEDED(hr))hr=activation;
  if(SUCCEEDED(hr))hr=audio.As(&client_);
  result_=hr;
  SetEvent(done_.h);
  return S_OK;
 }
 ComPtr<IAudioClient> wait(){
  if(WaitForSingleObject(done_.h,15000)!=WAIT_OBJECT_0)throw std::runtime_error("Timed out activating process loopback");
  check(result_,"ActivateAudioInterfaceAsync completion");return client_;
 }
};
void writeAll(HANDLE dest,const void* ptr,size_t bytes){
 auto* p=static_cast<const uint8_t*>(ptr);
 while(bytes){
  DWORD written=0;
  const DWORD amount=static_cast<DWORD>(std::min<size_t>(bytes,65536));
  if(!WriteFile(dest,p,amount,&written,nullptr)||written==0)throw std::runtime_error("Audio pipe disconnected");
  p+=written;bytes-=written;
 }
}
struct PipeWriter{
 HANDLE h=INVALID_HANDLE_VALUE;
 explicit PipeWriter(){
  constexpr auto path=L"\\\\.\\pipe\\TruckAudioSimPcm";
  if(!WaitNamedPipeW(path,20000))throw std::runtime_error("No PCM receiver. Start tas_spatial --preset ... --live first.");
  h=CreateFileW(path,GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open TruckAudioSimPcm named pipe");
  const tas::StreamFormat fmt{2,48000,tas::kStereo,tas::kStreamFloat32};
  const auto header=tas::encodeStreamHeader(fmt);
  writeAll(h,header.data(),header.size());
 }
 void frames(const float* data,uint32_t frameCount,bool silent){
  if(frameCount==0)return;
  constexpr uint32_t maxChunk=tas::kMaxStreamPacketFrames;
  uint32_t offset=0;
  while(offset<frameCount){
   const uint32_t n=std::min(maxChunk,frameCount-offset);
   std::vector<float> b(static_cast<size_t>(n)*2,0);
   if(!silent){
    for(uint32_t j=0;j<n*2;++j){
     const float s=data[static_cast<size_t>(offset)*2+j];
     b[j]=std::isfinite(s)?std::clamp(s,-1.0f,1.0f):0;
    }
   }
   const auto packet=tas::encodePacketFrames(n);
   writeAll(h,packet.data(),packet.size());
   writeAll(h,b.data(),b.size()*sizeof(float));
   offset+=n;
  }
 }
 void finish(){
  if(h!=INVALID_HANDLE_VALUE){
   const auto end=tas::encodePacketFrames(0);
   writeAll(h,end.data(),end.size());
   FlushFileBuffers(h);
  }
 }
 ~PipeWriter(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
};
struct Options{DWORD pid=0;unsigned seconds=90;};
Options parse(int argc,char** argv){
 Options options;
 for(int i=1;i<argc;++i){
  const std::string a=argv[i];
  if(a=="--help"||a=="-h"){
   std::cout<<"Usage: tas_capture_process.exe --pid PROCESS_ID [--seconds 90]\n"
    "Start tas_spatial --preset ... --live first. Capture is a COPY and may sound doubled.\n"
    "Ctrl+C ends capture; no virtual driver is installed.\n";std::exit(0);
  }
  if(i+1>=argc)throw std::runtime_error("Missing argument for "+a);
  const std::string value=argv[++i];
  unsigned long parsed=0;size_t pos=0;
  try{parsed=std::stoul(value,&pos);}catch(...){throw std::runtime_error("Expected a positive integer for "+a);}
  if(pos!=value.size())throw std::runtime_error("Unexpected non-integer for "+a);
  if(a=="--pid"){
   if(parsed==0||parsed>0xFFFFFFFFu)throw std::runtime_error("Invalid process ID");
   options.pid=static_cast<DWORD>(parsed);
  }else if(a=="--seconds"){
   if(parsed==0||parsed>3600)throw std::runtime_error("--seconds must be 1..3600");
   options.seconds=static_cast<unsigned>(parsed);
  }else throw std::runtime_error("Unknown option: "+a);
 }
 if(!options.pid)throw std::runtime_error("--pid is required");
 return options;
}
ComPtr<IAudioClient> activate(DWORD pid){
 AUDIOCLIENT_ACTIVATION_PARAMS params{};
 params.ActivationType=AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
 params.ProcessLoopbackParams.TargetProcessId=pid;
 params.ProcessLoopbackParams.ProcessLoopbackMode=PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
 PROPVARIANT variant{};PropVariantInit(&variant);
 variant.vt=VT_BLOB;
 variant.blob.cbSize=sizeof(params);
 variant.blob.pBlobData=reinterpret_cast<BYTE*>(&params);
 auto* callback=new ActivationHandler();
 ComPtr<IActivateAudioInterfaceAsyncOperation> op;
 const HRESULT hr=ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,
  __uuidof(IAudioClient),&variant,callback,&op);
 if(FAILED(hr)){callback->Release();check(hr,"ActivateAudioInterfaceAsync");}
 try{
  auto client=callback->wait();callback->Release();return client;
 }catch(...){callback->Release();throw;}
}
int run(const Options& opt){
 CoScope com;
 SetConsoleCtrlHandler(consoleControl,TRUE);
 ComPtr<IAudioClient> audio=activate(opt.pid);
 // Request native float32/48k stereo and let WASAPI handle conversions.
 WAVEFORMATEX fmt{};
 fmt.wFormatTag=WAVE_FORMAT_IEEE_FLOAT;
 fmt.nChannels=2;
 fmt.nSamplesPerSec=48000;
 fmt.wBitsPerSample=32;
 fmt.nBlockAlign=8;
 fmt.nAvgBytesPerSec=fmt.nSamplesPerSec*fmt.nBlockAlign;
 fmt.cbSize=0;
 const DWORD flags=AUDCLNT_STREAMFLAGS_LOOPBACK|AUDCLNT_STREAMFLAGS_EVENTCALLBACK|
                   AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
 check(audio->Initialize(AUDCLNT_SHAREMODE_SHARED,flags,0,0,&fmt,nullptr),
       "Initialize app loopback (float32 stereo 48k)");
 EventHandle sampleEvent;
 check(audio->SetEventHandle(sampleEvent.h),"SetEventHandle");
 ComPtr<IAudioCaptureClient> capture;
 check(audio->GetService(IID_PPV_ARGS(&capture)),"GetService(IAudioCaptureClient)");
 PipeWriter pipe;
 check(audio->Start(),"IAudioClient::Start");
 std::cout<<"Capturing process "<<opt.pid<<" at 48kHz stereo for up to "<<opt.seconds
          <<"s. This CAPTURES A COPY; original playback is not redirected. Ctrl+C to stop.\n";
 uint64_t total=0;
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(opt.seconds);
 try{
  while(!interrupted.load()&&std::chrono::steady_clock::now()<deadline){
   const auto wait=WaitForSingleObject(sampleEvent.h,250);
   if(wait==WAIT_FAILED)throw std::runtime_error("WaitForSingleObject failed");
   if(wait!=WAIT_OBJECT_0)continue;
   UINT32 packetFrames=0;
   check(capture->GetNextPacketSize(&packetFrames),"GetNextPacketSize");
   while(packetFrames){
    BYTE* data=nullptr;UINT32 frames=0;DWORD bufferFlags=0;
    check(capture->GetBuffer(&data,&frames,&bufferFlags,nullptr,nullptr),"GetBuffer");
    // Always release the packet, even if downstream IPC fails.
    try{
     pipe.frames(reinterpret_cast<const float*>(data),frames,
                 (bufferFlags&AUDCLNT_BUFFERFLAGS_SILENT)!=0);
     total+=frames;
    }catch(...){capture->ReleaseBuffer(frames);throw;}
    check(capture->ReleaseBuffer(frames),"ReleaseBuffer");
    check(capture->GetNextPacketSize(&packetFrames),"GetNextPacketSize");
   }
  }
 }catch(...){audio->Stop();throw;}
 check(audio->Stop(),"IAudioClient::Stop");
 pipe.finish();
 std::cout<<"Captured and transmitted "<<total<<" frames.\n";
 return 0;
}
}
int main(int argc,char** argv){
 try{return run(parse(argc,argv));}
 catch(const std::exception& e){std::cerr<<"Capture error: "<<e.what()<<"\n";return 1;}
}
