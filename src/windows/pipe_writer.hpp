// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>
#include <cstdint>
#include "stream_protocol.hpp"
namespace tas {
class PipeWriter {
 HANDLE pipe_=INVALID_HANDLE_VALUE;
 StreamFormat format_;
 void writeAll(const void*,size_t);
public:
 explicit PipeWriter(StreamFormat fmt);
 ~PipeWriter();
 PipeWriter(const PipeWriter&)=delete;
 PipeWriter& operator=(const PipeWriter&)=delete;
 void frames(const float* interleaved,uint32_t frameCount,bool silent);
 void finish();
};
}
