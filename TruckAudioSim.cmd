@echo off
rem SPDX-License-Identifier: MIT
setlocal EnableExtensions DisableDelayedExpansion
pushd "%~dp0" || exit /b 1
setlocal EnableDelayedExpansion
set "engine=bin-x64\tas_spatial.exe"
set "profile=profiles\Scania_S_7_1_Fidelidad.json"
set "result=1"

echo TruckAudioSim - audio de cabina para Windows
if not exist "%engine%" goto missing
if not exist "%profile%" goto missing
echo Configura Dolby Atmos para auriculares en la salida fisica.
echo Dirige Spotify o VLC a la entrada virtual elegida desde el mezclador de Windows.
echo.
"%engine%" --list-devices
if errorlevel 1 goto finish
echo.
set "source="
set /p "source=Numero de la entrada virtual (Intro para salir): "
if not defined source goto cancel
set "destination="
set /p "destination=Numero de tus auriculares o de la salida Realtek: "
if not defined destination goto cancel
echo.
"%engine%" --preset "%profile%" --input-index "!source!" --output-index "!destination!"
set "result=%errorlevel%"
goto finish

:missing
echo Extrae todo el ZIP y vuelve a abrir este archivo.

:finish
echo.
pause
popd
exit /b %result%

:cancel
popd
exit /b 0
