// SPDX-License-Identifier: MIT
#include "binaural.hpp"
#include "channel_layout.hpp"
#include "wav.hpp"
#include "wav_writer.hpp"
#include <pulse/simple.h>
#include <pulse/error.h>
#include <poll.h>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
constexpr uint32_t kRate = 48000;
constexpr uint32_t kBlock = tas::kBinauralBlock;
using PulseStream = std::unique_ptr<pa_simple, decltype(&pa_simple_free)>;

struct Options {
 std::string input, output, profile, pose, wav, render;
 std::string sofa = "/usr/share/libmysofa/default.sofa";
 uint32_t mask = tas::kStereo;
 bool diagnostic = false;
};

Options options(int argc, char** argv) {
 Options out;
 for (int i=1; i<argc; ++i) {
  const std::string_view arg=argv[i];
  if (arg=="--channel-test") { out.diagnostic=true; continue; }
  if (i+1==argc) throw std::runtime_error("Missing value after " + std::string(arg));
  const std::string value=argv[++i];
  if (arg=="--input") out.input=value;
  else if (arg=="--output") out.output=value;
  else if (arg=="--preset") out.profile=value;
  else if (arg=="--pose-file") out.pose=value;
  else if (arg=="--sofa") out.sofa=value;
  else if (arg=="--wav") out.wav=value;
  else if (arg=="--render") out.render=value;
  else if (arg=="--mask") {
   size_t size=0;
   const auto mask=std::stoul(value,&size,0);
   if (size!=value.size() || mask>UINT32_MAX) throw std::runtime_error("Invalid channel mask");
   out.mask=static_cast<uint32_t>(mask);
  } else throw std::runtime_error("Unknown option " + std::string(arg));
 }
 if (out.input.empty()==out.wav.empty() || out.output.empty()==out.render.empty() ||
     (!out.render.empty() && out.wav.empty()) ||
     (out.diagnostic ? out.wav.empty() : out.profile.empty()))
  throw std::runtime_error("Use --preset with --input MONITOR and --output SINK, or --wav WAV and --render WAV. --channel-test requires WAV.");
 if (!out.input.empty() && out.input==out.output+".monitor") throw std::runtime_error("Capture and playback would form a feedback loop");
 return out;
}

pa_channel_map channelMap(uint32_t mask) {
 constexpr std::array positions{PA_CHANNEL_POSITION_FRONT_LEFT, PA_CHANNEL_POSITION_FRONT_RIGHT,
  PA_CHANNEL_POSITION_FRONT_CENTER, PA_CHANNEL_POSITION_LFE, PA_CHANNEL_POSITION_REAR_LEFT,
  PA_CHANNEL_POSITION_REAR_RIGHT, PA_CHANNEL_POSITION_SIDE_LEFT, PA_CHANNEL_POSITION_SIDE_RIGHT};
 constexpr std::array bits{1u,2u,4u,8u,16u,32u,512u,1024u};
 if (mask!=tas::kStereo && mask!=tas::kTwoPointOne && mask!=tas::kFivePointOneBack &&
     mask!=tas::kFivePointOneSide && mask!=tas::kSevenPointOne)
  throw std::runtime_error("Capture requires stereo, 2.1, 5.1 or 7.1");
 pa_channel_map map{};
 for (size_t i=0; i<bits.size(); ++i) if (mask & bits[i]) map.map[map.channels++]=positions[i];
 return map;
}

PulseStream connect(const std::string& device, uint32_t mask, bool capture) {
 const auto map=channelMap(mask);
 const pa_sample_spec spec{PA_SAMPLE_FLOAT32NE, kRate, map.channels};
 const uint32_t bytes=sizeof(float)*map.channels;
 const pa_buffer_attr buffers{bytes*kBlock*16, bytes*kBlock*4, UINT32_MAX, UINT32_MAX, bytes*kBlock};
 int error=0;
 PulseStream stream(pa_simple_new(nullptr,"TruckAudioSim",capture?PA_STREAM_RECORD:PA_STREAM_PLAYBACK,
     device.c_str(),capture?"Capture music":"Binaural headphones",&spec,&map,&buffers,&error),pa_simple_free);
 if (!stream) throw std::runtime_error(device + ": " + pa_strerror(error));
 return stream;
}

bool shouldStop() {
 pollfd input{0,POLLIN,0};
 return poll(&input,1,0)>0 && (input.revents&(POLLHUP|POLLERR));
}

std::string readProfile(const std::string& file) {
 std::ifstream in(file, std::ios::binary);
 return {std::istreambuf_iterator<char>(in), {}};
}
}

int main(int argc, char** argv) {
 try {
  const auto opt=options(argc,argv);
  std::unique_ptr<tas::WavReader> wav;
  if (!opt.wav.empty()) wav=std::make_unique<tas::WavReader>(opt.wav);
  const uint32_t mask=wav?wav->channelMask():opt.mask;
  const uint32_t rate=wav?wav->sampleRate():kRate;
  const uint16_t channels=wav?wav->channels():channelMap(mask).channels;
  if (rate!=kRate) throw std::runtime_error("The binaural engine requires a 48 kHz WAV");
  tas::Binaural engine(opt.sofa.c_str(),rate,channels,mask,opt.diagnostic);
  std::string profileText;
  if (opt.diagnostic) engine.setProfile(tas::makeChannelTestProfile(channels,mask));
  else {
   profileText=readProfile(opt.profile);
   engine.setProfile(tas::parseProfileText(profileText,true));
  }
  tas::Pose pose{}, target{};
  if (!opt.pose.empty()) pose=target=tas::loadPoseFile(opt.pose);
  if (opt.diagnostic) pose.head=target.head={0,0,0};
  PulseStream capture(nullptr,pa_simple_free), playback(nullptr,pa_simple_free);
  if (!wav) capture=connect(opt.input,mask,true);
  if (!opt.output.empty()) playback=connect(opt.output,tas::kStereo,false);
  std::unique_ptr<tas::WavWriter> output;
  if (!opt.render.empty()) output=std::make_unique<tas::WavWriter>(opt.render,tas::StreamFormat{2,rate,tas::kStereo});
  std::cout<<"Desktop PCM: "<<channels<<" ch · binaural stereo · "<<rate<<" Hz\n"<<std::flush;
  std::vector<float> pcm(kBlock*channels);
  std::array<float,kBlock*2> stereo{};
  auto lastPose=std::chrono::steady_clock::time_point{};
  while (wav || !shouldStop()) {
   const auto now=std::chrono::steady_clock::now();
   if (now-lastPose>=std::chrono::milliseconds(15)) {
    if (!opt.pose.empty()) {
     try { target=tas::loadPoseFile(opt.pose); } catch (const std::exception&) {}
    }
    if (!opt.profile.empty() && playback) {
     const auto text=readProfile(opt.profile);
     if (text!=profileText) {
      try { engine.setProfile(tas::parseProfileText(text,true)); profileText=text; } catch (const std::exception&) {}
     }
    }
    lastPose=now;
   }
   auto audible=target;
   if (!audible.electronicsOn) { audible.volume=0; audible.electronicsOn=true; }
   pose=tas::smoothPose(pose,audible,0.12f);
   size_t frames=kBlock;
   int error=0;
   if (wav) frames=wav->readFrames(kBlock,pcm);
   else if (pa_simple_read(capture.get(),pcm.data(),pcm.size()*sizeof(float),&error)<0)
    throw std::runtime_error(std::string("Capture: ")+pa_strerror(error));
   if (!frames) break;
   engine.render(pcm,frames,pose,stereo.data());
   if (output) output->write(stereo.data(),frames);
   else if (pa_simple_write(playback.get(),stereo.data(),frames*2*sizeof(float),&error)<0)
    throw std::runtime_error(std::string("Playback: ")+pa_strerror(error));
  }
  if (output) output->finalize();
  if (playback && wav) pa_simple_drain(playback.get(),nullptr);
  return 0;
 } catch (const std::exception& error) {
  std::cerr<<"Error: "<<error.what()<<"\n";
  return 1;
 }
}
