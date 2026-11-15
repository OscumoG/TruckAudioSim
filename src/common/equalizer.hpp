// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <span>

namespace tas {
struct Equalizer {
 bool enabled=false;
 std::array<float,5> gains{};
};

class EqualizerFilter {
 public:
  void process(std::span<float> pcm, const Equalizer& equalizer, float rate);
 private:
  struct Band {
   std::array<float,5> coefficients{1,0,0,0,0}, target{1,0,0,0,0};
   float z1=0, z2=0;
  };
  std::array<Band,5> bands_{};
  std::array<float,5> gains_{};
  float rate_=0, headroom_=1, targetHeadroom_=1;
  unsigned transition_=0;
  bool active_=false;
};
}
