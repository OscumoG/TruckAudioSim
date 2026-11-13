// SPDX-License-Identifier: MIT
// Experimental WASAPI loopback of an existing Windows playback endpoint.
// Unlike a virtual driver, this captures a COPY of audio already rendered to
// that endpoint. To avoid an audio feedback loop, output MUST be a different
// *physical* endpoint chosen via tas_spatial --output-index.
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "device_select.hpp"
#include "pipe_writer.hpp"
#include "wav.hpp"
#include "wav_writer.hpp"
#include "endpoint_loopback.hpp"
#include "route_guard.hpp"
using Microsoft::WRL::ComPtr;
namespace {
std::atomic<bool> stopping{false};
BOOL WINAPI onConsole(DWORD action){
 if(action==CTRL_C_EVENT||action==CTRL_BREAK_EVENT){stopping.store(true);return TRUE;}
 return FALSE;
}
void check(HRESULT h,const char* name){
 if(FAILED(h)){char buffer[160];std::snprintf(buffer,sizeof(buffer),"%s: HRESULT 0x%08lX",name,static_cast<unsigned long>(h));throw std::runtime_error(buffer);}
}
struct ComScope{ComScope(){check(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"CoInitializeEx");}~ComScope(){CoUninitialize();}};
struct Options{bool list=false;std::optional<uint32_t> index;std::wstring inputId;unsigned seconds=90;std::string saveWav;};
Options parse(int argc,char**argv){
 Options o;
 for(int i=1;i<argc;++i){
  std::string a=argv[i];
  if(a=="--help"||a=="-h"){
   std::cout << "tas_endpoint_capture --list-devices\n"
    "tas_endpoint_capture --input-index N OR --input-id ID [--seconds 90] [--save-wav recording.wav]\n"
    "IMPORTANT: --input-index must NOT equal tas_spatial --output-index.\n"
    "Captures a COPY, not a virtual redirect. Supports 48kHz PCM16 or float32, \n"
    "with stereo/2.1/5.1/7.1 masks. Ctrl+C stops.\n";
   std::exit(0);
  }
  if(a=="--list-devices"){o.list=true;continue;}
  if(i+1>=argc)throw std::runtime_error("Missing value for "+a);
  const std::string val=argv[++i];
  if(a=="--input-id"){o.inputId.assign(val.begin(),val.end());continue;}
  if(a=="--save-wav"){
   if(val.empty())throw std::runtime_error("Empty save path");
   o.saveWav=val;
   continue;
  }
  size_t consumed=0;unsigned long value=0;
  try{value=std::stoul(val,&consumed);}catch(...){throw std::runtime_error("Invalid integer for "+a);}
  if(consumed!=val.size())throw std::runtime_error("Invalid integer for "+a);
  if(a=="--input-index"){
   if(value>UINT32_MAX)throw std::runtime_error("Input index too large");
   o.index=static_cast<uint32_t>(value);
  }else if(a=="--seconds"){
   if(value==0||value>3600)throw std::runtime_error("Seconds must be 1..3600");
   o.seconds=static_cast<unsigned>(value);
  }else throw std::runtime_error("Unknown option "+a);
 }
 if(!o.list&&!o.index&&o.inputId.empty())throw std::runtime_error("Select an input with --input-index N or --input-id ID (use --list-devices)");
 if(o.index && !o.inputId.empty())throw std::runtime_error("Choose either input-index or input-id");
 return o;
}
int run(const Options&o){
 ComScope com;
 ComPtr<IMMDeviceEnumerator> devices;
 check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&devices)),"CoCreateInstance(MMDeviceEnumerator)");
 if(o.list){tas::printRenderDevices(devices.Get());return 0;}
 ComPtr<IMMDevice> device=tas::selectRenderDevice(devices.Get(),o.index,o.inputId);
 ComPtr<IAudioClient> audio;
 check(device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(audio.GetAddressOf())),"Activate(IAudioClient)");
 WAVEFORMATEX* raw=nullptr;
 check(audio->GetMixFormat(&raw),"GetMixFormat");
 std::unique_ptr<WAVEFORMATEX,decltype(&CoTaskMemFree)> mix(raw,&CoTaskMemFree);
 if(!mix)throw std::runtime_error("Missing WASAPI mix format");
 auto f=tas::describeEndpointFormat(*mix);
 f.stream.sourceEndpointToken=tas::audioEndpointToken(tas::renderDeviceId(device.Get()));
 check(audio->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK,
                         0,0,mix.get(),nullptr),"IAudioClient::Initialize(loopback)");
 ComPtr<IAudioCaptureClient> capture;
 check(audio->GetService(IID_PPV_ARGS(&capture)),"GetService(IAudioCaptureClient)");
 std::unique_ptr<tas::PipeWriter> pipe;
 std::unique_ptr<tas::WavWriter> writer;
 if(o.saveWav.empty())pipe=std::make_unique<tas::PipeWriter>(f.stream);
 else writer=std::make_unique<tas::WavWriter>(o.saveWav,f.stream);
 SetConsoleCtrlHandler(onConsole,TRUE);
 check(audio->Start(),"IAudioClient::Start");
 std::cout<<"Capturing endpoint token 0x"<<std::hex<<f.stream.sourceEndpointToken<<std::dec<<" ["<<f.stream.channels<<"ch / 48k]. "
          <<"COPY only; choose a DIFFERENT physical spatial output! Ctrl+C to exit.\n";
 auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(o.seconds);
 uint64_t framesSent=0,discontinuities=0;
 try{
  while(!stopping.load()&&std::chrono::steady_clock::now()<deadline){
   UINT32 next=0;
   check(capture->GetNextPacketSize(&next),"GetNextPacketSize");
   while(next){
    BYTE* rawData=nullptr;UINT32 frames=0;DWORD flags=0;
    check(capture->GetBuffer(&rawData,&frames,&flags,nullptr,nullptr),"GetBuffer");
    try{
     if(flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)++discontinuities;
     const bool silent=(flags&AUDCLNT_BUFFERFLAGS_SILENT)!=0;
     if(f.convertPCM16 && !silent){
      const auto* samples=reinterpret_cast<const int16_t*>(rawData);
      std::vector<float> converted(static_cast<size_t>(frames)*f.stream.channels);
      for(size_t k=0;k<converted.size();++k)converted[k]=float(samples[k])/32768.0f;
      if(pipe)pipe->frames(converted.data(),frames,false);
      else writer->write(converted.data(),frames);
     }else{
      if(pipe)pipe->frames(reinterpret_cast<const float*>(rawData),frames,silent);
      else if(silent){
       std::vector<float> zeros(size_t(frames)*f.stream.channels,0.0f);
       writer->write(zeros.data(),frames);
      } else writer->write(reinterpret_cast<const float*>(rawData),frames);
     }
     framesSent+=frames;
    }catch(...){capture->ReleaseBuffer(frames);throw;}
    check(capture->ReleaseBuffer(frames),"ReleaseBuffer");
    check(capture->GetNextPacketSize(&next),"GetNextPacketSize");
   }
   std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
 }catch(...){audio->Stop();throw;}
 check(audio->Stop(),"IAudioClient::Stop");
 if(pipe)pipe->finish();
 if(writer)writer->finalize();
 std::cout<<"Finished, frames="<<framesSent<<", discontinuities="<<discontinuities<<"\n";
 return 0;
}
}
int main(int argc,char**argv){
 try{return run(parse(argc,argv));}
 catch(const std::exception&e){std::cerr<<"tas_endpoint_capture: "<<e.what()<<"\n";return 1;}
}
