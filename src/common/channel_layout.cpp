// SPDX-License-Identifier: MIT
#include "channel_layout.hpp"
#include "wav.hpp"
#include <stdexcept>

namespace tas {
Profile makeChannelTestProfile(uint16_t channels, uint32_t channelMask) {
 const bool valid=(channels==2 && channelMask==kStereo) ||
  (channels==3 && channelMask==kTwoPointOne) ||
  (channels==6 && (channelMask==kFivePointOneSide || channelMask==kFivePointOneBack)) ||
  (channels==8 && channelMask==kSevenPointOne);
 if(!valid)throw std::invalid_argument("Channel test expects a supported, exact speaker mask");
 Profile p;
 p.name="Independent channel object test";
 // Windows Spatial Sound is right-handed, +X right, +Y up, -Z forward.
 // LFE intentionally stays discrete: Spatial Sound may bass-manage the output
 // but TruckAudioSim does not invent or distribute LFE among other objects.
 const Speaker layout[]={
   {"FL","Front left","FL","speaker",{-0.85f,0.0f,-1.0f},0.0f},
   {"FR","Front right","FR","speaker",{0.85f,0.0f,-1.0f},0.0f},
   {"FC","Front center","FC","speaker",{0.0f,0.0f,-1.0f},0.0f},
   {"LFE","Low-frequency effects","LFE","sub",{0.0f,-0.65f,0.0f},0.0f},
   {"BL","Back left","BL","speaker",{-0.85f,0.0f,0.80f},0.0f},
   {"BR","Back right","BR","speaker",{0.85f,0.0f,0.80f},0.0f},
   {"SL","Side left","SL","speaker",{-1.0f,0.0f,0.0f},0.0f},
   {"SR","Side right","SR","speaker",{1.0f,0.0f,0.0f},0.0f},
 };
 // WAVEFORMATEXTENSIBLE interleaves channels by ascending mask bit.
 const uint32_t bits[]={0x1,0x2,0x4,0x8,0x10,0x20,0x200,0x400};
 for(size_t i=0;i<8;++i)if(channelMask & bits[i])p.speakers.push_back(layout[i]);
 if(p.speakers.size()!=channels)throw std::logic_error("Wrong diagnostic object count");
 return p;
}
}
