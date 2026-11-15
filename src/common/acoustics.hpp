// SPDX-License-Identifier: MIT
#pragma once
#include "pose.hpp"
#include <array>
#include <span>
#include <vector>

namespace tas {

class CabinAcoustics {
 public:
  void reset();
  void process(std::span<float> samples, const Acoustics& room, const Speaker& speaker,
               const Pose& pose, float rate);
 private:
  struct Tap { float delay=1, gain=0, brightness=0; };
  struct DelayLine { std::vector<float> buffer; size_t cursor=0; float low=0; };
  std::array<Tap,24> taps_{};
  std::array<DelayLine,4> diffuse_{};
  std::vector<std::array<float,2>> history_;
  size_t cursor_=0;
  float damped_=0, bright_=0, filtered_=0, loss_=1, occlusion_=0, wet_=0;
};

class SpeakerTest {
 public:
  void prepare(unsigned sequence, unsigned rate);
  void render(std::span<float> output, bool subwoofer) const;
  void advance(size_t count) { cursor_ += count; }
 private:
  unsigned sequence_=0, rate_=48000;
  size_t cursor_=0, length_=0;
};
}
