// SPDX-License-Identifier: MIT
#pragma once
#include <string>
namespace tas {
enum class MediaAction { None, Pause, Resume };
// Implements pause-on-electricity-off without taking over media paused by user.
// Only remembers a pause after the player confirms it succeeded.
class MediaPausePolicy {
 bool known_=false;
 bool lastElectric_=true;
 bool ownedPause_=false;
 std::string ownedSession_;
 MediaAction pending_=MediaAction::None;
 std::string pendingSession_;
public:
 MediaAction evaluate(bool electronicsOn,bool sessionPresent,const std::string& sessionId,bool playing);
 void complete(MediaAction command,bool succeeded);
 bool ownsPause() const noexcept{return ownedPause_;}
};
}
