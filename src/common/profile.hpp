// SPDX-License-Identifier: MIT
#pragma once
#include "equalizer.hpp"
#include <array>
#include <string>
#include <vector>
namespace tas {
struct Vec3 {float x=0,y=0,z=0;};
enum class CabAudio { Legacy, Realistic, Surround51, Surround71 };
struct Speaker {
 std::string id,name,channel,type;
 Vec3 pos;
 float gainDb=0;
 bool enabled=true;
 CabAudio cabAudio=CabAudio::Legacy;
 bool coach=false;
 float volume=1.0f;
};
struct Acoustics {
 bool enabled=false, splitDeck=false;
 Vec3 min{-1.15f,-1.15f,-1.3f}, max{1.15f,0.95f,0.6f};
 float reflections=0.43f, absorption=0.43f, bodyLoss=9, bodyCutoff=1050;
 float reverb=0.20f, decayMs=110;
 float leftWindow=0, rightWindow=0, deckHeight=1.25f;
};
struct Profile {
 std::string name,vehicle,game;
 std::vector<Speaker> speakers;
 Acoustics acoustics{};
 Equalizer equalizer{};
};
Profile loadProfile(const std::string& path);
Profile parseProfileText(const std::string& text, bool allowEmpty=false);
Vec3 relativePosition(const Vec3& speaker, const Vec3& listener, float yawDegrees);
float channelSample(const Speaker& speaker, float left, float right);
float dbToAmplitude(float db);
}
