// SPDX-License-Identifier: MIT
#include "audio_ring.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace tas;
int main(){
 try{
  for(size_t c:{2,3,6,8}){
   AudioFrameRing r(4,c);
   std::vector<float> a(6*c),b(6*c,-99);
   for(size_t i=0;i<a.size();++i)a[i]=float(i+1);
   if(r.write(a.data(),3)!=3||r.availableFrames()!=3)throw std::runtime_error("ring write");
   if(r.read(b.data(),2)!=2)throw std::runtime_error("ring read");
   for(size_t i=0;i<2*c;++i)if(b[i]!=a[i])throw std::runtime_error("ring value mismatch");
   // Wrap, then underflow.
   if(r.write(a.data(),4)!=3)throw std::runtime_error("ring overflow policy");
   if(r.read(b.data(),6)!=4)throw std::runtime_error("ring underrun count");
   for(size_t i=4*c;i<6*c;++i)if(b[i]!=0)throw std::runtime_error("ring underrun must be silent");
   if(r.availableFrames()!=0)throw std::runtime_error("ring not emptied");
  }
  // Threaded producer/consumer: fixed-size chunks, verify sequence continuity.
  AudioFrameRing q(1024,8);
  constexpr int n=100000;
  std::thread producer([&]{
   float frame[8];
   for(int i=0;i<n;){for(int c=0;c<8;++c)frame[c]=float(i*8+c);
    if(q.write(frame,1)==1)++i;else std::this_thread::yield();}
  });
  int next=0;float output[8];
  while(next<n){
   if(q.read(output,1)==0){std::this_thread::yield();continue;}
   for(int c=0;c<8;++c)if(output[c]!=float(next*8+c))throw std::runtime_error("ring thread order");
   ++next;
  }
  producer.join();
  std::cout<<"PASS: SPSC ring, 2/3/6/8 channels, underflow, overflow, concurrent order\n";
  return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
