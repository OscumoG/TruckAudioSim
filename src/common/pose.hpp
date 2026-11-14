// SPDX-License-Identifier: MIT
#pragma once
#include "profile.hpp"
#include <string>
namespace tas {
inline constexpr float kMaxVolume=10.0f;
struct Pose {
 Vec3 head{-0.40f,0.0f,0.0f};
 float yawDegrees=0;
 bool electronicsOn=true;
 float pitchDegrees=0;
 float rollDegrees=0;
 float volume=1;
 float exteriorGain=1;
 bool exterior=false;
 unsigned testSequence=0;
 std::string testSpeaker;
};
Pose parsePoseText(const std::string& json);
Pose loadPoseFile(const std::string& path);
// Smooth camera rotation (shortest arc) and translation across audio blocks.
Pose smoothPose(const Pose& previous,const Pose& target,float factor);
Vec3 relativePosition(const Vec3& speaker,const Pose& pose);
}
