// SPDX-License-Identifier: MIT
#pragma once
#include "pose.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace tas {
inline constexpr size_t kBinauralBlock = 256;

struct BinauralState;
class Binaural {
 public:
  Binaural(const char* sofa, uint32_t rate, uint16_t channels, uint32_t mask, bool discrete);
  ~Binaural();
  void setProfile(const Profile& profile);
  void render(const std::vector<float>& pcm, size_t frames, const Pose& pose, float* stereo);
 private:
  std::unique_ptr<BinauralState> state_;
};
}
