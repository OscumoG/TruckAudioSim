// SPDX-License-Identifier: MIT
#include "pcm_bridge.hpp"
#include "wav.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace tas;
static void producerIdentity(){
 StreamFormat bridge{8,48000,kSevenPointOne,kStreamFloat32,123456};
 validatePcmProducerFormat(bridge,bridge);
 auto expectReject=[&](StreamFormat mismatch){
  bool thrown=false;
  try{validatePcmProducerFormat(bridge,mismatch);}catch(const std::exception&){thrown=true;}
  assert(thrown);
 };
 auto other=bridge;other.sourceEndpointToken=999;expectReject(other);
 other=bridge;other.sourceEndpointToken=0;expectReject(other);
 other=bridge;other.channels=2;other.channelMask=kStereo;expectReject(other);
 other=bridge;other.sampleRate=44100;expectReject(other);
 other=bridge;other.channelMask=kFivePointOneBack;expectReject(other);
 other=bridge;other.sourceEndpointToken=123456;other.sampleFormat=0;expectReject(other);
 bridge.sourceEndpointToken=0;expectReject(bridge);
}

int main(){producerIdentity();std::cout<<"Live source format and endpoint identity checks passed\n";}
