// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
namespace tas {
// Standard WAVEFORMATEXTENSIBLE speaker masks (Windows ksmedia.h values).
constexpr uint32_t kStereo = 0x00000003u; // FL FR
constexpr uint32_t kTwoPointOne = 0x0000000Bu; // FL FR LFE (nonstandard device topology, valid WAV mask)
constexpr uint32_t kFivePointOneBack = 0x0000003Fu; // FL FR FC LFE BL BR
constexpr uint32_t kFivePointOneSide = 0x0000060Fu; // FL FR FC LFE SL SR
constexpr uint32_t kSevenPointOne = 0x0000063Fu; // FL FR FC LFE BL BR SL SR

class WavReader {
 std::ifstream in_;
 uint16_t channels_=0,format_=0,bits_=0;
 uint32_t sampleRate_=0,channelMask_=0,dataRemaining_=0;
 public:
 explicit WavReader(const std::string& path);
 // Always returns a full zero-padded block in dest; return value is frames read from disk.
 // A short read before the declared end of PCM data is an error.
 size_t readFrames(size_t frames, std::vector<float>& dest);
 uint16_t channels()const{return channels_;}
 uint32_t channelMask()const{return channelMask_;}
 uint32_t sampleRate()const{return sampleRate_;}
};
}
