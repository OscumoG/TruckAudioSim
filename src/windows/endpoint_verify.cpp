// SPDX-License-Identifier: MIT
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include "device_select.hpp"
#include "endpoint_loopback.hpp"

using Microsoft::WRL::ComPtr;
namespace {
constexpr uint32_t sampleRate = 48000;
constexpr size_t analysisFrames = sampleRate;
constexpr size_t warmupFrames = sampleRate / 4;

void check(HRESULT code, const char* step) {
 if (FAILED(code)) {
  char message[128];
  std::snprintf(message, sizeof(message), "%s: HRESULT 0x%08lX", step, static_cast<unsigned long>(code));
  throw std::runtime_error(message);
 }
}

struct ComApartment {
 ComApartment() { check(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "CoInitializeEx"); }
 ~ComApartment() { CoUninitialize(); }
};

constexpr double frequency(unsigned stream, size_t channel) {
 return (stream == 0 ? 300.0 : 1350.0) + 100.0 * channel;
}

class TestStream {
public:
 TestStream(IMMDevice* endpoint, unsigned stream) : stream_(stream) {
  check(endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
        reinterpret_cast<void**>(audio_.GetAddressOf())), "Activate test render client");
  WAVEFORMATEX* raw = nullptr;
  check(audio_->GetMixFormat(&raw), "Test render GetMixFormat");
  std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> mix(raw, &CoTaskMemFree);
  if (!mix) throw std::runtime_error("Test render mix format is missing");
  format_ = tas::describeEndpointFormat(*mix);
  GUID session{};
  check(CoCreateGuid(&session), "CoCreateGuid for independent audio session");
  check(audio_->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 1000000, 0, mix.get(), &session),
        "Initialize independent shared-mode render stream");
  check(audio_->GetBufferSize(&capacity_), "Get test render buffer size");
  if (!capacity_) throw std::runtime_error("Test render buffer is empty");
  check(audio_->GetService(IID_PPV_ARGS(&render_)), "Get test render service");
 }

 ~TestStream() { if (started_) audio_->Stop(); }

 void start() {
  refill();
  check(audio_->Start(), "Start test render stream");
  started_ = true;
 }

 void refill() {
  UINT32 padding = 0;
  check(audio_->GetCurrentPadding(&padding), "Get test render padding");
  if (padding > capacity_) throw std::runtime_error("Unexpected test render padding");
  const UINT32 frames = capacity_ - padding;
  if (!frames) return;
  BYTE* buffer = nullptr;
  check(render_->GetBuffer(frames, &buffer), "Get test render frames");
  const auto channels = format_.stream.channels;
  for (UINT32 f = 0; f < frames; ++f) {
   const auto phase = static_cast<double>((nextFrame_ + f) % sampleRate) / sampleRate;
   for (size_t c = 0; c < channels; ++c) {
    const double sample = 0.15 * std::sin(2.0 * std::numbers::pi * frequency(stream_, c) * phase);
    const size_t sampleIndex = static_cast<size_t>(f) * channels + c;
    if (format_.convertPCM16)
     reinterpret_cast<int16_t*>(buffer)[sampleIndex] = static_cast<int16_t>(std::lround(sample * 32767));
    else
     reinterpret_cast<float*>(buffer)[sampleIndex] = static_cast<float>(sample);
   }
  }
  check(render_->ReleaseBuffer(frames, 0), "Release test render frames");
  nextFrame_ += frames;
 }

private:
 unsigned stream_;
 ComPtr<IAudioClient> audio_;
 ComPtr<IAudioRenderClient> render_;
 tas::EndpointAcceptance format_{};
 UINT32 capacity_ = 0;
 uint64_t nextFrame_ = 0;
 bool started_ = false;
};

double amplitude(const float* pcm, size_t channels, size_t channel, double hz) {
 const double coefficient = 2.0 * std::cos(2.0 * std::numbers::pi * hz / sampleRate);
 double prev = 0.0, prev2 = 0.0;
 for (size_t f = 0; f < analysisFrames; ++f) {
  const double next = pcm[f * channels + channel] + coefficient * prev - prev2;
  prev2 = prev;
  prev = next;
 }
 return 2.0 * std::sqrt(std::max(0.0, prev * prev + prev2 * prev2 - coefficient * prev * prev2))
        / analysisFrames;
}

int verify(IMMDevice* endpoint, std::optional<size_t> expectedChannels, bool requireClean) {
 tas::EndpointLoopback capture(endpoint);
 const auto& format = capture.format();
 const size_t channels = format.channels;
 if (expectedChannels && channels != *expectedChannels)
  throw std::runtime_error("The Windows playback mix has " + std::to_string(channels) +
                           " channels, but the requested test expects " +
                           std::to_string(*expectedChannels) +
                           ". Configure this output in Windows Sound first.");
 std::cout << "WASAPI mix: " << channels << " channels, mask 0x" << std::hex << format.channelMask
           << std::dec << ", " << format.sampleRate << " Hz\n" << std::flush;
 TestStream first(endpoint, 0);
 TestStream second(endpoint, 1);
 capture.start();
 first.start();
 second.start();

 constexpr size_t requiredFrames = warmupFrames + analysisFrames;
 std::vector<float> captured, packet;
 captured.reserve(requiredFrames * channels);
 const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(12);
 while (captured.size() < requiredFrames * channels && std::chrono::steady_clock::now() < deadline) {
  first.refill();
  second.refill();
  const size_t remaining = requiredFrames - captured.size() / channels;
  const size_t frames = capture.readFrames(std::min<size_t>(remaining, 480), packet);
  captured.insert(captured.end(), packet.begin(), packet.begin() + frames * channels);
  std::this_thread::sleep_for(std::chrono::milliseconds(4));
 }
 std::cout << "Capture frames=" << captured.size() / channels
           << ", dropped=" << capture.droppedFrames()
           << ", discontinuities=" << capture.discontinuities() << "\n";
 if (captured.size() < requiredFrames * channels)
  throw std::runtime_error("Not enough PCM arrived through WASAPI loopback");
 const float* samples = captured.data() + warmupFrames * channels;

 bool pass = true;
 for (unsigned stream = 0; stream < 2; ++stream) {
  for (size_t channel = 0; channel < channels; ++channel) {
   const double hz = frequency(stream, channel);
   const double signal = amplitude(samples, channels, channel, hz);
   double bleed = 0.0;
   for (size_t other = 0; other < channels; ++other)
    if (other != channel) bleed = std::max(bleed, amplitude(samples, channels, other, hz));
   const bool passed = std::isfinite(signal) && signal >= 0.01 && bleed <= signal * 0.0631;
   std::cout << "Stream " << stream + 1 << " channel " << channel + 1 << ": " << hz
             << " Hz, amplitude=" << signal << ", other channels=" << bleed
             << (passed ? " PASS\n" : " FAIL\n");
   pass = pass && passed;
  }
 }
 if (requireClean && (capture.droppedFrames() || capture.discontinuities())) {
  std::cout << "FAIL: strict mode detected dropped frames or discontinuities\n";
  pass = false;
 }
 std::cout << (pass ? "PASS: two shared sessions and all channels preserved\n"
                    : "FAIL: expected source tones missing or leaking between channels\n");
 return pass ? 0 : 1;
}

int run(int argc, char** argv) {
 ComApartment com;
 ComPtr<IMMDeviceEnumerator> devices;
 check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&devices)),
       "Enumerate Windows audio endpoints");
 if (argc == 2 && std::string_view(argv[1]) == "--list-devices") {
  tas::printRenderDevices(devices.Get());
  return 0;
 }
 std::optional<uint32_t> index;
 std::wstring id;
 std::optional<size_t> expectedChannels;
 bool requireClean = false;
 for (int i = 1; i < argc; ++i) {
  const std::string_view option(argv[i]);
  if (option == "--require-clean") { requireClean = true; continue; }
  if (i + 1 >= argc) throw std::runtime_error("Missing value for " + std::string(option));
  const std::string_view value(argv[++i]);
  if (option == "--output-index") {
   uint32_t number = 0;
   const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
   if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
    throw std::runtime_error("Invalid output index");
   if (index) throw std::runtime_error("Duplicate output index");
   index = number;
  } else if (option == "--output-id" && !value.empty()) {
   if (!id.empty()) throw std::runtime_error("Duplicate output ID");
   id.assign(value.begin(), value.end());
  } else if (option == "--expect-channels") {
   uint32_t number = 0;
   const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
   if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
       (number != 2 && number != 6 && number != 8))
    throw std::runtime_error("--expect-channels must be 2, 6 or 8");
   if (expectedChannels) throw std::runtime_error("Duplicate expected channels");
   expectedChannels = number;
  } else {
   throw std::runtime_error("Invalid or empty endpoint option");
  }
 }
 if (index.has_value() == !id.empty())
  throw std::runtime_error("Specify exactly one playback endpoint (--output-index or --output-id)");
 const auto endpoint = tas::selectRenderDevice(devices.Get(), index, id);
 return verify(endpoint.Get(), expectedChannels, requireClean);
}
}

int main(int argc, char** argv) {
 try { return run(argc, argv); }
 catch (const std::exception& error) {
  std::cerr << "tas_endpoint_verify: " << error.what() << "\n"
            << "Usage: tas_endpoint_verify.exe (--output-index N | --output-id ID)"
               " [--expect-channels 2|6|8] [--require-clean] | --list-devices\n";
  return 1;
 }
}
