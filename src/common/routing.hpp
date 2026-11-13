// SPDX-License-Identifier: MIT
#pragma once
#include "profile.hpp"
#include <array>
#include <cstdint>

namespace tas {
using SpeakerRoute = std::array<float,8>;

// Coefficients follow the WAVEFORMATEXTENSIBLE channel order.
SpeakerRoute speakerRoute(const Speaker& speaker,uint16_t channels,uint32_t channelMask);
float sampleForRoute(const SpeakerRoute& route,const float* frame,uint16_t channels);
float sourceForSpeaker(const Speaker& speaker,const float* frame,uint16_t channels,uint32_t channelMask);
}
