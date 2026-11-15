// SPDX-License-Identifier: MIT
#include "profile.hpp"
#include "json.hpp"
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <set>
namespace tas {
Profile loadProfile(const std::string& path){
  std::ifstream f(std::filesystem::path(std::u8string(path.begin(),path.end())),std::ios::binary);if(!f)throw std::runtime_error("Cannot open preset: "+path);
  std::string raw((std::istreambuf_iterator<char>(f)),{});
  return parseProfileText(raw);
}
Profile parseProfileText(const std::string& raw,bool allowEmpty){
  if(raw.size()>2*1024*1024)throw std::runtime_error("Preset exceeds 2 MiB");
  const Json root=JsonParser(raw).parse();
  if(root.at("truckerAuxPreset").num()!=1)throw std::runtime_error("Unsupported Trucker AUX schema");
  Profile p;p.name=root.at("name").str();
  if(auto x=root.get("vehicle"))p.vehicle=x->str();
  if(auto x=root.get("game"))p.game=x->str();
  auto& layout=root.at("layout");const auto& speakers=layout.at("speakers");
  const auto* space=layout.get("vehicleSpace");
  const bool coach=space&&space->kind==Json::Kind::String&&space->str()=="coach";
  if(const auto* cfg=layout.get("acoustics"); cfg && cfg->kind==Json::Kind::Object){
    auto& a=p.acoustics;
    const auto flag=[&](const char* key){
      const auto* v=cfg->get(key);
      return v && v->kind==Json::Kind::Boolean && v->boolean;
    };
    const auto read=[&](const char* key,float fallback,double lo,double hi,float scale=1){
      const auto* v=cfg->get(key);
      return v && v->kind==Json::Kind::Number ? static_cast<float>(std::clamp(v->num(),lo,hi))*scale : fallback;
    };
    a.enabled=flag("enabled");
    a.reflections=read("reflections",a.reflections,0,100,0.01f);
    a.absorption=read("absorption",a.absorption,0,100,0.01f);
    a.reverb=read("reverb",a.reverb,0,100,0.01f);
    a.decayMs=read("decayMs",a.decayMs,40,550);
    a.bodyLoss=read("bodyLoss",a.bodyLoss,0,18);
    a.bodyCutoff=read("bodyCutoff",a.bodyCutoff,300,4000);
    a.leftWindow=read("leftWindow",0,0,100,0.01f);
    a.rightWindow=read("rightWindow",0,0,100,0.01f);
    a.deckHeight=read("deckHeight",a.deckHeight,-50,280,0.01f);
    if(const auto* bounds=layout.get("bounds")){
      const auto vector=[&](const char* key,Vec3 fallback){
        const auto* v=bounds->get(key);
        if(!v || v->kind!=Json::Kind::Array || v->array.size()!=3) return fallback;
        const auto component=[&](size_t i,double limit){
          return static_cast<float>(std::clamp(v->array[i].num(),-limit,limit));
        };
        return Vec3{component(0,5),component(1,5),component(2,coach?16:5)};
      };
      const auto lo=vector("min",a.min), hi=vector("max",a.max);
      if(lo.x<hi.x && lo.y<hi.y && lo.z<hi.z){a.min=lo;a.max=hi;}
    }
    a.splitDeck=flag("splitDeck") && a.deckHeight>a.min.y && a.deckHeight<a.max.y;
  }
  if(const auto* eq=layout.get("equalizer"); eq && eq->kind==Json::Kind::Object){
    const auto* enabled=eq->get("enabled");
    p.equalizer.enabled=enabled && enabled->kind==Json::Kind::Boolean && enabled->boolean;
    if(const auto* gains=eq->get("gains"); gains && gains->kind==Json::Kind::Array){
      for(size_t i=0;i<std::min(gains->array.size(),p.equalizer.gains.size());++i){
        if(gains->array[i].kind==Json::Kind::Number)
          p.equalizer.gains[i]=static_cast<float>(std::clamp(gains->array[i].num(),-9.0,9.0));
      }
    }
  }
  CabAudio cabAudio=CabAudio::Legacy;
  if(const auto* mode=layout.get("cabAudio")){
    const auto name=mode->str();
    if(name=="realistic")cabAudio=CabAudio::Realistic;
    else if(name=="5.1")cabAudio=CabAudio::Surround51;
    else if(name=="7.1")cabAudio=CabAudio::Surround71;
  }
  if(speakers.kind!=Json::Kind::Array || (!allowEmpty && speakers.array.empty()) || speakers.array.size()>16)
    throw std::runtime_error("Invalid number of speakers in preset");
  std::set<std::string> ids;
  for(const auto& x:speakers.array){
    Speaker s;s.id=x.at("id").str();s.name=x.at("name").str();s.cabAudio=cabAudio;s.coach=coach;
    s.channel=x.at("channel").str();s.type=x.at("type").str();
    const auto& a=x.at("position");
    if(a.kind!=Json::Kind::Array || a.array.size()!=3)throw std::runtime_error("Invalid speaker position");
    s.pos={float(a.array[0].num()),float(a.array[1].num()),float(a.array[2].num())};
    if(!std::isfinite(s.pos.x)||!std::isfinite(s.pos.y)||!std::isfinite(s.pos.z)||
       std::fabs(s.pos.x)>5||std::fabs(s.pos.y)>5||std::fabs(s.pos.z)>(coach?16:5))
      throw std::runtime_error("Speaker position out of bounds: "+s.id);
    s.gainDb=float(x.at("gainDb").num());
    if(s.gainDb < -100 || s.gainDb>12)throw std::runtime_error("Unsafe speaker gain: "+s.id);
    if(const auto* value=x.get("volumePercent")){
      const double percent=value->num();
      if(percent<0 || percent>100)throw std::runtime_error("Speaker volume must be between 0 and 100: "+s.id);
      s.volume=static_cast<float>(percent/100);
    }
    if(const auto* enabled=x.get("enabled")){
      if(enabled->kind!=Json::Kind::Boolean)throw std::runtime_error("Speaker enabled must be boolean");
      s.enabled=enabled->boolean;
    }
    if(s.channel!="L"&&s.channel!="R"&&s.channel!="M"&&s.channel!="FL"&&s.channel!="FR"&&s.channel!="FC"&&s.channel!="LFE"&&s.channel!="SL"&&s.channel!="SR"&&s.channel!="BL"&&s.channel!="BR")
      throw std::runtime_error("Unsupported channel: "+s.channel);
    if(!ids.insert(s.id).second)throw std::runtime_error("Duplicate speaker ID: "+s.id);
    p.speakers.push_back(std::move(s));
  }
  return p;
}
Vec3 relativePosition(const Vec3& speaker, const Vec3& listener,float yawDegrees){
  constexpr float pi=3.14159265358979323846f;
  const float a=yawDegrees*pi/180.0f,c=std::cos(a),s=std::sin(a);
  const Vec3 d{speaker.x-listener.x,speaker.y-listener.y,speaker.z-listener.z};
  return {c*d.x+s*d.z,d.y,-s*d.x+c*d.z};
}
float channelSample(const Speaker& s,float L,float R){
  if(s.type=="sub")return 0.0f; // No subwoofer feed without an explicit LFE source.
  if(s.channel=="L"||s.channel=="FL"||s.channel=="SL"||s.channel=="BL")return L;
  if(s.channel=="R"||s.channel=="FR"||s.channel=="SR"||s.channel=="BR")return R;
  if(s.channel=="LFE")return 0.0f; // Stereo material has no discrete LFE. No synthetic bass boost.
  return 0.5f*(L+R); // M and FC: very conservative phantom center.
}
float dbToAmplitude(float db){return std::pow(10.0f,db/20.0f);}
}
