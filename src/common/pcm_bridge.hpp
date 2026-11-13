// SPDX-License-Identifier: MIT
#pragma once
#include "stream_protocol.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tas {
// Cross-process *user-mode* transport prototype for a future virtual endpoint.
// The shared structure is consumed ONLY by processes built with this C++ ABI.
// Do not map this structure in a kernel driver; a future kernel transport must
// have its own versioned IOCTL contract and security/lifetime validation.
// Verify that a user-mode live producer is the source selected by the bridge.
// The source token is a routing guard, NOT an authentication mechanism.
void validatePcmProducerFormat(const StreamFormat& bridge, const StreamFormat& producer);
constexpr uint32_t kBridgeMagic = 0x31425054; // 'TPB1' in little-endian memory
constexpr uint32_t kBridgeVersion = 1;
constexpr uint32_t kBridgeFrames = 8192; // 170.7ms max @ 48k; target 20-40ms
constexpr uint32_t kBridgeChannels = 8;

struct alignas(64) SharedPcmBridge {
 std::atomic<uint32_t> ready{0};
 uint32_t version=0;
 StreamFormat format{};
 uint32_t capacityFrames=kBridgeFrames;
 uint32_t reserved[7]{};
 alignas(64) std::atomic<uint64_t> writeCursor{0};
 alignas(64) std::atomic<uint64_t> readCursor{0};
 alignas(64) std::atomic<uint64_t> droppedFrames{0};
 alignas(64) std::atomic<uint64_t> paddedFrames{0};
 alignas(64) float samples[kBridgeFrames*kBridgeChannels]{};

 void initialize(StreamFormat f);
 // Single-producer, single-consumer. Never overwrite unread frames.
 // Returns accepted frame count; excess frames are dropped from the incoming block.
 size_t push(std::span<const float> interleaved, size_t frames);
 // Returns real frames read, fills the rest with zero, and counts underruns.
 size_t pop(std::span<float> interleaved, size_t frames);
 uint64_t bufferedFrames() const;
 void validate() const;
};
static_assert(alignof(SharedPcmBridge)>=64);
}
