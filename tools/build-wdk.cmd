@echo off
rem SPDX-License-Identifier: MIT
setlocal
pushd "%~dp0.." || exit /b 1
where msbuild.exe >nul 2>&1
if errorlevel 1 (
  echo Abre x64 Native Tools Command Prompt de Visual Studio con Windows SDK y WDK.
  popd
  exit /b 1
)
msbuild.exe driver\sysvad\TruckAudioSimVirtualAudio.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:SignMode=Off
set "result=%errorlevel%"
popd
exit /b %result%
