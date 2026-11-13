// SPDX-License-Identifier: MIT
#include "wavert_model.hpp"
#include "wav.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
#include <stdexcept>
using namespace tas;
template<class F>void invalid(F&& f){bool threw=false;try{f();}catch(const std::invalid_argument&){threw=true;}assert(threw);}
static void exercise(uint16_t channels,uint32_t mask){
 StreamFormat fmt{channels,48000,mask,kStreamFloat32,0xC0FFEE};
 WaveRtModel m(fmt,1000);
 std::vector<float> pcm(200*channels),out(200*channels);
 for(size_t f=0;f<200;++f)for(size_t c=0;c<channels;++c)
  pcm[f*channels+c]=float(c+1)/float(channels+1);
 DriverPacketInfo p{fmt,0,200,0};
 m.submit(p,pcm);
 auto r=m.capture(0,200,out);
 assert(r.validFrames==200 && out==pcm);
 p.firstFrame=250;p.frameCount=200;
 m.submit(p,pcm);
 std::vector<float> gaps(250*channels);
 r=m.capture(200,250,gaps);
 assert(r.validFrames==250);
 for(size_t i=0;i<50*channels;++i)assert(gaps[i]==0);
 for(size_t i=50*channels;i<250*channels;++i)assert(gaps[i]==pcm[i-50*channels]);
 invalid([&]{m.submit(p,pcm);}); // overlapping render timestamp
 StreamFormat wrong=fmt;wrong.sourceEndpointToken=0xDEADBEEF;
 invalid([&]{m.submit(DriverPacketInfo{wrong,450,200,0},pcm);});
 p.firstFrame=450;p.frameCount=200;m.submit(p,pcm);
 p.firstFrame=650;m.submit(p,pcm);
 p.firstFrame=850;m.submit(p,pcm);
 // Circular history: frames [50,1050), so first 50 frames unavailable.
 std::vector<float> all(150*channels);
 r=m.capture(0,150,all);
 assert(r.overwrittenFrames==50 && r.validFrames==100 && r.notYetAvailableFrames==0);
 for(size_t i=0;i<50*channels;++i)assert(all[i]==0);
 r=m.capture(1040,110,all);
 assert(r.validFrames==10 && r.notYetAvailableFrames==100);
 // A large discontinuity must NOT leak previously played music.
 p.firstFrame=10000;m.submit(p,pcm);
 r=m.capture(9500,150,all);
 assert(r.overwrittenFrames==150 && r.validFrames==0);
 r=m.capture(10000,150,all);
 assert(r.validFrames==150);
 for(size_t i=0;i<150*channels;++i)assert(all[i]==pcm[i]);
}
int main(){
 exercise(2,kStereo);exercise(6,kFivePointOneSide);exercise(8,kSevenPointOne);
 std::cout<<"WaveRT render->loopback model: PCM identity, clock gaps, wrap, stale/future silence OK\n";
}
