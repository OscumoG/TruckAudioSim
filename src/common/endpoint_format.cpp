// SPDX-License-Identifier: MIT
#include "endpoint_format.hpp"
#include "wav.hpp"
#include <stdexcept>
namespace tas {
EndpointAcceptance acceptEndpointFormat(const EndpointFormat& fmt){
 if(fmt.sampleRate!=kStreamRate)throw std::runtime_error("Capture stream must deliver 48000 Hz");
 if(fmt.samples!=EndpointSample::Float32 && fmt.samples!=EndpointSample::PCM16)
  throw std::runtime_error("Only float32 and 16-bit PCM endpoint loopback are supported");
 const uint16_t expectedBits=fmt.samples==EndpointSample::Float32?32:16;
 if(fmt.bits!=expectedBits || fmt.validBits!=expectedBits)
  throw std::runtime_error("Unsupported endpoint container or valid-bit count");
 if(fmt.blockAlign!=fmt.channels*(fmt.bits/8) ||
    fmt.bytesPerSecond!=uint32_t(fmt.blockAlign)*fmt.sampleRate)
  throw std::runtime_error("Invalid endpoint block align or bytes/second");
 const StreamFormat result{fmt.channels,fmt.sampleRate,fmt.channelMask,kStreamFloat32};
 validateStreamFormat(result);
 return {result,fmt.samples==EndpointSample::PCM16};
}
}
