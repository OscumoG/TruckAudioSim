// SPDX-License-Identifier: MIT
#include "driver_wire.hpp"
#include "wav.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace tas;
template<typename F>static void invalid(F&& fn){
 bool failed=false;
 try{fn();}catch(const std::exception&){failed=true;}
 assert(failed);
}
static void test(uint16_t channels,uint32_t mask){
 StreamFormat fmt{channels,48000,mask,kStreamFloat32,0xA023};
 DriverPacketInfo info{fmt,UINT64_C(4294967317),480,0};
 std::vector<float> pcm(size_t(info.frameCount)*channels);
 for(uint32_t f=0;f<info.frameCount;++f)
  for(uint16_t ch=0;ch<channels;++ch)
   pcm[size_t(f)*channels+ch]=float((f+ch*7)%200)/150.f-0.6f;
 auto encoded=encodeDriverPacket(info,pcm);
 assert(encoded.size()==kDriverHeaderBytes+pcm.size()*4);
 assert(encoded[0]=='T'&&encoded[1]=='A'&&encoded[2]=='S'&&encoded[3]=='1');
 auto decoded=decodeDriverPacket(encoded);
 assert(decoded.info.firstFrame==info.firstFrame);
 assert(decoded.info.format.channelMask==mask);
 assert(decoded.info.format.sourceEndpointToken==fmt.sourceEndpointToken);
 assert(decoded.samples==pcm);
 auto broken=encoded;
 broken[0]^=1;invalid([&]{(void)decodeDriverPacket(broken);});
 broken=encoded;broken[4]=55;invalid([&]{(void)decodeDriverPacket(broken);});
 broken=encoded;broken[36]=0x01;invalid([&]{(void)decodeDriverPacket(broken);});
 broken=encoded;broken.pop_back();invalid([&]{(void)decodeDriverPacket(broken);});
 broken=encoded;broken.push_back(0);invalid([&]{(void)decodeDriverPacket(broken);});
 broken=encoded;broken[28]=9;invalid([&]{(void)decodeDriverPacket(broken);});
 auto poison=pcm;poison[0]=std::numeric_limits<float>::infinity();
 invalid([&]{(void)encodeDriverPacket(info,poison);});
 auto oversize=info;oversize.frameCount=kMaxDriverPacketFrames+1;
 invalid([&]{(void)encodeDriverHeader(oversize);});
 auto overflow=info;overflow.firstFrame=UINT64_MAX;
 invalid([&]{(void)encodeDriverHeader(overflow);});
 auto badFormat=info;badFormat.format.channelMask=0;
 invalid([&]{(void)encodeDriverHeader(badFormat);});
}
int main(){
 test(2,kStereo);test(3,kTwoPointOne);test(6,kFivePointOneSide);
 test(6,kFivePointOneBack);test(8,kSevenPointOne);
 std::cout<<"Driver wire: 2.0/2.1/5.1/7.1, LE, malformed packets, 64-bit clock OK\n";
}
