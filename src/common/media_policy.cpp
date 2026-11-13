// SPDX-License-Identifier: MIT
#include "media_policy.hpp"
#include <stdexcept>
namespace tas {
MediaAction MediaPausePolicy::evaluate(bool electronicsOn,bool present,const std::string& session,bool playing){
 if(pending_!=MediaAction::None)throw std::logic_error("Complete previous media action before evaluating again");
 bool justChanged=!known_||electronicsOn!=lastElectric_;
 known_=true;lastElectric_=electronicsOn;
 if(!present||session.empty()){
  // Do not resume an unrelated playback session if Spotify was closed/reopened.
  ownedPause_=false;ownedSession_.clear();return MediaAction::None;
 }
 if(electronicsOn){
  if(ownedPause_&&session==ownedSession_){
   ownedPause_=false;ownedSession_.clear();
   if(!playing){pending_=MediaAction::Resume;pendingSession_=session;return pending_;}
  } else if(ownedPause_){ownedPause_=false;ownedSession_.clear();}
  return MediaAction::None;
 }
 if(justChanged&&playing){
  pending_=MediaAction::Pause;pendingSession_=session;return pending_;
 }
 return MediaAction::None;
}
void MediaPausePolicy::complete(MediaAction command,bool succeeded){
 if(pending_!=command||command==MediaAction::None)throw std::logic_error("Unexpected media command completion");
 if(succeeded&&command==MediaAction::Pause){ownedPause_=true;ownedSession_=pendingSession_;}
 pending_=MediaAction::None;pendingSession_.clear();
}
}
