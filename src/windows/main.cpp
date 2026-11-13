// SPDX-License-Identifier: MIT
// Windows Spatial Sound renderer for WAV and live endpoint inputs.
// The stereo WAV smoke test can also exercise a shared-mode WASAPI output.
// Does not install a driver, access Spotify, or hook ETS2.
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <spatialaudioclient.h>
#include <wrl/client.h>
#include <propidl.h>
#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <memory>
#include <numbers>
#include <optional>
#include "device_select.hpp"
#include "endpoint_loopback.hpp"
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "profile.hpp"
#include "routing.hpp"
#include "wav.hpp"
#include "pose.hpp"
#include "live_pipe.hpp"
#include "normalization.hpp"
#include "spatial_mix.hpp"
#include "channel_layout.hpp"
#include "route_guard.hpp"
using Microsoft::WRL::ComPtr;
namespace {
void check(HRESULT hr, const char* what){
 if(FAILED(hr)){
  char msg[160];std::snprintf(msg,sizeof(msg),"%s failed HRESULT 0x%08lX",what,static_cast<unsigned long>(hr));
  throw std::runtime_error(msg);
 }
}
struct ScopedCom{ScopedCom(){check(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"CoInitializeEx");}~ScopedCom(){CoUninitialize();}};
struct WinEvent{HANDLE handle=nullptr;WinEvent(){handle=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!handle)throw std::runtime_error("CreateEvent failed");}~WinEvent(){if(handle)CloseHandle(handle);}};
std::atomic<bool> stopRequested{false};
BOOL WINAPI stopOnConsoleInterrupt(DWORD action){
 if(action!=CTRL_C_EVENT && action!=CTRL_BREAK_EVENT)return FALSE;
 stopRequested.store(true,std::memory_order_relaxed);
 return TRUE;
}
struct ConsoleInterrupt {
 explicit ConsoleInterrupt(bool enabled):enabled_(enabled){
  if(enabled_ && !SetConsoleCtrlHandler(stopOnConsoleInterrupt,TRUE))
   throw std::runtime_error("Could not register the console interrupt handler");
 }
 ~ConsoleInterrupt(){if(enabled_)SetConsoleCtrlHandler(stopOnConsoleInterrupt,FALSE);}
 bool enabled_;
};
struct Voice{
 tas::Speaker speaker;
 tas::SpeakerFilter filter;
 float gain=0.0f;
 ComPtr<ISpatialAudioObject> object;
 uint64_t nonzeroSamples=0;
 double sumSquares=0.0;
 float peak=0.0f;
};
struct Options{
 std::string preset,wav,poseFile;
 tas::Pose pose;
 bool probe=false,live=false,listDevices=false,smokeTest=false,channelTest=false,requireSpatial=false;
 bool editLive=false;
 std::optional<uint32_t> inputIndex,outputIndex;
 std::wstring inputId,outputId;
};
void usage(){
 std::cout<<"TruckAudioSim Windows Spatial Sound test\n"
 "tas_spatial.exe --list-devices\n"
 "tas_spatial.exe --probe [--output-index N|--output-id ID]\n"
 "tas_spatial.exe --smoke-test --wav stereo_48k.wav (--output-index N|--output-id ID) [--require-spatial]\n"
 "tas_spatial.exe --channel-test --wav demo_7_1_48k.wav --output-index N\n"
 "tas_spatial.exe --preset profile.json --live --output-index N [--pose-file camera-pose.json]\n"
 "tas_spatial.exe --preset profile.json (--input-index N|--input-id ID) (--output-index N|--output-id ID)\n"
 "Desktop bridge: add --edit-live and --pose-file pose.json to the endpoint mode.\n"
 "tas_spatial.exe --preset profile.json --wav audio_48k.wav [--yaw 0] [--head-x -0.4]\n"
 "                [--head-y 0] [--head-z 0] [--pose-file camera-pose.json]\n"
 "WAV: stereo / 2.1 / 5.1(side/back) / 7.1, PCM16 or float32, 48000 Hz.\n"
 "Choose Windows Sonic, Dolby Atmos or DTS on the *physical* audio device.\n"
 "--output-index selects the PHYSICAL output: never choose the captured virtual endpoint.\n"
 "For --live: start tas_stream_sender.exe --wav audio_48k.wav in a separate console.\n"
 "--channel-test maps 2.0 / 2.1 / 5.1 / 7.1 PCM to distinct spatial objects.\n"
 "This build has no ETS2 plugin or installed virtual driver.\n"
 "For stable device selection use --output-id ID (see --list-devices).\n";
}
std::string utf8(const std::wstring& text){
 if(text.empty())return {};
 const int count=static_cast<int>(text.size());
 const int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),count,nullptr,0,nullptr,nullptr);
 if(bytes<=0)throw std::runtime_error("Invalid command-line text");
 std::string result(static_cast<size_t>(bytes),'\0');
 WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),count,result.data(),bytes,nullptr,nullptr);
 return result;
}
Options parseArgs(int argc,wchar_t**argv){
 Options o;
 bool hasInitialPose=false;
 for(int i=1;i<argc;++i){
  const std::string k=utf8(argv[i]);
  if(k=="--help"||k=="-h"){usage();std::exit(0);}
  if(k=="--probe"){o.probe=true;continue;}
  if(k=="--list-devices"){o.listDevices=true;continue;}
  if(k=="--live"){o.live=true;continue;}
  if(k=="--smoke-test"){o.smokeTest=true;continue;}
  if(k=="--channel-test"){o.channelTest=true;continue;}
  if(k=="--require-spatial"){o.requireSpatial=true;continue;}
  if(k=="--edit-live"){o.editLive=true;continue;}
  if(i+1>=argc)throw std::runtime_error("Missing value for "+k);
  const std::string v=utf8(argv[++i]);
  if(k=="--preset")o.preset=v;
  else if(k=="--wav")o.wav=v;
  else if(k=="--yaw" || k=="--head-x" || k=="--head-y" || k=="--head-z"){
   hasInitialPose=true;
   const float value=std::stof(v);
   if(k=="--yaw")o.pose.yawDegrees=value;
   else if(k=="--head-x")o.pose.head.x=value;
   else if(k=="--head-y")o.pose.head.y=value;
   else o.pose.head.z=value;
  }
  else if(k=="--pose-file")o.poseFile=v;
  else if(k=="--output-id" || k=="--input-id"){
   auto& id=k=="--input-id"?o.inputId:o.outputId;
   if(v.empty())throw std::runtime_error("Empty endpoint ID for "+k);
   id=argv[i];
  }
  else if(k=="--output-index" || k=="--input-index"){
   uint32_t index=0;
   const auto parsed=std::from_chars(v.data(),v.data()+v.size(),index);
   if(parsed.ec!=std::errc{} || parsed.ptr!=v.data()+v.size())
    throw std::runtime_error("Invalid endpoint index for "+k);
   auto& selection=k=="--input-index"?o.inputIndex:o.outputIndex;
   selection=index;
  }
  else throw std::runtime_error("Unknown argument: "+k);
 }
 if(o.outputIndex && !o.outputId.empty())throw std::runtime_error("Choose output-index or output-id, not both");
 if(o.inputIndex && !o.inputId.empty())throw std::runtime_error("Choose input-index or input-id, not both");
 const bool endpointInput=o.inputIndex.has_value() || !o.inputId.empty();
 if(endpointInput && (o.probe || o.listDevices || (!o.outputIndex && o.outputId.empty())))
  throw std::runtime_error("Endpoint input requires an explicit, distinct physical output and playback mode");
 if(o.editLive && (!endpointInput || o.poseFile.empty()))
  throw std::runtime_error("--edit-live requires an endpoint input and --pose-file");
 if(o.requireSpatial && !o.smokeTest && !o.channelTest)
  throw std::runtime_error("--require-spatial is only valid with --smoke-test or --channel-test");
 const int sourceCount=int(!o.wav.empty())+int(o.live)+int(endpointInput);
 if(o.channelTest){
  if(o.smokeTest || o.live || endpointInput || o.probe || o.listDevices || !o.preset.empty() || !o.poseFile.empty() || hasInitialPose || o.wav.empty())
   throw std::runtime_error("--channel-test requires --wav and an explicit physical output; no preset, live source or pose options");
  if(!o.outputIndex && o.outputId.empty())
   throw std::runtime_error("--channel-test requires an explicit physical --output-index or --output-id");
 }
 if(o.smokeTest){
  if(o.wav.empty() || o.live || endpointInput || o.probe || o.listDevices || !o.preset.empty() || !o.poseFile.empty() || hasInitialPose)
   throw std::runtime_error("--smoke-test requires a stereo --wav and supplies its own moving source");
  if(!o.outputIndex && o.outputId.empty())
   throw std::runtime_error("--smoke-test requires an explicit physical --output-index or --output-id");
 }else if(!o.probe && !o.listDevices && (o.preset.empty() || sourceCount!=1))
  throw std::runtime_error("--preset is required with one source: --wav, --live, or an input endpoint");
 if(!std::isfinite(o.pose.yawDegrees)||std::fabs(o.pose.yawDegrees)>36000 ||
    !std::isfinite(o.pose.head.x)||!std::isfinite(o.pose.head.y)||!std::isfinite(o.pose.head.z)||
    std::fabs(o.pose.head.x)>5||std::fabs(o.pose.head.y)>5||std::fabs(o.pose.head.z)>5)
  throw std::runtime_error("Invalid initial listener pose");
 return o;
}

std::vector<tas::Speaker> speakersForOutput(const tas::Profile& profile,size_t capacity){
 auto speakers=profile.speakers;
 std::stable_sort(speakers.begin(),speakers.end(),[](const auto& a,const auto& b){
  if(a.enabled!=b.enabled)return a.enabled;
  return a.gainDb>b.gainDb;
 });
 if(speakers.size()>capacity)speakers.resize(capacity);
 return speakers;
}

tas::Profile loadDesktopProfile(const std::string& path){
 std::ifstream file(std::filesystem::path(std::u8string(path.begin(),path.end())),std::ios::binary);
 if(!file)throw std::runtime_error("Cannot open live preset");
 return tas::parseProfileText(std::string(std::istreambuf_iterator<char>(file),{}),true);
}

size_t playStereoFallback(IMMDevice* device,tas::WavReader& wav){
 WinEvent ready;
 ComPtr<IAudioClient> audio;
 check(device->Activate(__uuidof(IAudioClient),CLSCTX_INPROC_SERVER,nullptr,
       reinterpret_cast<void**>(audio.GetAddressOf())),"IAudioClient activation");
 WAVEFORMATEX format{};
 format.wFormatTag=WAVE_FORMAT_IEEE_FLOAT;
 format.nChannels=2;
 format.nSamplesPerSec=wav.sampleRate();
 format.wBitsPerSample=32;
 format.nBlockAlign=2*sizeof(float);
 format.nAvgBytesPerSec=format.nSamplesPerSec*format.nBlockAlign;
 const DWORD flags=AUDCLNT_STREAMFLAGS_EVENTCALLBACK|AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|
                   AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
 check(audio->Initialize(AUDCLNT_SHAREMODE_SHARED,flags,0,0,&format,nullptr),
       "WASAPI stereo fallback initialization");
 check(audio->SetEventHandle(ready.handle),"WASAPI SetEventHandle");
 ComPtr<IAudioRenderClient> render;
 check(audio->GetService(IID_PPV_ARGS(&render)),"IAudioRenderClient");
 UINT32 bufferFrames=0;
 check(audio->GetBufferSize(&bufferFrames),"WASAPI GetBufferSize");
 if(!bufferFrames)throw std::runtime_error("WASAPI returned an empty render buffer");

 std::vector<float> samples;
 const auto submit=[&](UINT32 capacity){
  const auto frames=static_cast<UINT32>(wav.readFrames(capacity,samples));
  if(frames){
   BYTE* buffer=nullptr;
   check(render->GetBuffer(frames,&buffer),"WASAPI GetBuffer");
   std::memcpy(buffer,samples.data(),size_t(frames)*format.nBlockAlign);
   check(render->ReleaseBuffer(frames,0),"WASAPI ReleaseBuffer");
  }
  return frames;
 };

 size_t totalFrames=submit(bufferFrames);
 if(!totalFrames)throw std::runtime_error("Smoke-test WAV contains no audio frames");
 bool finished=totalFrames<bufferFrames;
 check(audio->Start(),"WASAPI Start");
 while(true){
  if(WaitForSingleObject(ready.handle,3000)!=WAIT_OBJECT_0)
   throw std::runtime_error("WASAPI stereo fallback timed out");
  UINT32 padding=0;
  check(audio->GetCurrentPadding(&padding),"WASAPI GetCurrentPadding");
  if(padding>bufferFrames)throw std::runtime_error("WASAPI returned invalid buffer padding");
  if(finished && !padding)break;
  const UINT32 capacity=bufferFrames-padding;
  if(!finished && capacity){
   const UINT32 submitted=submit(capacity);
   totalFrames+=submitted;
   finished=submitted<capacity;
  }
 }
 check(audio->Stop(),"WASAPI Stop");
 return totalFrames;
}

int run(const Options& opt){
 ScopedCom com;
 ComPtr<IMMDeviceEnumerator> devices;
 check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&devices)),"MMDeviceEnumerator");
 if(opt.listDevices){tas::printRenderDevices(devices.Get());return 0;}
 ComPtr<IMMDevice> device=tas::selectRenderDevice(devices.Get(),opt.outputIndex,opt.outputId);
 std::unique_ptr<tas::WavReader> wav;
 if(opt.smokeTest || opt.channelTest){
  wav=std::make_unique<tas::WavReader>(opt.wav);
  if(opt.smokeTest && wav->channels()!=2)throw std::runtime_error("--smoke-test expects a stereo 48 kHz WAV");
  std::cout<<"Selected render endpoint token: 0x"<<std::hex
           <<tas::audioEndpointToken(tas::renderDeviceId(device.Get()))<<std::dec<<"\n";
 }
 const auto stereoFallback=[&](const char* reason){
  std::cout<<"Spatial smoke test unavailable: "<<reason<<"\n";
  if(opt.requireSpatial)return 2;
  std::cout<<"Smoke-test mode: WASAPI_STEREO_FALLBACK (original stereo WAV)\n";
  const size_t frames=playStereoFallback(device.Get(),*wav);
  std::cout<<"Smoke-test stream completed: "<<frames<<" input frames\n";
  return 0;
 };
 ComPtr<ISpatialAudioClient> spatial;
 const HRESULT activation=device->Activate(__uuidof(ISpatialAudioClient),CLSCTX_INPROC_SERVER,nullptr,
                        reinterpret_cast<void**>(spatial.GetAddressOf()));
 if(opt.smokeTest && (activation==E_NOINTERFACE || activation==E_NOTIMPL))
  return stereoFallback("ISpatialAudioClient is unavailable on this endpoint");
 check(activation,"ISpatialAudioClient activation");
 UINT32 maxObjects=0;
 check(spatial->GetMaxDynamicObjectCount(&maxObjects),"GetMaxDynamicObjectCount");
 if(opt.probe || opt.smokeTest || opt.channelTest)
  std::cout<<"Dynamic spatial objects available: "<<maxObjects<<"\n";
 if(opt.probe){
  std::cout<<"Note: this API does not identify the vendor spatializer by name.\n";
  return maxObjects?0:2;
 }
 if(opt.smokeTest && !maxObjects)return stereoFallback("Windows reported zero dynamic objects");
 if(maxObjects==0)throw std::runtime_error("No dynamic objects. Activate Spatial Sound on your physical output device.");
 tas::Profile profile;
 if(opt.smokeTest){
  profile.name="Windows spatial smoke test";
  profile.speakers.push_back({"smoke","Moving WAV","M","speaker",{0.0f,0.0f,-1.0f},0.0f});
 }else if(!opt.channelTest){
  profile=opt.editLive?loadDesktopProfile(opt.preset):tas::loadProfile(opt.preset);
 }
 std::unique_ptr<tas::LivePipeReceiver> pipe;
 std::unique_ptr<tas::EndpointLoopback> loopback;
 uint16_t channels=0;uint32_t channelMask=0,sampleRate=0;
 if(opt.live){
  pipe=std::make_unique<tas::LivePipeReceiver>();
  pipe->accept();
  const auto fmt=pipe->format();
  const auto outputToken=tas::audioEndpointToken(tas::renderDeviceId(device.Get()));
  if(!tas::inputOutputRouteSafe(fmt.sourceEndpointToken,outputToken))
   throw std::runtime_error("AUDIO LOOP BLOCKED: input and spatial output are the same endpoint. Select a different physical --output-id.");
  if(fmt.sourceEndpointToken==0)
   std::cerr<<"Warning: audio source endpoint is unknown; verify manually that it differs from spatial output.\n";
  channels=fmt.channels;channelMask=fmt.channelMask;sampleRate=fmt.sampleRate;
 }else if(opt.inputIndex || !opt.inputId.empty()){
  const auto input=tas::selectRenderDevice(devices.Get(),opt.inputIndex,opt.inputId);
  if(tas::renderDeviceId(input.Get())==tas::renderDeviceId(device.Get()))
   throw std::runtime_error("AUDIO LOOP BLOCKED: choose different input and output endpoints");
  loopback=std::make_unique<tas::EndpointLoopback>(input.Get());
  const auto& fmt=loopback->format();
  channels=fmt.channels;channelMask=fmt.channelMask;sampleRate=fmt.sampleRate;
 }else{
  if(!wav)wav=std::make_unique<tas::WavReader>(opt.wav);
  channels=wav->channels();channelMask=wav->channelMask();sampleRate=wav->sampleRate();
 }
 if(opt.channelTest){
  profile=tas::makeChannelTestProfile(channels,channelMask);
  std::cout<<"Channel-test mode: SPATIAL_DISCRETE_CHANNELS ("<<channels
           <<" independent objects, no upmix or LFE synthesis)\n";
  std::cout<<"WAVEFORMATEXTENSIBLE channel order:";
  for(const auto& speaker:profile.speakers)std::cout<<" "<<speaker.channel;
  std::cout<<"\n";
 }
 WAVEFORMATEX objectFormat{};
 objectFormat.wFormatTag=WAVE_FORMAT_IEEE_FLOAT;
 objectFormat.nChannels=1;
 objectFormat.wBitsPerSample=32;
 objectFormat.nSamplesPerSec=sampleRate;
 objectFormat.nBlockAlign=4;
 objectFormat.nAvgBytesPerSec=objectFormat.nSamplesPerSec*4;
 const HRESULT formatSupport=spatial->IsAudioObjectFormatSupported(&objectFormat);
 if(opt.smokeTest && formatSupport==AUDCLNT_E_UNSUPPORTED_FORMAT)
  return stereoFallback("the endpoint rejected the 48 kHz float mono object format");
 check(formatSupport,"IsAudioObjectFormatSupported (float mono 48k)");
 if(opt.channelTest && profile.speakers.size()>maxObjects)
  throw std::runtime_error("Insufficient dynamic spatial objects for the discrete channel test; no channels will be dropped");
 if(profile.speakers.size()>maxObjects){
  std::cerr<<"Warning: only "<<maxObjects<<" dynamic objects available. Using highest-gain speakers.\n";
 }
 auto candidates=opt.channelTest?profile.speakers:speakersForOutput(profile,maxObjects);
 const float peakCeiling=opt.editLive?0.95f:0.75f;
 auto voiceGains=opt.channelTest ? std::vector<float>(candidates.size(),1.0f)
      : tas::neutralVoiceGains(candidates,channels,channelMask,peakCeiling,opt.editLive?sampleRate:0);
 const auto objectCount=opt.editLive?std::min<UINT32>(16,maxObjects):static_cast<UINT32>(candidates.size());
 WinEvent ready;
 SpatialAudioObjectRenderStreamActivationParams params{};
 params.ObjectFormat=&objectFormat;
 params.StaticObjectTypeMask=AudioObjectType_None;
 params.MinDynamicObjectCount=objectCount;
 params.MaxDynamicObjectCount=objectCount;
 params.Category=AudioCategory_Media;
 params.EventHandle=ready.handle;
 params.NotifyObject=nullptr;
 PROPVARIANT pv{};PropVariantInit(&pv);pv.vt=VT_BLOB;
 pv.blob.cbSize=sizeof(params);
 pv.blob.pBlobData=reinterpret_cast<BYTE*>(&params);
 ComPtr<ISpatialAudioObjectRenderStream> stream;
 const HRESULT streamActivation=spatial->ActivateSpatialAudioStream(&pv,__uuidof(ISpatialAudioObjectRenderStream),
       reinterpret_cast<void**>(stream.GetAddressOf()));
 if(opt.smokeTest && (streamActivation==SPTLAUDCLNT_E_NO_MORE_OBJECTS ||
                      streamActivation==SPTLAUDCLNT_E_STREAM_NOT_AVAILABLE))
  return stereoFallback("Windows could not reserve a dynamic object stream");
 check(streamActivation,"ActivateSpatialAudioStream");
 std::vector<Voice> voices;
 voices.reserve(candidates.size());
 tas::Pose target=opt.pose,current=opt.pose;
 if(opt.editLive){
  try{current=target=tas::loadPoseFile(opt.poseFile);}catch(const std::exception&){}
 }
 const ConsoleInterrupt consoleInterrupt{loopback != nullptr};
 check(stream->Start(),"Spatial stream start");
 if(loopback){
  loopback->start();
  std::cout<<"Mode: WASAPI_LOOPBACK_TO_SPATIAL_DYNAMIC. Ctrl+C to stop.\n";
 }
 if(opt.smokeTest)
  std::cout<<"Smoke-test mode: SPATIAL_DYNAMIC (one moving object, stereo WAV folded to mono)\n";
 std::cout<<"Rendering "<<profile.name<<" input="<<channels<<"ch mask=0x"<<std::hex<<channelMask
          <<std::dec<<" as "<<candidates.size()<<" moving spatial objects\n";
 size_t totalFrames=0;
 uint64_t outputFrames=0, silentSourceFrames=0, clippedObjectSamples=0;
 float preLimitPeak=0.0f, postLimitPeak=0.0f;
 size_t drainFrames=4800;
 auto lastPoll=std::chrono::steady_clock::time_point{};
 std::string lastProfile;
 std::vector<float> samples;
 float minX=1.0f,maxX=-1.0f;
 while(!stopRequested.load(std::memory_order_relaxed)){
  const DWORD waitResult=WaitForSingleObject(ready.handle,3000);
  if(stopRequested.load(std::memory_order_relaxed))break;
  if(waitResult!=WAIT_OBJECT_0)
   throw std::runtime_error("Spatial audio timed out or device was disconnected");
  UINT32 availableObjects=0,frameCount=0;
  check(stream->BeginUpdatingAudioObjects(&availableObjects,&frameCount),"BeginUpdatingAudioObjects");
  if(voices.empty()){
   if(availableObjects<objectCount)
    throw std::runtime_error("The spatial renderer has fewer dynamic objects available than reserved; playback cannot start without losing channels");
   for(UINT32 i=0;i<objectCount;++i){
    Voice v;
    if(i<candidates.size()){v.speaker=candidates[i];v.gain=voiceGains[i];}
    v.filter.configure(v.speaker,sampleRate);
    check(stream->ActivateSpatialAudioObject(AudioObjectType_Dynamic,&v.object),"ActivateSpatialAudioObject");
    voices.push_back(std::move(v));
   }
   if(opt.channelTest)std::cout<<"Activated discrete objects: "<<voices.size()<<"/"<<channels<<"\n";
   if(opt.editLive)std::cout<<"Desktop PCM: "<<channels<<" channels, "<<sampleRate<<" Hz; "<<objectCount<<" objects\n"<<std::flush;
  }
  if(!opt.poseFile.empty() && std::chrono::steady_clock::now()-lastPoll>std::chrono::milliseconds(30)){
   lastPoll=std::chrono::steady_clock::now();
   const HANDLE input=GetStdHandle(STD_INPUT_HANDLE);
   if(opt.editLive && GetFileType(input)==FILE_TYPE_PIPE &&
      !PeekNamedPipe(input,nullptr,0,nullptr,nullptr,nullptr) && GetLastError()==ERROR_BROKEN_PIPE)break;
   try{target=tas::loadPoseFile(opt.poseFile);}catch(const std::exception&){/* Retain last valid pose on partial writes. */}
   if(opt.editLive){
    try{
     std::ifstream file(std::filesystem::path(std::u8string(opt.preset.begin(),opt.preset.end())),std::ios::binary);
     std::string raw((std::istreambuf_iterator<char>(file)),{});
     if(!raw.empty() && raw!=lastProfile){
      auto revised=tas::parseProfileText(raw,true);
      candidates=speakersForOutput(revised,objectCount);
      voiceGains=tas::neutralVoiceGains(candidates,channels,channelMask,peakCeiling,sampleRate);
      for(size_t i=0;i<voices.size();++i){
       auto next=i<candidates.size()?candidates[i]:tas::Speaker{};
       auto& voice=voices[i];
       if(voice.speaker.id!=next.id || voice.speaker.type!=next.type ||
          voice.speaker.channel!=next.channel || voice.speaker.cabAudio!=next.cabAudio)
        voice.filter.configure(next,sampleRate);
       voice.speaker=std::move(next);
       voices[i].gain=i<voiceGains.size()?voiceGains[i]:0.0f;
      }
      if(revised.speakers.size()>objectCount)
       std::cout<<"Desktop capacity: "<<objectCount<<" of "<<revised.speakers.size()<<" speakers can play\n"<<std::flush;
      else std::cout<<"Desktop capacity: OK\n"<<std::flush;
      lastProfile=std::move(raw);
     }
    }catch(const std::exception&){/* Keep the last complete preset. */}
   }
  }
  auto audible=target;
  if(!audible.electronicsOn)audible.volume=0;
  current=tas::smoothPose(current,audible,0.12f);
  // Windows Spatial Sound's object volume is limited to unity. Apply boost
  // to the PCM before the mixer clamps samples to the valid output range.
  const float masterGain=current.volume*(opt.editLive?tas::kDesktopReferenceGain:1.0f);
  const float pcmBoost=std::max(1.0f,masterGain);
  const float objectVolume=std::min(1.0f,masterGain)*current.exteriorGain;
  size_t count=0;
  if(loopback)count=loopback->readFrames(frameCount,samples);
  else if(pipe)count=pipe->readFrames(frameCount,samples);
  else count=wav->readFrames(frameCount,samples);
  if(count>frameCount)
   throw std::runtime_error("Audio source returned more frames than the spatial renderer requested");
  if(loopback || (pipe && !pipe->finished()))silentSourceFrames+=frameCount-count;
  for(auto& v:voices){
   BOOL active=FALSE;
   check(v.object->IsActive(&active),"ISpatialAudioObject::IsActive");
   if(!active)
    throw std::runtime_error("A spatial audio object was invalidated; check the physical output device or spatial sound availability");
   BYTE* buffer=nullptr;UINT32 bytes=0;
   check(v.object->GetBuffer(&buffer,&bytes),"ISpatialAudioObject::GetBuffer");
   if(bytes<frameCount*sizeof(float))throw std::runtime_error("Spatial output buffer too small");
   // The loopback and pipe may return fewer frames than Windows requests.
   // Explicitly zero the tail: do not read stale or uninitialized PCM.
   const auto stats=tas::mixSpatialObject(v.speaker,samples,count,frameCount,
                                        channels,channelMask,v.gain*pcmBoost,
                                        current.volume>0 && v.gain>0,reinterpret_cast<float*>(buffer),
                                        &v.filter);
   if(opt.channelTest){
    const auto* pcm=reinterpret_cast<const float*>(buffer);
    for(size_t i=0;i<count;++i){
     const float value=pcm[i];
     if(value!=0.0f)++v.nonzeroSamples;
     v.sumSquares+=double(value)*double(value);
     v.peak=std::max(v.peak,std::abs(value));
    }
   }
   clippedObjectSamples+=stats.clippedSamples;
   preLimitPeak=std::max(preLimitPeak,stats.inputPeak);
   postLimitPeak=std::max(postLimitPeak,stats.outputPeak);
   auto rel=tas::relativePosition(v.speaker.pos,current);
   if(opt.smokeTest){
    // One full left/right sweep every four seconds, measured on the WAV clock.
    const double phase=double(totalFrames%(4*sampleRate))/(4*sampleRate);
    rel={-static_cast<float>(std::cos(2*std::numbers::pi*phase)),0.0f,-1.0f};
    if(count){
     minX=std::min(minX,rel.x);
     maxX=std::max(maxX,rel.x);
    }
   }
   check(v.object->SetPosition(rel.x,rel.y,rel.z),"SetPosition");
   check(v.object->SetVolume(objectVolume),"SetVolume");
  }
  check(stream->EndUpdatingAudioObjects(),"EndUpdatingAudioObjects");
  totalFrames+=count;
  outputFrames+=frameCount;
  if(!loopback && count==0 && (!pipe || pipe->finished() && pipe->bufferedFrames()==0)){
   if(pipe && !pipe->error().empty())throw std::runtime_error("Live sender: "+pipe->error());
   if(frameCount>=drainFrames)break;
   drainFrames-=frameCount;
  }
 }
 check(stream->Stop(),"Spatial stream stop");
 if(loopback)std::cout<<"Loopback discontinuities="<<loopback->discontinuities()
                       <<", dropped frames="<<loopback->droppedFrames()<<"\n";
 if(opt.channelTest){
  bool allChannelsPresent=totalFrames>0 && voices.size()==channels;
  for(const auto& v:voices){
   const float rms=totalFrames?static_cast<float>(std::sqrt(v.sumSquares/double(totalFrames))):0.0f;
   std::cout<<"Object "<<v.speaker.channel<<": nonzero_samples="<<v.nonzeroSamples
            <<" rms="<<rms<<" peak="<<v.peak<<"\n";
   if(!v.nonzeroSamples)allChannelsPresent=false;
  }
  std::cout<<"Discrete channel source check: "<<(allChannelsPresent?"PASS":"FAIL")<<"\n";
  if(!allChannelsPresent)return 2;
 }
 if(opt.smokeTest){
  if(!totalFrames)throw std::runtime_error("Smoke-test WAV contains no audio frames");
  std::cout<<"Smoke-test stream completed: "<<totalFrames<<" input frames, object X range "
           <<minX<<" to "<<maxX<<" meters\n";
 }
 std::cout<<"PCM telemetry: rendered="<<outputFrames
          <<", missing_source_frames="<<silentSourceFrames
          <<", clipped_object_samples="<<clippedObjectSamples
          <<", peak_before_limit="<<preLimitPeak
          <<", peak_after_limit="<<postLimitPeak<<"\n";
 std::cout<<"Finished ("<<totalFrames<<" input frames consumed).\n";
 return 0;
}
}
int wmain(int argc,wchar_t**argv){
 try{return run(parseArgs(argc,argv));}
 catch(const std::exception&e){std::cerr<<"Error: "<<e.what()<<"\n";usage();return 1;}
}
