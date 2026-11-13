// SPDX-License-Identifier: MIT
#include "spatial_mix.hpp"
#include "wav.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

int main() {
 using namespace tas;
 const Speaker left{"left", "Left", "FL", "speaker", {-1, 0, -1}, 0};
 const Speaker right{"right", "Right", "FR", "speaker", {1, 0, -1}, 0};
 // Two real interleaved frames followed by a partial-source underrun.
 const std::vector<float> stereo{0.25f, -0.5f, -0.75f, 0.125f};
 float mono[5]{9, 9, 9, 9, 9};
 auto stats = mixSpatialObject(left, stereo, 2, 5, 2, kStereo, 1.0f, true, mono);
 assert(mono[0] == 0.25f && mono[1] == -0.75f);
 assert(std::all_of(mono + 2, mono + 5, [](float x) { return x == 0.0f; }));
 assert(stats.inputPeak == 0.75f && stats.outputPeak == 0.75f && stats.clippedSamples == 0);
 stats = mixSpatialObject(right, stereo, 2, 5, 2, kStereo, 1.0f, true, mono);
 assert(mono[0] == -0.5f && mono[1] == 0.125f && mono[4] == 0.0f);
 // No frame available: no reads from an empty source and no stale audio.
 std::fill_n(mono, 5, 9.0f);
 stats = mixSpatialObject(left, {}, 0, 5, 2, kStereo, 1.0f, true, mono);
 assert(stats.inputPeak == 0.0f && stats.clippedSamples == 0);
 assert(std::all_of(mono, mono + 5, [](float x) { return x == 0.0f; }));
 // Muting when electronics are off must not leak previous frames.
 std::fill_n(mono, 5, 9.0f);
 mixSpatialObject(left, stereo, 2, 5, 2, kStereo, 1.0f, false, mono);
 assert(std::all_of(mono, mono + 5, [](float x) { return x == 0.0f; }));
 // Real 7.1 channel identity: do not fold a discrete right surround into left front.
 const std::vector<float> surround{1, 2, 3, 4, 5, 6, 7, 8};
 const Speaker sideRight{"sr", "Side Right", "SR", "speaker", {1, 0, 0}, 0};
 mixSpatialObject(sideRight, surround, 1, 1, 8, kSevenPointOne, 0.1f, true, mono);
 assert(std::fabs(mono[0] - 0.8f) < 1e-6f);
 // Explicit ceiling and observable clipping, without changing quiet samples.
 stats = mixSpatialObject(left, stereo, 2, 2, 2, kStereo, 5.0f, true, mono);
 assert(mono[0] == 1.0f && mono[1] == -1.0f);
 assert(stats.clippedSamples == 2 && stats.inputPeak == 3.75f && stats.outputPeak == 1.0f);
 const std::vector<float> nonfinite{std::numeric_limits<float>::quiet_NaN(), 1.0f};
 mixSpatialObject(left, nonfinite, 1, 1, 2, kStereo, 1.0f, true, mono);
 assert(mono[0] == 0.0f);
 // Cab presets apply the speaker bandwidth in the native render path.
 Speaker center{"FC","Center","FC","mid",{},0,true,CabAudio::Surround51};
 Speaker sub{"LFE","Sub","LFE","sub",{},0,true,CabAudio::Surround51};
 const auto amplitude=[](const Speaker& speaker,float hz){
  constexpr size_t frames=4800;
  std::vector<float> input(frames*2),output(frames);
  for(size_t f=0;f<frames;++f)
   input[2*f]=input[2*f+1]=0.5f*std::sin(2*std::numbers::pi_v<float>*hz*f/48000);
  SpeakerFilter filter;
  filter.configure(speaker,48000);
  mixSpatialObject(speaker,input,frames,frames,2,kStereo,1,true,output.data(),&filter);
  float peak=0;
  for(size_t f=frames/2;f<frames;++f)peak=std::max(peak,std::fabs(output[f]));
  return peak;
 };
 assert(amplitude(center,1000)>0.20f && amplitude(center,1000)<0.26f
        && amplitude(center,60)<0.02f && amplitude(center,8000)<0.02f);
 assert(amplitude(sub,60)>0.20f && amplitude(sub,60)<0.26f && amplitude(sub,1000)<0.02f);
 Speaker front{"FL","Front left","FL","full",{},0,true,CabAudio::Surround51};
 assert(amplitude(front,60)>0.49f && amplitude(front,8000)>0.43f);
 Speaker rear{"SL","Side left","SL","full",{},0,true,CabAudio::Surround51};
 assert(amplitude(rear,1000)>0.24f && amplitude(rear,1000)<0.26f);
 // Reject invalid dimensions before dereferencing the PCM buffer.
 bool thrown = false;
 try { mixSpatialObject(left, stereo, 3, 5, 2, kStereo, 1.0f, true, mono); }
 catch (const std::invalid_argument&) { thrown = true; }
 assert(thrown);
 thrown = false;
 try { mixSpatialObject(left, stereo, 2, 1, 2, kStereo, 1.0f, true, mono); }
 catch (const std::invalid_argument&) { thrown = true; }
 assert(thrown);
}
