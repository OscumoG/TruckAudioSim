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
 // Turning in place changes spatial placement, never the acoustic impulse response.
 auto split=dry;
 tas::CabinAcoustics blocked;
 auto view=pose;
 for(size_t begin=0;begin<split.size();begin+=256) {
  view.yawDegrees+=7;
  view.pitchDegrees-=3;
  blocked.process(std::span(split).subspan(begin,256),room,speaker,view,rate);
 }
 require(impulse==split,"Stationary room responses must agree across block boundaries and rotation");

 // Warm the cache, then change room, source and listener without constructing a new processor.
 tas::CabinAcoustics reused;
 auto changedRoom=room;
 changedRoom.leftWindow=0.85f;
 changedRoom.reflections=0.1f;
 changedRoom.decayMs=240;
 auto changedSpeaker=speaker;
 changedSpeaker.pos={0.6f,0.25f,-0.6f};
 auto changedPose=pose;
 changedPose.head={0.35f,0.25f,0.2f};
 std::vector<float> silence(rate);
 reused.process(silence,room,speaker,pose,rate);
 reused.process(silence,changedRoom,changedSpeaker,changedPose,rate);
 auto adjusted=dry, expected=dry;
 reused.process(adjusted,changedRoom,changedSpeaker,changedPose,rate);
 tas::CabinAcoustics{}.process(expected,changedRoom,changedSpeaker,changedPose,rate);
 require(energy(std::span(expected).subspan(50))>0,"Updated settings must produce reflections");
 require(std::equal(adjusted.begin(),adjusted.end(),expected.begin(),[](float a,float b) {
  return std::abs(a-b)<1e-5f;
 }),"A live profile edit must refresh the acoustic paths and decay");
 auto outside=changedPose;
 outside.exterior=true;
 outside.head={-3,0,0};
 std::fill(silence.begin(),silence.end(),0);
 reused.process(silence,changedRoom,changedSpeaker,outside,rate);
 adjusted=dry;
 expected=dry;
 reused.process(adjusted,changedRoom,changedSpeaker,outside,rate);
 tas::CabinAcoustics{}.process(expected,changedRoom,changedSpeaker,outside,rate);
 require(std::abs(adjusted[0]-expected[0])<1e-5f &&
         std::abs(adjusted[0]-dry[0])>0.05f,"Exterior switch must refresh body attenuation");

 // Render only the late field and compare a short cabin with a long coach.
 auto lateRoom=room;
 lateRoom.reflections=0;
 lateRoom.reverb=0.6f;
 lateRoom.decayMs=90;
 auto compact=dry;
 tas::CabinAcoustics{}.process(compact,lateRoom,speaker,pose,rate);
 require(energy(std::span(compact).subspan(450,1300))>1e-6f,"Late field missing");
 lateRoom.max.z=11.5f;
 lateRoom.decayMs=320;
 auto coach=dry;
 tas::CabinAcoustics{}.process(coach,lateRoom,speaker,pose,rate);
 require(coach!=compact && energy(std::span(coach).subspan(4000))>
     5*energy(std::span(compact).subspan(4000)),"Coach should have a distinct, longer decay");
 require(energy(std::span(coach).subspan(6000))<energy(std::span(coach).subspan(1250,2000)),
     "Late field does not decay");
 lateRoom.reverb=0;
 auto noReverb=dry;
 tas::CabinAcoustics{}.process(noReverb,lateRoom,speaker,pose,rate);
 require(noReverb==dry,"Zero wet levels altered the direct sound");
 room.reverb=0;
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

 // A cut changes the target band while the EQ bypass preserves exact PCM.
 std::vector<float> bass(rate), mid(rate);
 for (size_t i=0;i<rate;++i) {
  bass[i]=0.2f*std::sin(6.2831853f*80*i/rate);
  mid[i]=0.2f*std::sin(6.2831853f*1000*i/rate);
 }
 tas::Equalizer eq{false,{-9,0,0,0,0}};
 auto unfiltered=bass;
 tas::EqualizerFilter{}.process(unfiltered,eq,rate);
 require(unfiltered==bass,"Bypassed equalizer changed PCM");
 eq.enabled=true;
 auto bassCut=bass, midCut=mid;
 tas::EqualizerFilter{}.process(bassCut,eq,rate);
 tas::EqualizerFilter{}.process(midCut,eq,rate);
 require(energy(bassCut)<0.16f*energy(bass) && energy(midCut)>0.95f*energy(mid),
    "Bass cut was not selective");
 eq.gains={9,9,9,9,9};
 tas::EqualizerFilter{}.process(mid,eq,96000);
 require(std::ranges::all_of(mid,[](float v){return std::isfinite(v) && std::abs(v)<0.3f;}),
    "Combined EQ boost exceeded the expected headroom");
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
