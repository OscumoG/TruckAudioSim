@echo off
rem TruckAudioSim ETS2 Live: no modifica ExecutionPolicy global ni instala drivers.
setlocal EnableExtensions
pushd "%~dp0" || exit /b 1
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\ets2-live.ps1"
set "result=%errorlevel%"
echo.
if not "%result%"=="0" echo La prueba ha terminado con un error. Consulta el mensaje anterior.
echo Puedes cerrar esta ventana. Recuerda RESTAURAR MANUALMENTE la salida de ETS2 desde el mezclador de Windows.
pause
popd
exit /b %result%
