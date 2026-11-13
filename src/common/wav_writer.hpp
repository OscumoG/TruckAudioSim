// SPDX-License-Identifier: MIT
#pragma once
#include "stream_protocol.hpp"
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
namespace tas {
// Portable WAVEFORMATEXTENSIBLE float32 writer for loopback diagnostics.
// Writer owns a seekable file; finalize patches its RIFF and data sizes.
class WavWriter {
 std::ofstream out_;
 StreamFormat format_;
 uint64_t framesWritten_=0;
 bool closed_=false;
public:
 WavWriter(const std::string& file, const StreamFormat& format);
 WavWriter(const WavWriter&)=delete;
 WavWriter& operator=(const WavWriter&)=delete;
 void write(const float* interleaved,size_t frames);
 void finalize();
 ~WavWriter();
 uint64_t framesWritten() const noexcept{return framesWritten_;}
};
}
