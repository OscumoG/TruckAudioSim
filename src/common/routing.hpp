// SPDX-License-Identifier: MIT
#pragma once
#include "profile.hpp"
#include "wav.hpp"
#include <cstddef>
#include <cstdint>
namespace tas {
// Routes source channels to cab speaker objects without EQ, bass extraction,
// compression, dynamics or upmix. 5.1/7.1 signals remain independent.
// A Trucker AUX L/R/M preset is mapped by physical speaker position when the
// source is discrete; with stereo, the original L/R/M mix is preserved.
float sourceForSpeaker(const Speaker& speaker, const float* frame,
                       uint16_t inputChannels, uint32_t channelMask);
// Gives each speaker a deterministic source-channel bit; zero means silent.
uint32_t sourceChannelBit(const Speaker& speaker,uint16_t channels,uint32_t channelMask);
}
