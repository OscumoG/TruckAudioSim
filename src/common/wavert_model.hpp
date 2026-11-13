// SPDX-License-Identifier: MIT
#pragma once
#include "driver_wire.hpp"
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <vector>

namespace tas {
// TEST MODEL for a single, *already mixed* WaveRT render pin. Not kernel code.
// The Windows shared-mode audio engine handles mixing before these frames.
// Absolute frame indices preserve timing. A circular history contains the
// newest N frames; a late capture gets silence for overwritten/missing frames.
// This model is used to test render->loopback routing without synthetic tones.
struct LoopbackReadResult {
 size_t validFrames=0;
 size_t overwrittenFrames=0;
 size_t notYetAvailableFrames=0;
};
class WaveRtModel {
 mutable std::mutex mutex_;
 StreamFormat format_;
 size_t capacity_;
 std::vector<float> ring_;
 uint64_t nextFrame_=0;
 uint64_t oldestFrame_=0;
public:
 explicit WaveRtModel(StreamFormat fmt,size_t capacityFrames=8192);
 StreamFormat format() const noexcept {return format_;}
 size_t capacity() const noexcept {return capacity_;}
 uint64_t nextFrame() const;
 // Monotonic presentation-frame index, no overlap. A gap is recorded as silence.
 void submit(const DriverPacketInfo& info,std::span<const float> pcm);
 // Reads exactly `frames` of PCM, filling missing history/future with silence.
 LoopbackReadResult capture(uint64_t firstFrame,size_t frames,std::span<float> out) const;
};
} // namespace tas
