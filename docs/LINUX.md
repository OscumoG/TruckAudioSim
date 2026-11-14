# TruckAudioSim en Linux x86-64

La edición Linux usa el mismo editor y perfiles de cabina. Captura PCM estéreo, 2.1, 5.1 o 7.1 desde un monitor de PipeWire/PulseAudio. Calcula la salida estéreo binaural para auriculares con HRTF SOFA. La orientación y las posiciones de las bocinas influyen en la mezcla. El motor espacial de Windows usa su propia salida Dolby Atmos, DTS o Windows Sonic.

## Ubuntu y PipeWire

1. Instala las dependencias con `sudo apt install pulseaudio-utils pavucontrol libpulse0 libmysofa1 playerctl`. Conserva tu sesión PipeWire con `pipewire-pulse` activa.
2. Extrae el paquete Linux en tu carpeta personal. Ejecuta `./start.sh` desde la carpeta extraída.
3. En **Audio**, selecciona **TruckAudioSim_Input** en **Capture from** y tus auriculares en **Headphones / output**. La aplicación crea la salida virtual en la sesión de audio. Si el servidor impide crearla, puedes elegir una salida existente que tenga un monitor de captura.
4. Reproduce música y abre **Open pavucontrol → Playback**. Envía Spotify, VLC o el navegador a **TruckAudioSim_Input**. Deja ETS2/ATS en tus auriculares o altavoces habituales. El reproductor determina los canales PCM de origen.
5. Elige un perfil y pulsa **Start**. Puedes elegir **Spotify** o **VLC** en la pausa por electrónica si el reproductor publica una sesión MPRIS. Se requiere `playerctl` para esa función.

El motor procesa a 48 kHz y el servidor de audio convierte las frecuencias del reproductor y de tu dispositivo. Usa el HRTF de `/usr/share/libmysofa/default.sofa`. Para usar otro conjunto, inicia la aplicación con `TAS_HRTF_SOFA=/ruta/al/archivo.sofa ./start.sh`.

Los atajos globales existentes usan APIs de Windows. En Linux el volumen y el silencio se controlan desde el editor. Si tu escritorio tiene soporte de bandeja, `TAS_LINUX_TRAY=1 ./start.sh` habilita el icono y permite ocultar la ventana al cerrarla.

## ETS2 o ATS bajo Steam Proton

1. En Steam, activa **Forzar el uso de una herramienta de compatibilidad de Steam Play** para el juego.
2. Con el juego cerrado, en TruckAudioSim selecciona **Camera → Install ETS2 / ATS plugins…** y apunta a la carpeta del juego. Si te pide elegir, selecciona **Proton / Wine**. Los DLL son los mismos de Windows y van en `bin/win_x64/plugins/`.
3. En **Opciones de lanzamiento** de Steam, pon la ruta absoluta al script del paquete seguida por `%command%`. Las comillas permiten espacios en la ruta.

   ```text
   "/ruta/a/TruckAudioSim-v0.24.0-Linux-x64/proton.sh" %command%
   ```

4. Inicia el juego y acepta el aviso del SDK. En el editor usa **Camera → Mode → Raw Camera** para leer la cámara del plugin antiguo cuando sea compatible con la versión del juego. También se conserva **Adjusted Telemetry** para seguimiento dentro de la cabina.

El script inicia el juego y el puente dentro de la misma sesión de Proton. El editor puede arrancar antes o después. Los datos se intercambian en `~/.local/share/TruckAudioSim/telemetry/`, una carpeta de tu usuario con permiso 700. Para quitar el puente al iniciar, borra la opción de lanzamiento de Steam.

## WineHQ

Con el mismo prefijo del juego, usa el mismo script. Ejemplo con Wine de 64 bits:

```bash
WINEPREFIX="$HOME/.wine-ets2" /ruta/al/paquete/proton.sh wine64 "/ruta/al/juego/bin/win_x64/eurotrucks2.exe"
```

El entorno Wine debe exponer los archivos locales mediante la unidad `Z:`. Los DLL se instalan en la misma ruta que para Proton.

## ETS2 o ATS nativos de Linux

Usa **Install ETS2 / ATS plugins…** y, si aparece la elección, selecciona **Native Linux**. Se instala `trucker_aux_telemetry.so` en `bin/linux_x64/plugins/`. El plugin del SDK proporciona identificación del vehículo, movimiento de la cabeza dentro de la cabina y electrónica. Para este modo selecciona **Adjusted Telemetry**. La cámara libre y exterior exacta requiere el DLL de cámara en Proton/Wine.

## Compilar

En Ubuntu instala `build-essential cmake pkg-config python3 libpulse-dev libmysofa-dev` y Node.js 22 o posterior. Desde la raíz del repositorio:

```bash
git submodule update --init
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release -DTAS_BUILD_LINUX_DESKTOP=ON
cmake --build build-linux --parallel
ctest --test-dir build-linux --output-on-failure
python3 tools/make-channel-identity.py
cd desktop
npm ci
npm test
npm start
```

Para empaquetar, genera también `tas_proton_bridge.exe` y `tas_proton_launch.exe` con los targets de CMake en Windows x64. La compilación Windows de GitHub Actions incluye ambos ejecutables. Desde la raíz, ejecuta `desktop/linux/package.sh build-linux RUTA_HELPERS RUTA_SALIDA`. Para el editor desde fuente, usa un directorio de compilación alternativo con `TAS_LINUX_BUILD`.

- [Servidor compatible con PulseAudio de PipeWire](https://docs.pipewire.org/page_module_protocol_pulse.html)
- [libmysofa](https://github.com/hoene/libmysofa)
- [SCS Telemetry SDK](https://modding.scssoft.com/wiki/Documentation/Engine/SDK/Telemetry)
