// SPDX-License-Identifier: MIT
#include "wav_writer.hpp"
#include "wav.hpp"
#include <cmath>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <vector>
int main(){
 try{
  const char* path="tas_writer_test.wav";
  for(const auto c:{uint16_t(2),uint16_t(3),uint16_t(6),uint16_t(8)}){
   uint32_t mask=c==2?tas::kStereo:c==3?tas::kTwoPointOne:c==6?tas::kFivePointOneSide:tas::kSevenPointOne;
   tas::WavWriter wr(path,{c,48000,mask,1});
   std::vector<float> f(320*c);
   for(size_t i=0;i<f.size();++i)f[i]=std::sin(float(i)*0.13f)*0.15f;
   wr.write(f.data(),320);wr.finalize();
   tas::WavReader r(path);
   if(r.channels()!=c||r.channelMask()!=mask)throw std::runtime_error("format mismatch");
   std::vector<float> got;
   if(r.readFrames(320,got)!=320)throw std::runtime_error("frame count mismatch");
   for(size_t i=0;i<f.size();++i)if(std::fabs(f[i]-got[i])>1e-7f)throw std::runtime_error("sample mismatch");
  }
  std::remove(path);std::cout<<"PASS WAVEFORMATEXTENSIBLE writer roundtrip\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
