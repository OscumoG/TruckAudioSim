// SPDX-License-Identifier: MIT
// User-mode shared-memory PCM -> named pipe spatial input. Not an audio driver.
#include <windows.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include "shared_mapping.hpp"
#include "pipe_writer.hpp"
#include "device_select.hpp"
#include "route_guard.hpp"
#include "wav.hpp"
#include <chrono>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
namespace {
tas::StreamFormat fmtFor(unsigned channels,bool back){
 using namespace tas;
 switch(channels){
  case 2:return {2,48000,kStereo,kStreamFloat32,0};
  case 3:return {3,48000,kTwoPointOne,kStreamFloat32,0};
  case 6:return {6,48000,back?kFivePointOneBack:kFivePointOneSide,kStreamFloat32,0};
  case 8:return {8,48000,kSevenPointOne,kStreamFloat32,0};
  default:throw std::invalid_argument("Supported channels are 2,3,6,8");
 }
}
unsigned readUInt(const std::string&v){
 size_t used=0;
 const auto n=std::stoul(v,&used);
 if(used!=v.size() || n>UINT32_MAX)throw std::invalid_argument("Invalid integer: "+v);
 return static_cast<unsigned>(n);
}
struct ComGuard {
 bool initialized=false;
 ComGuard(){
  const HRESULT hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
  if(FAILED(hr))throw std::runtime_error("CoInitializeEx failed");
  initialized=true;
 }
 ~ComGuard(){if(initialized)CoUninitialize();}
};
int run(int argc,char**argv){
 unsigned channels=2,seconds=30;bool back=false;
 std::optional<uint32_t> sourceIndex;
 std::wstring sourceId;
 for(int i=1;i<argc;++i){
  const std::string k=argv[i];
  if(k=="--5.1-back"){back=true;continue;}
  if(k=="--help"){
   std::cout<<"tas_shm_bridge --channels 2|3|6|8 [--5.1-back] [--seconds 90]\n"
    "  [--source-index N | --source-id ID]  # REQUIRED for live WASAPI capture\n"
    "  [no source selector]                # WAV test sender only\n"
    "This program creates a shared PCM ring and forwards frames to tas_spatial --live.\n"
    "Choose a DIFFERENT output device in tas_spatial to avoid audio feedback.\n";
   return 0;
  }
  if(i+1>=argc)throw std::invalid_argument("Missing option value: "+k);
  const std::string v=argv[++i];
  if(k=="--source-id")sourceId.assign(v.begin(),v.end());
  else if(k=="--source-index")sourceIndex=readUInt(v);
  else if(k=="--channels")channels=readUInt(v);
  else if(k=="--seconds")seconds=readUInt(v);
  else throw std::invalid_argument("Unknown option: "+k);
 }
 if(seconds<1||seconds>3600)throw std::invalid_argument("--seconds out of range 1..3600");
 if(sourceIndex && !sourceId.empty())throw std::invalid_argument("Choose source-index or source-id, not both");
 auto fmt=fmtFor(channels,back);
 if(sourceIndex || !sourceId.empty()){
  ComGuard com;
  Microsoft::WRL::ComPtr<IMMDeviceEnumerator> devices;
  const HRESULT hr=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&devices));
  if(FAILED(hr))throw std::runtime_error("Cannot enumerate Windows render endpoints");
  auto endpoint=tas::selectRenderDevice(devices.Get(),sourceIndex,sourceId);
  fmt.sourceEndpointToken=tas::audioEndpointToken(tas::renderDeviceId(endpoint.Get()));
  if(fmt.sourceEndpointToken==0)throw std::runtime_error("Cannot identify selected source endpoint");
 }
 tas::SharedPcmMapping mapping(true,fmt);
 tas::PipeWriter pipe(fmt);
 std::cout<<"Shared PCM ready: "<<channels<<" channels at 48 kHz, source token 0x"
          <<std::hex<<fmt.sourceEndpointToken<<std::dec<<"\n"
          <<"For live capture start tas_shm_capture --input-index N (same source).\n"
          <<"For a WAV test start tas_shm_sender --wav demo_stereo_48k.wav.\n";
 std::vector<float> pcm(size_t(480)*channels,0.0f);
 const auto start=std::chrono::steady_clock::now();
 for(unsigned tick=0;tick<seconds*100;++tick){
  std::this_thread::sleep_until(start+std::chrono::milliseconds(10*(tick+1)));
  mapping.buffer().pop(pcm,480);
  pipe.frames(pcm.data(),480,false);
 }
 pipe.finish();
 std::cout<<"Finished; dropped="<<mapping.buffer().droppedFrames.load()
          <<" padded="<<mapping.buffer().paddedFrames.load()<<" PCM frames\n";
 return 0;
}
}
int main(int argc,char**argv){try{return run(argc,argv);}catch(const std::exception&e){
 std::cerr<<"tas_shm_bridge: "<<e.what()<<"\n";return 1;}}
