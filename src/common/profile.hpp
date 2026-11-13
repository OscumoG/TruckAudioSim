// SPDX-License-Identifier: MIT
#pragma once
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
};
struct Profile {std::string name,vehicle,game; std::vector<Speaker> speakers;};
Profile loadProfile(const std::string& path);
Profile parseProfileText(const std::string& text, bool allowEmpty=false);
Vec3 relativePosition(const Vec3& speaker, const Vec3& listener, float yawDegrees);
float channelSample(const Speaker& speaker, float left, float right);
float dbToAmplitude(float db);
}
