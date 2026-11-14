// SPDX-License-Identifier: MIT
// Proton runs this as the game's primary command. Launch both processes
// through CreateProcess so they share the same Wine server and prefix.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <string>

namespace {
std::wstring quote(const std::wstring& argument) {
 std::wstring result=L"\"";
 size_t slashes=0;
 for (const auto ch : argument) {
  if (ch==L'\\') { ++slashes; continue; }
  result.append(ch==L'"' ? slashes*2+1 : slashes,L'\\');
  result+=ch;
  slashes=0;
 }
 result.append(slashes*2,L'\\');
 return result+L'"';
}

bool spawn(const wchar_t* executable,std::wstring command,PROCESS_INFORMATION& process) {
 STARTUPINFOW startup{};
 startup.cb=sizeof startup;
 if (!CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process)) return false;
 CloseHandle(process.hThread);
 return true;
}
}

int wmain(int argc,wchar_t** argv) {
 if (argc<5) return 2;
 std::wstring command=quote(argv[4]);
 for (int i=5;i<argc;++i) command+=L" "+quote(argv[i]);
 PROCESS_INFORMATION game{};
 if (!spawn(argv[4],command,game)) {
  fwprintf(stderr,L"TruckAudioSim: cannot start %ls (%lu)\n",argv[4],GetLastError());
  return 3;
 }
 PROCESS_INFORMATION bridge{};
 const bool bridged=spawn(argv[1],quote(argv[1])+L" "+std::to_wstring(game.dwProcessId)+L" "+quote(argv[2])+L" "+quote(argv[3]),bridge);
 if (!bridged) fwprintf(stderr,L"TruckAudioSim: bridge could not start (%lu)\n",GetLastError());
 WaitForSingleObject(game.hProcess,INFINITE);
 if (bridged) {
  WaitForSingleObject(bridge.hProcess,1000);
  CloseHandle(bridge.hProcess);
 }
 DWORD code=0;
 GetExitCodeProcess(game.hProcess,&code);
 CloseHandle(game.hProcess);
 return static_cast<int>(code);
}
