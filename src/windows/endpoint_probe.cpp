// SPDX-License-Identifier: MIT
// Reports EXACT WASAPI shared-mode PCM format support for a playback endpoint.
// This does not install or modify any audio driver. Use it to check future SysVAD builds.
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <ksmedia.h>
#include <wrl/client.h>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <cstdlib>
#include "device_select.hpp"
#include "wav.hpp"
#include "route_guard.hpp"
using Microsoft::WRL::ComPtr;
namespace {
void check(HRESULT hr,const char* why){if(FAILED(hr))throw std::runtime_error(why);}
void probe(IAudioClient* client,unsigned channels,DWORD mask){
 WAVEFORMATEXTENSIBLE f{};
 f.Format.wFormatTag=WAVE_FORMAT_EXTENSIBLE;
 f.Format.nChannels=static_cast<WORD>(channels);
 f.Format.nSamplesPerSec=48000;
 f.Format.wBitsPerSample=32;
 f.Format.nBlockAlign=static_cast<WORD>(channels*4);
 f.Format.nAvgBytesPerSec=f.Format.nBlockAlign*48000;
 f.Format.cbSize=sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX);
 f.Samples.wValidBitsPerSample=32;
 f.dwChannelMask=mask;
 f.SubFormat=KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
 WAVEFORMATEX* closest=nullptr;
 const HRESULT result=client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED,&f.Format,&closest);
 if(closest)CoTaskMemFree(closest);
 std::cout<<"  "<<channels<<" channels 48kHz float32 mask=0x"<<std::hex<<mask<<std::dec
          <<": "<<(result==S_OK?"EXACT":result==S_FALSE?"CLOSEST FORMAT ONLY":"NOT SUPPORTED")<<"\n";
}
int run(int argc,char**argv){
 bool list=false;std::optional<uint32_t> index;std::wstring id;
 for(int i=1;i<argc;++i){
  std::string a=argv[i];
  if(a=="--help"||a=="-h"){
   std::cout<<"tas_endpoint_probe --list-devices\n"
            <<"tas_endpoint_probe --input-index N | --input-id ID\n"
            <<"Reports exact 48kHz float32 shared WASAPI formats; does not validate PCM transport.\n";
   return 0;
  }
  if(a=="--list-devices"){list=true;continue;}
  if(i+1>=argc)throw std::runtime_error("Missing argument for "+a);
  std::string v=argv[++i];
  if(a=="--input-id")id.assign(v.begin(),v.end());
  else if(a=="--input-index"){
   size_t n=0;const unsigned long x=std::stoul(v,&n);
   if(n!=v.size() || x>UINT32_MAX)throw std::runtime_error("Invalid endpoint index");
   index=static_cast<uint32_t>(x);
  }else throw std::runtime_error("Unknown argument: "+a);
 }
 if(index && !id.empty())throw std::runtime_error("Select an index OR stable id");
 if(!list&&!index&&id.empty())throw std::runtime_error("Select an endpoint with --input-index or --input-id");
 check(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"CoInitializeEx failed");
 struct ComGuard{~ComGuard(){CoUninitialize();}} guard;
 ComPtr<IMMDeviceEnumerator> enumerator;
 check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator)),"Device enumerator unavailable");
 if(list){tas::printRenderDevices(enumerator.Get());return 0;}
 auto device=tas::selectRenderDevice(enumerator.Get(),index,id);
 const auto stableId=tas::renderDeviceId(device.Get());
 std::wcout<<L"Selected endpoint: "<<stableId<<L"\n";
 std::cout<<"Identity token: 0x"<<std::hex<<tas::audioEndpointToken(stableId)<<std::dec<<"\n";
 ComPtr<IAudioClient> client;
 check(device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(client.GetAddressOf())),"IAudioClient unavailable");
 WAVEFORMATEX* mix=nullptr;
 check(client->GetMixFormat(&mix),"GetMixFormat failed");
 if(mix){
  std::cout<<"Current shared mix: "<<mix->nChannels<<"ch "<<mix->nSamplesPerSec<<"Hz "<<mix->wBitsPerSample<<"bit\n";
  CoTaskMemFree(mix);
 }
 probe(client.Get(),2,tas::kStereo);
 probe(client.Get(),6,tas::kFivePointOneSide);
 probe(client.Get(),6,tas::kFivePointOneBack);
 probe(client.Get(),8,tas::kSevenPointOne);
 std::cout<<"NOTE: Format support alone does not mean the driver carries real PCM.\n";
 return 0;
}
}
int main(int argc,char**argv){try{return run(argc,argv);}catch(const std::exception&e){std::cerr<<"Endpoint probe: "<<e.what()<<"\n";return 1;}}
