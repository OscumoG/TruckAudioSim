// SPDX-License-Identifier: MIT
#pragma once
#include "audio_ring.hpp"
#include "stream_protocol.hpp"
#include <windows.h>
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>
namespace tas {
// Windows local named-pipe receiver. Unprivileged user-mode transport: NOT
// a signed WDM driver. Exactly one writer per session. Not exposed to network.
class LivePipeReceiver {
 HANDLE pipe_=INVALID_HANDLE_VALUE;
 std::unique_ptr<AudioFrameRing> ring_;
 std::thread worker_;
 std::atomic<bool> finished_{false},cancel_{false};
 std::string error_;
 StreamFormat format_{};
 void pump();
public:
 LivePipeReceiver()=default;
 ~LivePipeReceiver();
 LivePipeReceiver(const LivePipeReceiver&)=delete;
 LivePipeReceiver& operator=(const LivePipeReceiver&)=delete;
 void accept(const std::wstring& name=L"TruckAudioSimPcm");
 StreamFormat format()const{return format_;}
 size_t readFrames(size_t frames,std::vector<float>& samples);
 size_t bufferedFrames()const {return ring_?ring_->availableFrames():0;}
 bool finished()const{return finished_.load(std::memory_order_acquire);}
 // Must only be called after finished(); worker never modifies error thereafter.
 std::string error() const{return finished()?error_:"";}
};
}
