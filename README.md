# TruckAudioSim

Escucha Spotify, VLC u otra fuente de audio como si las bocinas estuvieran en la cabina de tu camión en **Euro Truck Simulator 2** o **American Truck Simulator**. Incluye el editor 3D adaptado de Trucker AUX y 150 perfiles Realista, 5.1 y 7.1. También puedes usarlo para escuchar música con el juego cerrado.

## Instalar en Windows

1. Descarga el instalador cuando esté disponible en [Releases](https://github.com/OscumoG/TruckAudioSim/releases). Ejecuta `TruckAudioSim-v0.23.6-Setup-Windows-x64.exe` en Windows 10 u 11 de 64 bits y abre **TruckAudioSim** desde Inicio. El instalador beta está sin firma digital.
2. Con el juego cerrado, abre **Camera → Install ETS2 / ATS plugins…** y selecciona la carpeta del juego. Si necesitas copiar los DLL manualmente, están en la carpeta `plugin` de TruckAudioSim. Coloca `scs-telemetry.dll` y `trucker_aux_camera.dll` en `bin\win_x64\plugins\` del juego.
3. Abre el juego y acepta el aviso de funciones avanzadas del SDK. En TruckAudioSim selecciona **Camera → Mode → Raw Camera** para seguir la vista dentro y fuera de la cabina.

## Configurar el audio

1. Activa **Dolby Atmos para auriculares**, **Windows Sonic** o **DTS Headphone:X** en la salida física de Windows. La aplicación usa el proveedor espacial que tengas activo.
2. En el mezclador de Windows, dirige el reproductor a un dispositivo de reproducción disponible. Desde v0.23.4, WASAPI convierte automáticamente la frecuencia de captura a 48 kHz para el motor espacial. Esto permite usar dispositivos configurados a 96 o 192 kHz si su controlador admite la conversión en modo compartido. Puedes usar una salida virtual, como **Steam Streaming Speakers**, o una salida física que tengas libre, como **Realtek Digital Output (S/PDIF)**, si Windows la muestra activa y admite captura WASAPI loopback. Deja el audio del juego en tu salida habitual.
3. En **Audio → Capture from**, selecciona el mismo dispositivo al que enviaste el reproductor. En **Headphones / output**, elige tus auriculares u otra salida física distinta del dispositivo de captura. Pulsa **Start**.
4. En **Layout → Preset**, elige el perfil de tu camión. **Volume** llega al 1000%. Empieza al 100% si antes usabas un valor alto para compensar el nivel de versiones anteriores. A niveles altos, los picos pueden saturar.

La edición beta incluye el motor y los plugins del juego. El controlador SysVAD experimental queda en el código fuente. Los perfiles 5.1 y 7.1 admiten canales discretos cuando la fuente y el dispositivo de captura los entregan.

## Mientras juegas

- **Atajos de volumen.** Configura Volume Up, Volume Down y Mute en Controls. El volumen cambia en pasos de 10 puntos y un aviso muestra el nivel o «Silenciado» incluso con la aplicación en la bandeja. En el juego, usa modo ventana sin bordes si la pantalla completa exclusiva oculta el aviso.

- **Cámara exterior.** La música pierde nivel con la distancia y queda inaudible a partir de 40 m. Si tu versión del juego deja de entregar la cámara, **Adjusted Telemetry** permite seguir el giro de la cabeza dentro de la cabina.
- **Electrónica.** En **Pause player on electrics off**, elige Spotify o VLC. La pausa depende de que el reproductor exponga una sesión multimedia compatible en Windows.
- **Segundo plano.** La X oculta la ventana. Reábrela desde el icono de la bandeja junto al reloj, o usa **clic derecho → Salir** para detener el motor.
- **Perfiles propios.** Usa **File → Import → Preset files** para importar JSON de Trucker AUX, o **Data from older version** para migrar ajustes y perfiles de la edición ZIP.

## Compilar

En Windows, instala Visual Studio 2022 con las herramientas de C++, Windows SDK, CMake, Python 3 y Node.js. Desde la raíz:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
python tools/make-demo-wav.py
python tools/make-channel-identity.py
ctest --test-dir build -C Release --output-on-failure
New-Item -ItemType Directory -Force bin-x64
Copy-Item build/Release/tas_spatial.exe, build/Release/tas_media_control.exe bin-x64/
cd desktop
npm ci
npm test
npm run dist
```

Los WAV de diagnóstico se generan con los dos scripts de Python del ejemplo.

Para recompilar el plugin de cámara, inicializa el submódulo con `git submodule update --init` y ejecuta `native/camera-plugin/build.cmd`.

Para compilar el instalador con NSIS, usa `python desktop/installer/build.py <carpeta-win-unpacked> <ruta-del-instalador.exe>` desde la raíz. Más información en la [guía de perfiles](docs/PERFILES_CABINA.md) y el [historial de cambios](CHANGELOG.md).

## Licencia

El código de TruckAudioSim se publica bajo [MIT](LICENSE). La interfaz y el editor se basan en Trucker AUX de SergeyDoes, también bajo MIT. Los componentes de terceros conservan sus avisos y licencias, incluida [MS-PL](driver/sysvad/LICENSE-MS-PL.txt) para SysVAD. Consulta [los créditos](ATTRIBUTION.md).
