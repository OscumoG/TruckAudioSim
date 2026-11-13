@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title TruckAudioSim - 7.1 ocho objetos espaciales
if not exist "bin-x64\tas_spatial.exe" (
  echo ERROR: Falta bin-x64\tas_spatial.exe
  pause
  exit /b 1
)
if not exist "validacion-x64\prueba_7_1_canales_48k.wav" (
  echo ERROR: Falta el archivo de prueba 7.1.
  pause
  exit /b 1
)
echo =======================================================
echo   TruckAudioSim - Prueba 7.1 de ocho fuentes discretas
echo =======================================================
echo.
echo Selecciona Dolby Atmos para auriculares o Windows Sonic
echo sobre la salida FISICA; no selecciones el endpoint virtual.
echo Ajusta el volumen a un nivel moderado.
echo.
"bin-x64\tas_spatial.exe" --list-devices
if errorlevel 1 (
  echo No se pudieron enumerar las salidas de audio.
  pause
  exit /b 1
)
echo.
set "deviceIndex="
set /p "deviceIndex=Indice de salida FISICA [0]: "
if not defined deviceIndex set "deviceIndex=0"
echo(%deviceIndex%|findstr /r "^[0-9][0-9]*$" >nul
if errorlevel 1 (
  echo El indice debe ser un numero entero no negativo.
  pause
  exit /b 2
)
echo.
echo El audio suena por orden: FL, FR, FC, LFE, BL, BR, SL, SR.
echo Cada canal dura 1 segundo. LFE es un grave sutil.
echo.
"bin-x64\tas_spatial.exe" --preset "profiles\Diagnostico_7_1_Objetos.json" --wav "validacion-x64\prueba_7_1_canales_48k.wav" --output-index %deviceIndex%
set "result=%errorlevel%"
echo.
if "%result%"=="0" (
  echo El motor termino de reproducir. Esta prueba no demuestra
  echo por si sola que el controlador virtual este instalado.
) else (
  echo Fallo la reproduccion. Codigo de salida: %result%
)
pause
exit /b %result%
