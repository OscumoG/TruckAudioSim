// SPDX-License-Identifier: MIT
#pragma once
#include "profile.hpp"
#include <cstdint>
#include <vector>
namespace tas {
// A conservative static peak bound across all source channels: no auto-EQ,
// bass boost or dynamic compression; prevents speaker duplication from raising
// peak gain merely because a preset uses many virtual speakers.
std::vector<float> neutralVoiceGains(const std::vector<Speaker>& speakers,
                                     uint16_t channels,uint32_t mask,float peakCeiling=0.75f,
                                     float sampleRate=0.0f);
// Compensate for the desktop spatial mix's lower listening level at 100%.
inline constexpr float kDesktopReferenceGain=1.5f;
}
