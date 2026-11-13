@echo off
rem SPDX-License-Identifier: MIT
setlocal EnableExtensions DisableDelayedExpansion
pushd "%~dp0" || exit /b 1
setlocal EnableDelayedExpansion

set "program=validacion-x64\tas_spatial.exe"
set "wav=validacion-x64\prueba_izquierda_derecha_48k.wav"
set "log=build-win\validation\w0-dolby-atmos.log"
set "result=1"

if not exist "%program%" goto missing
if not exist "%wav%" goto missing

echo TruckAudioSim W0 - Dolby Atmos para auriculares
echo En Configuracion ^> Sistema ^> Sonido, abre la salida fisica de tus auriculares.
echo Selecciona Dolby Atmos para auriculares en Sonido espacial.
echo Si falta esa opcion, configura Dolby Access desde Microsoft Store.
echo Vuelve a esta ventana cuando Atmos este activo en esa salida.
pause
echo.
"%program%" --list-devices
if errorlevel 1 goto finish
echo.
set "outputSelection="
set /p "outputSelection=Escribe el numero de la salida (por ejemplo 0) o pega su ID completo. Intro para salir: "
if not defined outputSelection goto cancel
set "deviceArg=--output-index"
if "!outputSelection:~0,1!"=="{" set "deviceArg=--output-id"

if not exist "build-win\validation" mkdir "build-win\validation"
ver >"%log%" || goto finish
>>"%log%" echo Proveedor indicado por el usuario: Dolby Atmos para auriculares
>>"%log%" echo Selector de salida: %deviceArg% !outputSelection!
>>"%log%" echo WAV: %wav%

echo.
echo Escucha un pitido a la izquierda y dos a la derecha.
"%program%" --smoke-test --wav "%wav%" %deviceArg% "!outputSelection!" --require-spatial >>"%log%" 2>&1
set "result=%errorlevel%"
>>"%log%" echo Codigo de salida: %result%
type "%log%"

if not "%result%"=="0" goto failed
findstr /L /C:"Smoke-test mode: SPATIAL_DYNAMIC" "%log%" >nul || goto incomplete
findstr /L /C:"Smoke-test stream completed: 192000 input frames" "%log%" >nul || goto incomplete

echo.
choice /C IDN /N /M "Elige I si oiste un pitido a la izquierda y dos a la derecha, D si fue al reves, o N si no se distinguio: "
set "heard=%errorlevel%"
if "%heard%"=="1" >>"%log%" echo Escucha: un pitido a la izquierda, dos a la derecha.
if "%heard%"=="2" >>"%log%" echo Escucha: un pitido a la derecha, dos a la izquierda.
if "%heard%"=="3" >>"%log%" echo Escucha: orden sin confirmar.
goto report

:missing
echo Extrae el ZIP completo antes de ejecutar la prueba.
goto finish

:cancel
set "result=0"
goto finish

:failed
echo.
echo La prueba espacial no termino. Revisa el error de la consola y el formato de la salida elegida.
goto report

:incomplete
set "result=1"
>>"%log%" echo La salida no confirma la ruta espacial completa.
echo La salida no confirma la ruta espacial completa.

:report
echo Registro: !CD!\%log%

:finish
echo.
pause
popd
exit /b %result%
