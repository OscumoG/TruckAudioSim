Unicode true
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "WinVer.nsh"

Name "TruckAudioSim ${VERSION} Portable"
OutFile "${SETUP_OUT}"
Icon "..\src\main\truckaudiosim.ico"
RequestExecutionLevel user
SilentInstall silent
AutoCloseWindow true
SetCompressor /SOLID lzma
VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "TruckAudioSim"
VIAddVersionKey "FileDescription" "TruckAudioSim Portable para Windows x64"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "TruckAudioSim y colaboradores de Trucker AUX"

Function .onInit
  ${IfNot} ${IsNativeAMD64}
  ${OrIfNot} ${AtLeastWin10}
    MessageBox MB_OK|MB_ICONSTOP "TruckAudioSim requiere Windows 10 u 11 de 64 bits (x64)."
    SetErrorLevel 2
    Abort
  ${EndIf}
FunctionEnd

Section
  InitPluginsDir
  System::Call 'kernel32::SetEnvironmentVariableW(w "PORTABLE_EXECUTABLE_DIR", w "$EXEDIR") i .r1'
  ${If} $1 == 0
    MessageBox MB_OK|MB_ICONSTOP "No se pudo iniciar TruckAudioSim en modo portátil."
    SetErrorLevel 2
    Abort
  ${EndIf}
  ClearErrors
  SetOutPath "$PLUGINSDIR\TruckAudioSim"
  File /r "${PAYLOAD}\*"
  ${If} ${Errors}
    MessageBox MB_OK|MB_ICONSTOP "No se pudo extraer TruckAudioSim en la carpeta temporal."
    SetErrorLevel 2
    Abort
  ${EndIf}
  ${GetParameters} $0
  ClearErrors
  ExecWait '"$PLUGINSDIR\TruckAudioSim\TruckAudioSim.exe" $0' $2
  ${If} ${Errors}
    MessageBox MB_OK|MB_ICONSTOP "No se pudo abrir TruckAudioSim."
    SetErrorLevel 2
  ${Else}
    SetErrorLevel $2
  ${EndIf}
  SetOutPath "$EXEDIR"
  RMDir /r "$PLUGINSDIR\TruckAudioSim"
SectionEnd
