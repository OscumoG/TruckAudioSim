// SPDX-License-Identifier: MIT
#pragma once
#include "stream_protocol.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tas {
// Portable, versioned SPECIFICATION for a future SysVAD WaveRT adapter.
// This is NOT an IOCTL or an installed driver. No pointers or C++ ABI cross
// the proposed kernel/user-mode boundary. All integers/floats are LE.
//  0 magic (u32)   4 version (u16)  6 headerSize (u16)
//  8 sampleRate    12 channelMask  16 firstFrame (u64)
// 24 frameCount    28 channels (u16) 30 sampleFormat (u16)
// 32 sourceToken   36 flags (u32)  40 ... PCM float32 interleaved
constexpr uint32_t kDriverPacketMagic=0x31534154u; // 'TAS1' in LE bytes
constexpr uint16_t kDriverPacketVersion=1;
constexpr size_t kDriverHeaderBytes=40;
constexpr uint32_t kMaxDriverPacketFrames=960; // <=20ms @ 48kHz
constexpr uint32_t kDriverPacketFloat32=1;
using DriverHeaderBytes=std::array<uint8_t,kDriverHeaderBytes>;
struct DriverPacketInfo {
 StreamFormat format{};
 uint64_t firstFrame=0;
 uint32_t frameCount=0;
 uint32_t flags=0; // zero until flags acquire a documented meaning
};

DriverHeaderBytes encodeDriverHeader(const DriverPacketInfo& info);
DriverPacketInfo decodeDriverHeader(std::span<const uint8_t> bytes);
// Serializes header and payload without memcpy of structs, including on
// big-endian hosts. Non-finite sample values are rejected, never amplified.
std::vector<uint8_t> encodeDriverPacket(const DriverPacketInfo& info,
                                         std::span<const float> samples);
struct DecodedDriverPacket {
 DriverPacketInfo info;
 std::vector<float> samples;
};
DecodedDriverPacket decodeDriverPacket(std::span<const uint8_t> bytes);
} // namespace tas
