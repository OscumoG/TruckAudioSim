// SPDX-License-Identifier: MIT
#include "wav.hpp"
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void put16(std::ostream& f,uint16_t v){const char b[2]={char(v&255),char(v>>8)};f.write(b,2);}
void put32(std::ostream& f,uint32_t v){const char b[4]={char(v&255),char((v>>8)&255),char((v>>16)&255),char((v>>24)&255)};f.write(b,4);}
void fixture(const std::filesystem::path& name,uint16_t channels,uint32_t mask,bool ext){
 std::ofstream f(name,std::ios::binary);
 const uint16_t block=channels*2;
 const uint32_t fmtLen=ext?40:16;
 const uint32_t dataLen=uint32_t(2*block);
 f.write("RIFF",4);put32(f,4+(8+fmtLen)+(8+dataLen));f.write("WAVE",4);
 f.write("fmt ",4);put32(f,fmtLen);put16(f,ext?0xfffe:1);put16(f,channels);
 put32(f,48000);put32(f,48000*block);put16(f,block);put16(f,16);
 if(ext){
  put16(f,22);put16(f,16);put32(f,mask);put32(f,1); // PCM GUID
  const char guidTail[12]={0,0,0x10,0,char(0x80),0,0,char(0xaa),0,0x38,char(0x9b),0x71};
  f.write(guidTail,12);
 }
 f.write("data",4);put32(f,dataLen);
 for(int frame=0;frame<2;++frame)for(int ch=0;ch<channels;++ch)put16(f,uint16_t(1000*(ch+1)*(frame+1)));
}
void test(uint16_t channels,uint32_t mask,bool ext){
 const auto path=std::filesystem::current_path()/("tas_wav_fixture_"+std::to_string(channels)+"_"+std::to_string(mask)+".wav");
 fixture(path,channels,mask,ext);
 try{
  tas::WavReader wav(path.string());
  if(wav.channels()!=channels||wav.channelMask()!=mask)throw std::runtime_error("WAV header decode mismatch");
  std::vector<float> samples;
  if(wav.readFrames(4,samples)!=2)throw std::runtime_error("WAV short read mismatch");
  if(samples.size()!=size_t(channels)*4)throw std::runtime_error("WAV output length mismatch");
  for(int ch=0;ch<channels;++ch){
   if(std::abs(samples[ch]-(1000.0f*(ch+1)/32768.0f))>0.00001f)throw std::runtime_error("WAV channel order mismatch");
  }
  if(samples[channels*2]!=0||samples[channels*3]!=0)throw std::runtime_error("WAV padding must be silent");
  if(wav.readFrames(4,samples)!=0)throw std::runtime_error("WAV EOF mismatch");
 }catch(...){std::filesystem::remove(path);throw;}
 std::filesystem::remove(path);
}

void testIncompletePcm(){
 const auto path=std::filesystem::current_path()/"tas_wav_incomplete_pcm.wav";
 fixture(path,2,tas::kStereo,false);
 try{
  // The header promises two stereo frames, but the second frame is truncated.
  std::filesystem::resize_file(path,std::filesystem::file_size(path)-1);
  try{
   tas::WavReader wav(path.string());
   std::vector<float> samples;
   wav.readFrames(4,samples);
   throw std::logic_error("Truncated WAV reported successful playback");
  }catch(const std::runtime_error& error){
   if(std::string(error.what())!="Truncated WAV PCM data")throw;
  }
  // Declaring exactly seven bytes also fails, because stereo PCM16 uses four bytes per frame.
  {
   std::fstream file(path,std::ios::in|std::ios::out|std::ios::binary);
   file.seekp(40);
   put32(file,7);
   if(!file)throw std::logic_error("Failed to write malformed WAV fixture");
  }
  try{
   tas::WavReader wav(path.string());
   throw std::logic_error("WAV accepted an incomplete PCM frame");
  }catch(const std::runtime_error& error){
   if(std::string(error.what())!="WAV data size does not contain whole PCM frames")throw;
  }
 }catch(...){std::filesystem::remove(path);throw;}
 std::filesystem::remove(path);
}
}
int main(){
 try{
  test(2,tas::kStereo,false);
  test(2,tas::kStereo,true);
  test(3,tas::kTwoPointOne,true);
  test(6,tas::kFivePointOneSide,true);
  test(6,tas::kFivePointOneBack,true);
  test(8,tas::kSevenPointOne,true);
  testIncompletePcm();
  std::cout<<"PASS: WAV channel masks, sample ordering and incomplete PCM rejection\n";
  return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
