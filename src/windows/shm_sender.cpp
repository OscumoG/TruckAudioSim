// SPDX-License-Identifier: MIT
// Test producer for shared-memory PCM. Not a hardware or virtual audio driver.
#include <windows.h>
#include "shared_mapping.hpp"
#include "wav.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
int main(int argc,char**argv){
 try{
  std::string wav;
  for(int i=1;i<argc;++i){
   if(std::string(argv[i])=="--help"){
    std::cout<<"tas_shm_sender.exe --wav audio_48k.wav\n"
               "Test-only producer for tas_shm_bridge, not Spotify capture or driver.\n";
    return 0;
   }
   if(std::string(argv[i])=="--wav"&&i+1<argc)wav=argv[++i];
   else throw std::invalid_argument("Usage: tas_shm_sender.exe --wav file.wav");
  }
  if(wav.empty())throw std::invalid_argument("Missing --wav file.wav");
  tas::WavReader input(wav);
  tas::SharedPcmMapping mapping(false);
  tas::SharedPcmProducerLock lock;
  const auto f=mapping.buffer().format;
  if(input.channels()!=f.channels||input.channelMask()!=f.channelMask||input.sampleRate()!=f.sampleRate)
   throw std::runtime_error("WAV channels/mask/sample rate do not match shared PCM endpoint");
  std::vector<float> pcm;
  uint64_t total=0,accepted=0;
  const auto start=std::chrono::steady_clock::now();
  for(;;){
   const auto frames=input.readFrames(480,pcm);
   if(!frames)break;
   accepted+=mapping.buffer().push(pcm,frames);
   total+=frames;
   std::this_thread::sleep_until(start+std::chrono::duration<double>(double(total)/f.sampleRate));
  }
  std::cout<<"Sent="<<accepted<<" / "<<total<<" frames, dropped="<<total-accepted<<"\n";
  return total==accepted?0:2;
 }catch(const std::exception&e){std::cerr<<"tas_shm_sender: "<<e.what()<<"\n";return 1;}
}
