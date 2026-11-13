Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
!include "WinVer.nsh"
!define PRODUCT "TruckAudioSim"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT}"
!define MUI_ICON "..\src\main\truckaudiosim.ico"
!define MUI_UNICON "..\src\main\truckaudiosim.ico"
Name "${PRODUCT} ${VERSION}"
OutFile "${SETUP_OUT}"
InstallDir "$LOCALAPPDATA\Programs\${PRODUCT}"
InstallDirRegKey HKCU "${UNINSTALL_KEY}" "InstallLocation"
RequestExecutionLevel user
SetCompressor /SOLID lzma
VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "${PRODUCT}"
VIAddVersionKey "FileDescription" "Instalador de TruckAudioSim para Windows x64"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "TruckAudioSim y colaboradores de Trucker AUX"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\${PRODUCT}.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Abrir TruckAudioSim"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Spanish"

!macro RequireClosed
  FindWindow $0 "Chrome_WidgetWin_1" "TruckAudioSim — Trucker AUX Editor"
  ${If} $0 != 0
    MessageBox MB_OK|MB_ICONEXCLAMATION "Cierra TruckAudioSim con Salir desde el icono junto al reloj y vuelve a intentarlo."
    SetErrorLevel 2
    Abort
  ${EndIf}
!macroend

Function .onInit
  ${IfNot} ${IsNativeAMD64}
  ${OrIfNot} ${AtLeastWin10}
    MessageBox MB_OK|MB_ICONSTOP "TruckAudioSim requiere Windows 10 u 11 de 64 bits (x64)."
    SetErrorLevel 2
    Abort
  ${EndIf}
  !insertmacro RequireClosed
FunctionEnd

Function un.onInit
  !insertmacro RequireClosed
FunctionEnd

Section "TruckAudioSim"
  SetShellVarContext current
  SetOutPath "$INSTDIR"
  File /r "${PAYLOAD}\*"
  WriteUninstaller "$INSTDIR\Uninstall TruckAudioSim.exe"
  CreateShortCut "$SMPROGRAMS\TruckAudioSim.lnk" "$INSTDIR\TruckAudioSim.exe"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "TruckAudioSim"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "Publisher" "TruckAudioSim"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\TruckAudioSim.exe"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '$\"$INSTDIR\Uninstall TruckAudioSim.exe$\"'
  WriteRegStr HKCU "${UNINSTALL_KEY}" "QuietUninstallString" '$\"$INSTDIR\Uninstall TruckAudioSim.exe$\" /S'
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "EstimatedSize" ${SIZE_KB}
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  SetShellVarContext current
  Delete "$SMPROGRAMS\TruckAudioSim.lnk"
  !include "${UNINSTALL_LIST}"
  Delete "$INSTDIR\Uninstall TruckAudioSim.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKCU "${UNINSTALL_KEY}"
SectionEnd
