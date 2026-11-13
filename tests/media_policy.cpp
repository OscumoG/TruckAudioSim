// SPDX-License-Identifier: MIT
#include "media_policy.hpp"
#include <stdexcept>
#include <iostream>
using tas::MediaAction;using tas::MediaPausePolicy;
void expect(MediaAction a,MediaAction b){if(a!=b)throw std::runtime_error("wrong media action");}
int main(){
 try{
  MediaPausePolicy p;
  expect(p.evaluate(true,true,"spotify",true),MediaAction::None);
  expect(p.evaluate(false,true,"spotify",true),MediaAction::Pause);p.complete(MediaAction::Pause,true);
  if(!p.ownsPause())throw std::runtime_error("pause was not owned");
  expect(p.evaluate(false,true,"spotify",false),MediaAction::None);
  expect(p.evaluate(true,true,"spotify",false),MediaAction::Resume);p.complete(MediaAction::Resume,true);
  expect(p.evaluate(true,true,"spotify",false),MediaAction::None);
  // Explicitly paused by user while electricity is on: never auto-resume.
  MediaPausePolicy q;
  expect(q.evaluate(true,true,"spotify",false),MediaAction::None);
  expect(q.evaluate(false,true,"spotify",false),MediaAction::None);
  expect(q.evaluate(true,true,"spotify",false),MediaAction::None);
  // Failed pause: do not falsely claim ownership.
  MediaPausePolicy failed;
  expect(failed.evaluate(false,true,"spotify",true),MediaAction::Pause);failed.complete(MediaAction::Pause,false);
  expect(failed.evaluate(true,true,"spotify",false),MediaAction::None);
  // Different media session or application closed: never start unrelated audio.
  MediaPausePolicy other;
  expect(other.evaluate(false,true,"spotify",true),MediaAction::Pause);other.complete(MediaAction::Pause,true);
  expect(other.evaluate(true,true,"youtube",false),MediaAction::None);
  MediaPausePolicy gone;
  expect(gone.evaluate(false,true,"spotify",true),MediaAction::Pause);gone.complete(MediaAction::Pause,true);
  expect(gone.evaluate(false,false,"",false),MediaAction::None);
  expect(gone.evaluate(true,true,"spotify",false),MediaAction::None);
  std::cout<<"PASS: media pause/resume only on electronics transitions and for owned session\n";
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
