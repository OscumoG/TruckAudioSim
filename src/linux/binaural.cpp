// SPDX-License-Identifier: MIT
#include "binaural.hpp"
#include "acoustics.hpp"
#include "normalization.hpp"
#include "spatial_mix.hpp"
#include <mysofa.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <numbers>
#include <stdexcept>
#include <span>

namespace tas {
namespace {
using Complex = std::complex<float>;
constexpr float kDirectionThreshold = 0.9986f; // about 3 degrees
constexpr float kMaxDelaySeconds = 0.01f;

using Sofa = std::unique_ptr<MYSOFA_EASY, decltype(&mysofa_close)>;
using Ears = std::array<std::vector<Complex>, 2>;

// Unscaled radix-2 transform. Precompute the permutation and roots once per stream.
class Transform {
 public:
  explicit Transform(size_t size) : order_(size), roots_(size / 2) {
   for (size_t i = 1; i < size; ++i) order_[i] = (order_[i >> 1] >> 1) | ((i & 1) * (size >> 1));
   for (size_t i = 0; i < roots_.size(); ++i)
    roots_[i] = std::polar(1.0f, -2.0f * std::numbers::pi_v<float> * i / size);
  }

  void apply(std::span<Complex> data, bool inverse = false) const {
   for (size_t i = 1; i < order_.size(); ++i) if (i < order_[i]) std::swap(data[i], data[order_[i]]);
   for (size_t width = 2; width <= data.size(); width *= 2) {
    const size_t half = width / 2;
    const size_t step = data.size() / width;
    for (size_t start = 0; start < data.size(); start += width) {
     for (size_t offset = 0; offset < half; ++offset) {
      const auto root = roots_[offset * step];
      auto& even = data[start + offset];
      auto& odd = data[start + offset + half];
      const auto rotated = odd * (inverse ? std::conj(root) : root);
      odd = even - rotated;
      even += rotated;
     }
    }
   }
  }

 private:
  std::vector<size_t> order_;
  std::vector<Complex> roots_;
};

struct Voice {
 Speaker speaker;
 SpeakerFilter filter;
 CabinAcoustics acoustics;
 float gain = 0;
 std::vector<float> history;
 Ears hrtf;
 Ears nextHrtf;
 Vec3 direction{};
 bool ready = false;
};

float dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }

bool sameFilter(const Speaker& a, const Speaker& b) {
 return a.id == b.id && a.type == b.type && a.channel == b.channel && a.cabAudio == b.cabAudio;
}
}

struct BinauralState {
 uint32_t rate;
 uint16_t channels;
 uint32_t mask;
 bool discrete;
 int impulseLength = 0;
 size_t fftSize;
 Sofa sofa;
 std::vector<float> time;
 std::vector<Complex> spectrum;
 Ears sum;
 std::array<std::vector<float>, 2> impulse;
 std::unique_ptr<Transform> transform;
 std::vector<Complex> work;
 std::vector<Voice> voices;
 Acoustics acoustics;
 SpeakerTest speakerTest;

 BinauralState(const char* path, uint32_t rate, uint16_t channels, uint32_t mask, bool discrete)
     : rate(rate), channels(channels), mask(mask), discrete(discrete), fftSize(kBinauralBlock),
       sofa(nullptr, mysofa_close) {
  int error = 0;
  sofa.reset(mysofa_open(path, static_cast<float>(rate), &impulseLength, &error));
  if (!sofa || impulseLength <= 0 || impulseLength > 8192)
   throw std::runtime_error("Cannot load SOFA HRTF: " + std::string(path) + " (code " + std::to_string(error) + ")");
  const size_t historyNeeded = impulseLength + rate*kMaxDelaySeconds + 1;
  while (fftSize < kBinauralBlock + historyNeeded) fftSize *= 2;
  time.resize(fftSize);
  spectrum.resize(fftSize / 2 + 1);
  for (auto& ear : sum) ear.resize(spectrum.size());
  for (auto& ear : impulse) ear.resize(impulseLength);
  transform = std::make_unique<Transform>(fftSize);
  work.resize(fftSize);
 }

 void forward() {
  std::copy(time.begin(), time.end(), work.begin());
  transform->apply(work);
  std::copy_n(work.begin(), spectrum.size(), spectrum.begin());
 }

 void inverse(size_t ear) {
  const auto& bins = sum[ear];
  std::copy(bins.begin(), bins.end(), work.begin());
  for (size_t bin = 1; bin + 1 < bins.size(); ++bin) work[fftSize-bin] = std::conj(bins[bin]);
  transform->apply(work, true);
 }

 void selectDirection(Voice& voice, Vec3 dir) {
  if (voice.ready && dot(voice.direction, dir) > kDirectionThreshold) return;
  float delay[2]{};
  // SOFA coordinates are +X forward, +Y left, +Z up. Use a unit direction
  // and apply distance in the mixer so the HRTF cannot add distance gain twice.
  mysofa_getfilter_float(sofa.get(), -dir.z, -dir.x, dir.y,
                         impulse[0].data(), impulse[1].data(), &delay[0], &delay[1]);
  const float earliest = std::min(delay[0], delay[1]);
  for (size_t ear = 0; ear < 2; ++ear) {
   std::fill(time.begin(), time.end(), 0.0f);
   const float samples = (delay[ear] - earliest) * rate;
   const float offset = std::isfinite(samples) ? std::clamp(samples, 0.0f, rate*kMaxDelaySeconds) : 0;
   const size_t whole = static_cast<size_t>(offset);
   const float fraction = offset - whole;
   for (int n = 0; n < impulseLength; ++n) {
    time[whole+n] += impulse[ear][n]*(1.0f-fraction);
    time[whole+n+1] += impulse[ear][n]*fraction;
   }
   forward();
   voice.nextHrtf[ear] = spectrum;
   if (!voice.ready) voice.hrtf[ear] = spectrum;
  }
  voice.direction = dir;
  voice.ready = true;
 }
};

Binaural::Binaural(const char* sofa, uint32_t rate, uint16_t channels, uint32_t mask, bool discrete)
    : state_(std::make_unique<BinauralState>(sofa, rate, channels, mask, discrete)) {}
Binaural::~Binaural() = default;

void Binaural::setProfile(const Profile& profile) {
 auto& s = *state_;
 const auto gains = s.discrete ? std::vector<float>(profile.speakers.size(), 1.0f)
    : neutralVoiceGains(profile.speakers, s.channels, s.mask, 0.95f, s.rate);
 std::vector<Voice> next;
 next.reserve(profile.speakers.size());
 for (size_t i = 0; i < profile.speakers.size(); ++i) {
  const auto& speaker = profile.speakers[i];
  if (!speaker.enabled) continue;
  const auto found = std::find_if(s.voices.begin(), s.voices.end(), [&](const auto& voice) {
   return sameFilter(voice.speaker, speaker);
  });
  auto& voice = next.emplace_back();
  if (found == s.voices.end()) {
   voice.filter.configure(speaker, s.rate);
   voice.history.resize(s.fftSize-kBinauralBlock);
  } else {
   voice = std::move(*found);
  }
  voice.speaker = speaker;
  voice.gain = gains[i];
 }
 s.voices.swap(next);
 s.acoustics=profile.acoustics;
}

void Binaural::render(const std::vector<float>& pcm, size_t frames, const Pose& pose, float* stereo) {
 auto& s = *state_;
 for (auto& ear : s.sum) std::fill(ear.begin(), ear.end(), Complex{});
 const bool testing=pose.testSequence!=0;
 s.speakerTest.prepare(pose.testSequence,s.rate);
 const float master = testing?0.5f*pose.exteriorGain:(pose.electronicsOn ? pose.volume*pose.exteriorGain : 0);
 const float reference = s.discrete ? 0.7f : kDesktopReferenceGain;
 for (auto& voice : s.voices) {
  if(testing && pose.testSpeaker!=voice.speaker.id) continue;
  const auto relative = relativePosition(voice.speaker.pos, pose);
  const float radius = std::hypot(relative.x, relative.y, relative.z);
  const Vec3 dir = radius > 0.001f
     ? Vec3{relative.x/radius, relative.y/radius, relative.z/radius} : Vec3{0,0,-1};
  s.selectDirection(voice, dir);
  const float distance = voice.speaker.coach ? 0.25f : 0.5f;
  const float attenuation = voice.speaker.volume/(1.0f+distance*std::max(0.0f,radius-1.0f));
  const auto history = voice.history.size();
  std::copy(voice.history.begin(), voice.history.end(), s.time.begin());
  auto block=std::span(s.time.data()+history,kBinauralBlock);
  if(testing) s.speakerTest.render(block,voice.speaker.type=="sub");
  else mixSpatialObject(voice.speaker, pcm, frames, kBinauralBlock, s.channels, s.mask,
                        voice.gain, true, block.data(), &voice.filter);
  voice.acoustics.process(block,s.acoustics,voice.speaker,pose,s.rate);
  std::copy_n(s.time.begin()+kBinauralBlock, history, voice.history.begin());
  s.forward();
  for (size_t ear = 0; ear < 2; ++ear) {
   for (size_t bin = 0; bin < s.spectrum.size(); ++bin) {
    auto& current = voice.hrtf[ear][bin];
    current += (voice.nextHrtf[ear][bin]-current)*0.35f;
    s.sum[ear][bin] += s.spectrum[bin]*current*attenuation;
   }
  }
 }
 s.speakerTest.advance(kBinauralBlock);
 const float outputGain = master*reference/static_cast<float>(s.fftSize);
 for (size_t ear = 0; ear < 2; ++ear) {
  s.inverse(ear);
  for (size_t frame = 0; frame < kBinauralBlock; ++frame) {
   const float sample = s.work[s.fftSize-kBinauralBlock+frame].real()*outputGain;
   stereo[2*frame+ear] = std::isfinite(sample) ? std::clamp(sample,-0.98f,0.98f) : 0.0f;
  }
 }
}
}
