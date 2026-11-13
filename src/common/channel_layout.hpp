// SPDX-License-Identifier: MIT
#pragma once
#include "profile.hpp"
#include <cstdint>

namespace tas {
// Diagnostic layout: one fixed-position spatial object per PCM channel,
// in WAVEFORMATEXTENSIBLE bit order. No stereo upmix, matrixing or LFE synthesis.
// Throws for an unsupported/mismatched channel count and speaker mask.
Profile makeChannelTestProfile(uint16_t channels, uint32_t channelMask);
}
