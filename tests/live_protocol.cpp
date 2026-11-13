// SPDX-License-Identifier: MIT
#include "stream_protocol.hpp"
#include "wav.hpp"
#include <array>
#include <string>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool ok, const char* message){if(!ok)throw std::runtime_error(message);}
}
int main(){
 for(auto [channels,mask]:{std::pair{2, tas::kStereo}, {3,tas::kTwoPointOne},
      {6,tas::kFivePointOneSide},{6,tas::kFivePointOneBack},{8,tas::kSevenPointOne}}){
  const tas::StreamFormat fmt{static_cast<uint16_t>(channels),48000,mask,1,0x89abcdefu};
  auto wire=tas::encodeStreamHeader(fmt);auto round=tas::decodeStreamHeader(wire);
  require(round.channels==fmt.channels&&round.channelMask==fmt.channelMask&&round.sourceEndpointToken==fmt.sourceEndpointToken,"Header round-trip failed");
  auto oldWire=wire;oldWire[20]=oldWire[21]=oldWire[22]=oldWire[23]=0;
  require(tas::decodeStreamHeader(oldWire).sourceEndpointToken==0,"Old zero-reserved header should remain valid");
  wire[0]='X';bool rejected=false;
  try{(void)tas::decodeStreamHeader(wire);}catch(const std::runtime_error&){rejected=true;}
  require(rejected,"Malformed header was accepted");
 }
 std::array<uint8_t,8> truncated{};bool rejected=false;
 try{(void)tas::decodeStreamHeader(truncated);}catch(const std::runtime_error&){rejected=true;}
 require(rejected,"Truncated header was accepted");
 for(auto count:{uint32_t{0},uint32_t{1},uint32_t{480},tas::kMaxStreamPacketFrames}){
  auto encoded=tas::encodePacketFrames(count);
  require(tas::decodePacketFrames(encoded)==count,"PCM chunk roundtrip failed");
 }
 for(auto count:{tas::kMaxStreamPacketFrames+1,UINT32_MAX}){
  bool refused=false;
  try{(void)tas::encodePacketFrames(count);}catch(const std::runtime_error&){refused=true;}
  require(refused,"Oversized PCM packet was accepted");
 }
 std::array<uint8_t,3> shortPacket{};
 try{(void)tas::decodePacketFrames(shortPacket);throw std::runtime_error("Truncated packet was accepted");}
 catch(const std::runtime_error& e){require(std::string(e.what())!="Truncated packet was accepted","Truncated packet was accepted");}
 std::cout<<"Live PCM header and bounded packet validation OK\n";
}
