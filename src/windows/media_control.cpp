// SPDX-License-Identifier: MIT
// Controls a selected Windows media session from ETS2/ATS electrical telemetry.
#include <windows.h>
#include <unknwn.h>
#include <winrt/windows.foundation.h>
#include <winrt/windows.foundation.collections.h>
#include <winrt/windows.media.control.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include "json.hpp"
#include "media_policy.hpp"
#include "pose.hpp"

namespace {
namespace wmc=winrt::Windows::Media::Control;
using Session=wmc::GlobalSystemMediaTransportControlsSession;
using Status=wmc::GlobalSystemMediaTransportControlsSessionPlaybackStatus;
using Action=tas::MediaAction;
std::atomic<bool> stopped{false};
BOOL WINAPI ctrlHandler(DWORD type){
 if(type!=CTRL_C_EVENT && type!=CTRL_BREAK_EVENT)return FALSE;
 stopped=true;
 return TRUE;
}

std::string lower(std::string value){
 std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return char(std::tolower(c));});
 return value;
}
struct Options { std::string pose,telemetry,app="spotify"; unsigned pollMs=150; bool dryRun=false; };
Options parse(int argc,wchar_t** argv){
 Options o;
 for(int i=1;i<argc;++i){
  const auto key=winrt::to_string(argv[i]);
  if(key=="--help" || key=="-h"){
   std::cout<<"tas_media_control --telemetry-file media.json | --pose-file pose.json [--app spotify]\n"
              "Options: --poll-ms 150 --dry-run\n";
   std::exit(0);
  }
  if(key=="--dry-run"){o.dryRun=true;continue;}
  if(++i>=argc)throw std::runtime_error("Missing value for "+key);
  const auto value=winrt::to_string(argv[i]);
  if(key=="--pose-file")o.pose=value;
  else if(key=="--telemetry-file")o.telemetry=value;
  else if(key=="--app")o.app=lower(value);
  else if(key=="--poll-ms"){
   const auto period=std::stoi(value);
   if(period<100 || period>2000)throw std::runtime_error("poll-ms must be 100..2000");
   o.pollMs=static_cast<unsigned>(period);
  }else throw std::runtime_error("Unknown argument "+key);
 }
 if(o.pose.empty()==o.telemetry.empty() || o.app.empty())throw std::runtime_error("Choose exactly one telemetry or pose file");
 return o;
}

struct Telemetry { std::string player,vehicle; bool electricOn=true; };
Telemetry readTelemetry(const Options& o){
 if(o.telemetry.empty())return {o.app,"pose",tas::loadPoseFile(o.pose).electronicsOn};
 std::ifstream file(std::filesystem::path(std::u8string(o.telemetry.begin(),o.telemetry.end())),std::ios::binary);
 std::string raw((std::istreambuf_iterator<char>(file)),{});
 const auto root=tas::JsonParser(raw).parse();
 const auto player=root.at("player").str();
 const auto vehicle=root.at("vehicle").str();
 const auto& electric=root.at("electricOn");
 if(vehicle.empty() || (player!="spotify" && player!="vlc") || electric.kind!=tas::Json::Kind::Boolean)return {};
 return {player,vehicle,electric.boolean};
}

Status playbackStatus(const Session& session){
 const auto info=session.GetPlaybackInfo();
 return info?info.PlaybackStatus():Status::Closed;
}
bool parentClosed(){
 const HANDLE input=GetStdHandle(STD_INPUT_HANDLE);
 if(GetFileType(input)!=FILE_TYPE_PIPE)return false;
 if(PeekNamedPipe(input,nullptr,0,nullptr,nullptr,nullptr))return false;
 const auto error=GetLastError();
 return error==ERROR_BROKEN_PIPE || error==ERROR_NO_DATA;
}

int run(const Options& o){
 winrt::init_apartment(winrt::apartment_type::multi_threaded);
 SetConsoleCtrlHandler(ctrlHandler,TRUE);
 const auto manager=wmc::GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
 tas::MediaPausePolicy policy;
 Session owned{nullptr};
 Telemetry previous;
 std::chrono::steady_clock::time_point pauseTime;
 std::string lastMessage;
 const auto report=[&](const std::string& message){
  if(message!=lastMessage)std::cout<<"Media: "<<message<<std::endl;
  lastMessage=message;
 };
 const auto clear=[&]{policy={};owned=nullptr;};
 while(!stopped && (o.telemetry.empty() || !parentClosed())){
  try{
   const auto telemetry=readTelemetry(o);
   if(telemetry.vehicle!=previous.vehicle || telemetry.player!=previous.player)clear();
   previous=telemetry;
   if(telemetry.vehicle.empty()){
    report("Waiting for truck electrics");
   }else{
    Session chosen{nullptr};
    if(owned){
     // Keep the actual session that received Pause. Never resolve Play by app ID.
     chosen=owned;
     const auto status=playbackStatus(chosen);
     if(status==Status::Closed || status==Status::Stopped || (!telemetry.electricOn && status==Status::Playing &&
        std::chrono::steady_clock::now()-pauseTime>std::chrono::milliseconds(650))){
      // A closed session or manual playback cancels our right to resume.
      policy.evaluate(telemetry.electricOn,false,{},false);
      owned=nullptr;
      chosen=nullptr;
     }
    }else{
     const auto current=manager.GetCurrentSession();
     if(current && lower(winrt::to_string(current.SourceAppUserModelId())).find(telemetry.player)!=std::string::npos)chosen=current;
     if(!chosen || playbackStatus(chosen)!=Status::Playing){
      for(const auto& session:manager.GetSessions()){
       if(lower(winrt::to_string(session.SourceAppUserModelId())).find(telemetry.player)==std::string::npos)continue;
       if(!chosen)chosen=session;
       if(playbackStatus(session)==Status::Playing){chosen=session;break;}
      }
     }
    }
    const auto status=chosen?playbackStatus(chosen):Status::Closed;
    const auto identity=chosen?winrt::to_string(chosen.SourceAppUserModelId()):std::string{};
    const auto command=policy.evaluate(telemetry.electricOn,bool(chosen),identity,status==Status::Playing);
    if(command==Action::None){
     if(!chosen)report("Waiting for "+telemetry.player+" in Windows media controls");
     else if(telemetry.electricOn && status==Status::Playing)report("Player connected: "+telemetry.player);
    }else{
     bool ok=false;
     try{
      if(o.dryRun)ok=true;
      else if(command==Action::Pause)ok=chosen.TryPauseAsync().get();
      else if(status==Status::Paused)ok=chosen.TryPlayAsync().get();
     }catch(...){policy.complete(command,false);throw;}
     policy.complete(command,ok);
     if(command==Action::Pause && ok){owned=chosen;pauseTime=std::chrono::steady_clock::now();}
     if(command==Action::Resume)owned=nullptr;
     report(command==Action::Pause
       ? (ok?"Paused by truck electrics":"Player rejected pause")
       : (ok?"Resumed by truck electrics":"Player was not resumed"));
    }
   }
  }catch(const winrt::hresult_error& e){
   report("Windows media control: "+winrt::to_string(e.message()));
  }catch(const std::exception&){
   // An in-progress file write is retried on the next poll.
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(o.pollMs));
 }
 return 0;
}
}
int wmain(int argc,wchar_t** argv){
 try{return run(parse(argc,argv));}
 catch(const winrt::hresult_error& e){std::cerr<<"tas_media_control: "<<winrt::to_string(e.message())<<"\n";}
 catch(const std::exception& e){std::cerr<<"tas_media_control: "<<e.what()<<"\n";}
 return 1;
}
