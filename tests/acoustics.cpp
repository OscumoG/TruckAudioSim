// SPDX-License-Identifier: MIT
#include "acoustics.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
constexpr unsigned rate=48000;
void require(bool value,const char* message) {
 if (!value) throw std::runtime_error(message);
}
float energy(std::span<const float> samples) {
 float sum=0;
 for (float sample:samples) sum+=sample*sample;
 return sum;
}
float compare(tas::Acoustics room, const tas::Speaker& speaker, const tas::Pose& pose, float hz) {
 tas::CabinAcoustics effect;
 std::vector<float> tone(rate);
 for (size_t i=0;i<tone.size();++i) tone[i]=0.25f*std::sin(6.2831853f*hz*i/rate);
 effect.process(tone,room,speaker,pose,rate);
 return energy(std::span(tone).last(rate/2));
}
}
int main() {
 tas::Speaker speaker;
 speaker.pos={-0.65f,0,-0.3f};
 tas::Pose pose;
 tas::Acoustics room;
 std::array<float,8192> impulse{};
 impulse[0]=1;
 tas::CabinAcoustics acoustics;
 const auto dry=impulse;
 acoustics.process(impulse,room,speaker,pose,rate);
 require(impulse==dry,"Disabled acoustic processing changed PCM");

 room.enabled=true;
 acoustics.process(impulse,room,speaker,pose,rate);
 require(energy(std::span(impulse).subspan(50))>0,"Early reflections missing");
 room.reflections=0;
 pose.exterior=true;
 pose.head={-2.7f,0,-0.2f};
 const float closed=compare(room,speaker,pose,3100);
 room.rightWindow=1;
 const float farOpening=compare(room,speaker,pose,3100);
 room.rightWindow=0;
 room.leftWindow=0.5f;
 const float partial=compare(room,speaker,pose,3100);
 room.leftWindow=1;
 const float open=compare(room,speaker,pose,3100);
 require(closed<farOpening && farOpening<partial && partial<open,"Window opening must follow proximity and percentage");
 require(closed<compare(room,speaker,pose,160)*0.15f,"Body did not attenuate high frequencies");

 pose.exterior=false;
 room.min.y=-0.8f;
 room.max.y=2.8f;
 room.splitDeck=true;
 pose.head={0,0,0};
 const float lower=compare(room,speaker,pose,2000);
 pose.head.y=1.95f;
 require(compare(room,speaker,pose,2000)<lower*0.3f,"Deck should obstruct the opposite floor");

 tas::SpeakerTest test;
 test.prepare(1,rate);
 std::vector<float> preview(rate);
 test.render(preview,false);
 require(energy(preview)>1,"Speaker test is silent");
 require(std::ranges::all_of(preview,[](float value){return std::abs(value)<0.25f;}),"Speaker test level exceeded limit");
 test.advance(rate);
 test.prepare(1,rate);
 test.render(preview,false);
 require(energy(preview)==0,"Speaker test replayed without a new request");
}
