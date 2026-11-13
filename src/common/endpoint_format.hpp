// SPDX-License-Identifier: MIT
#pragma once
#include "stream_protocol.hpp"
#include <cstdint>
namespace tas {
enum class EndpointSample {Float32, PCM16, Unsupported};
struct EndpointFormat {
 uint16_t channels=0;
 uint32_t sampleRate=0;
 uint32_t channelMask=0;
 uint16_t bits=0;
 uint16_t validBits=0;
 uint16_t blockAlign=0;
 uint32_t bytesPerSecond=0;
 EndpointSample samples=EndpointSample::Unsupported;
};
struct EndpointAcceptance {
 StreamFormat stream;
 bool convertPCM16=false;
};
// Validates the capture stream delivered to the 48 kHz PCM pipeline.
EndpointAcceptance acceptEndpointFormat(const EndpointFormat& format);
}
