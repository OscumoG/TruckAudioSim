// SPDX-License-Identifier: MIT
#include "normalization.hpp"
#include "routing.hpp"
#include "wav.hpp"
#include <stdexcept>
#include <cmath>
#include <iostream>
namespace {
void require(bool ok, const char* message){if(!ok)throw std::runtime_error(message);}
}
int main(){
 using tas::Speaker;
 std::vector<Speaker> s;
 for(int i=0;i<3;++i){Speaker sp;sp.channel="L";sp.gainDb=0; s.push_back(sp);}
 for(int i=0;i<3;++i){Speaker sp;sp.channel="R";sp.gainDb=0; s.push_back(sp);}
 Speaker c;c.channel="M";s.push_back(c);
 auto g=tas::neutralVoiceGains(s,2,tas::kStereo);
 require(g.size()==s.size(),"Incorrect output gains count");
 // Three copies plus phantom-center 0.5 -> fixed 0.75 dBFS sum bound.
 require(std::fabs((3.0f*g[0]+0.5f*g[6])-0.75f)<1e-5f,"Stereo left channel exceeded static budget");
 Speaker sub;sub.channel="LFE";sub.type="sub";
 auto h=tas::neutralVoiceGains({sub},2,tas::kStereo);
 require(h.size()==1,"LFE gain count mismatch");
 require(tas::sourceForSpeaker(sub,std::array<float,2>{1,1}.data(),2,tas::kStereo)==0,"LFE must not be synthesized from stereo");
 Speaker midbass;midbass.channel="L";midbass.type="midbass";midbass.cabAudio=tas::CabAudio::Realistic;
 Speaker tweeter=midbass;tweeter.type="tweeter";
 const auto cab=tas::neutralVoiceGains({midbass,tweeter},2,tas::kStereo,0.95f,48000);
 require(cab[0]>0.94f && cab[0]<0.96f && cab[1]==cab[0],
         "Complementary crossover was budgeted like duplicate full-range speakers");
 tweeter.type="midbass";
 const auto overlap=tas::neutralVoiceGains({midbass,tweeter},2,tas::kStereo,0.95f,48000);
 require(overlap[0]>0.47f && overlap[0]<0.49f,
         "Overlapping bands must share the channel budget");
 std::cout<<"Neutral gain planning OK\n";
}
