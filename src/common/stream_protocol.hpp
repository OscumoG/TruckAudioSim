// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
namespace tas {
// Versioned 24-byte LITTLE-ENDIAN framing for a future multichannel virtual driver.
// After the header: repeating uint32_le frameCount (1..4800) + interleaved
// float32 PCM samples. A zero frameCount ends the stream.
// All frames are 48 kHz. Bytes 20..23 carry an optional device token.
// Zero keeps compatibility with previous releases; token is not authentication.
// The wire format is independent of Win32.
constexpr uint32_t kStreamRate=48000;
constexpr uint32_t kStreamFloat32=1;
constexpr uint32_t kMaxStreamPacketFrames=4800; // 100 ms at 48 kHz
constexpr size_t kStreamHeaderSize=24;
using StreamHeaderBytes=std::array<uint8_t,kStreamHeaderSize>;
using PacketLengthBytes=std::array<uint8_t,4>;
struct StreamFormat {
 uint16_t channels=2;
 uint32_t sampleRate=kStreamRate;
 uint32_t channelMask=0x3u;
 uint32_t sampleFormat=kStreamFloat32;
 uint32_t sourceEndpointToken=0; // optional identity token, 0=unknown
};
void validateStreamFormat(const StreamFormat& f);
StreamHeaderBytes encodeStreamHeader(const StreamFormat& f);
StreamFormat decodeStreamHeader(std::span<const uint8_t> bytes);
// Packet frame counts are explicitly little-endian. Zero marks clean end of stream.
PacketLengthBytes encodePacketFrames(uint32_t frames);
uint32_t decodePacketFrames(std::span<const uint8_t> bytes);
}
