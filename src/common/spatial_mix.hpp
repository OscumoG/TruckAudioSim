// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "profile.hpp"

namespace tas {
// Statistics are measured immediately before and after the protective ceiling.
struct SpatialMixStats {
 uint64_t clippedSamples = 0;
 float inputPeak = 0.0f;
 float outputPeak = 0.0f;
};

class SpeakerFilter {
 struct Section {
  float b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
  float process(float sample);
  float magnitude(double cosine) const;
 };
 std::array<Section,4> sections_{};
 size_t count_=0;
public:
 void configure(const Speaker& speaker,float sampleRate);
 void reset();
 float process(float sample);
 float magnitude(double cosine) const;
};

// Render one speaker into a mono object buffer. Source frames are interleaved;
// validFrames can be less than outputFrames (for example during WASAPI startup
// or a temporary loopback underrun). The rest MUST be silence, never stale PCM.
// No allocation, and safe when validFrames == 0 and interleaved is empty.
SpatialMixStats mixSpatialObject(const Speaker& speaker,
                                 const std::vector<float>& interleaved,
                                 size_t validFrames, size_t outputFrames,
                                 uint16_t channels, uint32_t channelMask,
                                 float gain, bool electronicsOn, float* mono,
                                 SpeakerFilter* filter=nullptr);
}
