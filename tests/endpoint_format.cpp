#include "endpoint_format.hpp"
#include "wav.hpp"
#include <cassert>
#include <stdexcept>
#include <iostream>
using namespace tas;
namespace {
EndpointFormat good(uint16_t ch,uint32_t mask,EndpointSample type=EndpointSample::Float32){
 const uint16_t bits=type==EndpointSample::Float32?32:16;
 return {ch,48000,mask,bits,bits,static_cast<uint16_t>(ch*bits/8),uint32_t(48000*ch*bits/8),type};
}
void rejects(EndpointFormat format){
 bool rejected=false;
 try{(void)acceptEndpointFormat(format);}catch(const std::runtime_error&){rejected=true;}
 assert(rejected);
}
}
int main(){
 for(auto [ch,mask]:{std::pair<uint16_t,uint32_t>{2,kStereo},{3,kTwoPointOne},
                    {6,kFivePointOneSide},{6,kFivePointOneBack},{8,kSevenPointOne}}){
  const auto f=acceptEndpointFormat(good(ch,mask));
  assert(f.stream.channels==ch && f.stream.channelMask==mask && !f.convertPCM16);
  assert(acceptEndpointFormat(good(ch,mask,EndpointSample::PCM16)).convertPCM16);
 }
 auto f=good(8,kSevenPointOne);f.sampleRate=44100;rejects(f);
 f=good(6,0);rejects(f);
 f=good(2,kStereo);f.blockAlign=1;rejects(f);
 f=good(2,kStereo);f.samples=EndpointSample::Unsupported;rejects(f);
 f=good(8,kSevenPointOne);f.bits=24;rejects(f);
 f=good(8,kSevenPointOne);f.validBits=24;rejects(f);
 std::cout<<"Endpoint format checks passed\n";
}
