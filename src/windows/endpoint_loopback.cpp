// SPDX-License-Identifier: MIT
#include "endpoint_loopback.hpp"
#include <ksmedia.h>
#include <cstdio>
#include <stdexcept>
#include "device_select.hpp"
#include "route_guard.hpp"

namespace tas {
namespace {
void check(HRESULT code, const char* step) {
 if (FAILED(code)) {
  char message[144];
  std::snprintf(message, sizeof(message), "%s: HRESULT 0x%08lX", step, static_cast<unsigned long>(code));
  throw std::runtime_error(message);
 }
}
}

EndpointAcceptance describeEndpointFormat(const WAVEFORMATEX& mix) {
 uint32_t mask = 0;
 uint16_t validBits = mix.wBitsPerSample;
 bool isFloat = mix.wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
 bool isPcm = mix.wFormatTag == WAVE_FORMAT_PCM;
 if (mix.wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
  if (mix.cbSize < 22) throw std::runtime_error("Invalid WAVEFORMATEXTENSIBLE data");
  const auto& extended = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(mix);
  mask = extended.dwChannelMask;
  validBits = extended.Samples.wValidBitsPerSample;
  isFloat = extended.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
  isPcm = extended.SubFormat == KSDATAFORMAT_SUBTYPE_PCM;
 } else if (mix.nChannels == 2) {
  mask = KSAUDIO_SPEAKER_STEREO;
 }
 if (!isFloat && !isPcm) throw std::runtime_error("The loopback source must use PCM16 or float32");
 return acceptEndpointFormat({mix.nChannels, mix.nSamplesPerSec, mask, mix.wBitsPerSample,
     validBits, mix.nBlockAlign, mix.nAvgBytesPerSec, isFloat ? EndpointSample::Float32 : EndpointSample::PCM16});
}

EndpointLoopback::EndpointLoopback(IMMDevice* device) {
 check(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
       reinterpret_cast<void**>(audio_.GetAddressOf())), "Loopback source activation");
 WAVEFORMATEX* raw = nullptr;
 check(audio_->GetMixFormat(&raw), "Loopback GetMixFormat");
 std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> mix(raw, &CoTaskMemFree);
 if (!mix) throw std::runtime_error("The loopback source did not provide a mix format");
 const DWORD sourceRate = mix->nSamplesPerSec;
 DWORD flags = AUDCLNT_STREAMFLAGS_LOOPBACK;
 if (sourceRate != kStreamRate) {
  // Shared-mode WASAPI converts the capture rate while the original channel
  // mask and sample representation remain intact.
  mix->nSamplesPerSec = kStreamRate;
  mix->nAvgBytesPerSec = mix->nBlockAlign * kStreamRate;
  flags |= AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
 }
 format_ = describeEndpointFormat(*mix);
 format_.stream.sourceEndpointToken = audioEndpointToken(renderDeviceId(device));
 const auto result = audio_->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 0, 0, mix.get(), nullptr);
 const auto description = sourceRate == kStreamRate
  ? std::string("WASAPI shared loopback initialization")
  : "WASAPI loopback conversion from " + std::to_string(sourceRate) + " Hz to 48000 Hz";
 check(result, description.c_str());
 check(audio_->GetService(IID_PPV_ARGS(&capture_)), "Loopback IAudioCaptureClient");
 check(audio_->GetBufferSize(&bufferFrames_), "Loopback GetBufferSize");
 if (!bufferFrames_ || bufferFrames_ > 48000)
  throw std::runtime_error("The loopback source returned an unexpected buffer size");
 ring_ = std::make_unique<AudioFrameRing>(std::max<size_t>(5760, bufferFrames_), format_.stream.channels);
 converted_.reserve(static_cast<size_t>(bufferFrames_) * format_.stream.channels);
}

EndpointLoopback::~EndpointLoopback() {
 if (started_) audio_->Stop();
}

void EndpointLoopback::start() {
 check(audio_->Start(), "WASAPI shared loopback start");
 started_ = true;
}

size_t EndpointLoopback::readFrames(size_t requested, std::vector<float>& samples) {
 const size_t channels = format_.stream.channels;
 for (size_t processed = 0; processed < bufferFrames_;) {
  UINT32 next = 0;
  check(capture_->GetNextPacketSize(&next), "Loopback GetNextPacketSize");
  if (!next) break;
  BYTE* data = nullptr;
  UINT32 frames = 0;
  DWORD flags = 0;
  const HRESULT result = capture_->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
  if (result == AUDCLNT_S_BUFFER_EMPTY) break;
  check(result, "Loopback GetBuffer");
  const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
  try {
   if (!frames || frames > bufferFrames_ || (!silent && !data))
    throw std::runtime_error("WASAPI returned an invalid loopback packet");
   if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) ++discontinuities_;
   const float* source = reinterpret_cast<const float*>(data);
   if (silent) {
    converted_.assign(static_cast<size_t>(frames) * channels, 0.0f);
    source = converted_.data();
   } else if (format_.convertPCM16) {
    converted_.resize(static_cast<size_t>(frames) * channels);
    const auto* pcm = reinterpret_cast<const int16_t*>(data);
    for (size_t i = 0; i < converted_.size(); ++i) converted_[i] = static_cast<float>(pcm[i]) / 32768.0f;
    source = converted_.data();
   }
   droppedFrames_ += frames - ring_->write(source, frames);
  } catch (...) {
   capture_->ReleaseBuffer(frames);
   throw;
  }
  check(capture_->ReleaseBuffer(frames), "Loopback ReleaseBuffer");
  processed += frames;
 }
 samples.resize(requested * channels);
 // Let one 20 ms interval accumulate before crossing the independent
 // source and destination clocks. Re-prime after starvation or silence.
 if (!primed_ && ring_->availableFrames() < std::min(ring_->capacityFrames(), std::max<size_t>(960, requested))) {
  std::fill(samples.begin(), samples.end(), 0.0f);
  return 0;
 }
 const auto received = ring_->read(samples.data(), requested);
 primed_ = received == requested;
 return received;
}
}
