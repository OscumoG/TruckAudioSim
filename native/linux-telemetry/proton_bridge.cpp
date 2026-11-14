// SPDX-License-Identifier: MIT
// A Windows helper launched in the same Proton invocation as the game. The
// Linux editor reads these bytes directly from a file in its private directory.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../camera-plugin/camera_block.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <cwchar>

namespace {
template<size_t Size>
struct Mapping {
 const wchar_t* name;
 const void* data = nullptr;
 Mapping(const wchar_t* key) : name(key) {}
 ~Mapping() { close(); }
 void close() {
  if (data) UnmapViewOfFile(data);
  data = nullptr;
 }
 bool read(std::array<char, Size>& bytes) {
  if (!data) {
   const HANDLE section = OpenFileMappingW(FILE_MAP_READ, FALSE, name);
   if (!section) return false;
   data = MapViewOfFile(section, FILE_MAP_READ, 0, 0, Size);
   CloseHandle(section);
  }
  if (!data) return false;
  std::memcpy(bytes.data(), data, Size);
  return true;
 }
};

template<size_t Size>
bool publish(const wchar_t* path, const std::array<char, Size>& bytes) {
 wchar_t temporary[MAX_PATH]{};
 if (swprintf(temporary, MAX_PATH, L"%ls.next", path)<0) return false;
 const HANDLE file=CreateFileW(temporary,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if (file==INVALID_HANDLE_VALUE) return false;
 DWORD written=0;
 const bool ok=WriteFile(file,bytes.data(),bytes.size(),&written,nullptr) && written==bytes.size();
 CloseHandle(file);
 if (ok && MoveFileExW(temporary,path,MOVEFILE_REPLACE_EXISTING)) return true;
 DeleteFileW(temporary);
 return false;
}
}

int wmain(int argc, wchar_t** argv) {
 if (argc!=4) return 2;
 const auto pid=wcstoul(argv[1],nullptr,10);
 const HANDLE game=OpenProcess(SYNCHRONIZE,FALSE,pid);
 if (!game) return 3;
 Mapping<3436> telemetry(L"Local\\SCSTelemetry");
 Mapping<sizeof(TruckerAuxCamera)> camera(TRUCKER_AUX_CAMERA_NAME);
 std::array<char,3436> pose{}, lastPose{};
 std::array<char,sizeof(TruckerAuxCamera)> first{}, second{}, lastCamera{};
 bool havePose=false, haveCamera=false;
 while (WaitForSingleObject(game,16)==WAIT_TIMEOUT) {
  if (telemetry.read(pose) && (!havePose || pose!=lastPose) && publish(argv[2],pose)) {
   havePose=true;
   lastPose=pose;
  }
  if (camera.read(first) && camera.read(second) && first==second) {
   TruckerAuxCamera block{};
   std::memcpy(&block,first.data(),first.size());
   if (block.layout==1 && block.sequence%2==0 && (!haveCamera || first!=lastCamera) && publish(argv[3],first)) {
    haveCamera=true;
    lastCamera=first;
   }
  }
 }
 DeleteFileW(argv[2]);
 DeleteFileW(argv[3]);
 CloseHandle(game);
 return 0;
}
