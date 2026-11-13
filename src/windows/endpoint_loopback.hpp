// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <memory>
#include <vector>
#include "audio_ring.hpp"
#include "endpoint_format.hpp"

namespace tas {
EndpointAcceptance describeEndpointFormat(const WAVEFORMATEX& mix);

class EndpointLoopback {
public:
 explicit EndpointLoopback(IMMDevice* device);
 ~EndpointLoopback();
 void start();
 size_t readFrames(size_t frames, std::vector<float>& samples);
 const StreamFormat& format() const noexcept { return format_.stream; }
 uint64_t droppedFrames() const noexcept { return droppedFrames_; }
 uint64_t discontinuities() const noexcept { return discontinuities_; }

private:
 Microsoft::WRL::ComPtr<IAudioClient> audio_;
 Microsoft::WRL::ComPtr<IAudioCaptureClient> capture_;
 EndpointAcceptance format_{};
 std::unique_ptr<AudioFrameRing> ring_;
 std::vector<float> converted_;
 UINT32 bufferFrames_ = 0;
 uint64_t droppedFrames_ = 0;
 uint64_t discontinuities_ = 0;
 bool started_ = false;
 bool primed_ = false;
};
}
