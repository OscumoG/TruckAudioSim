## v0.19 — prueba inmediata de sonido espacial 7.1

**La forma más fácil de continuar ahora:** abre `Probar-7.1-8-objetos.cmd`
en Windows. No cambia controladores ni requiere instalar nada. Esta prueba
funciona con el ejecutable Windows existente. Consulte `docs/PRUEBA_7_1_V019.md`.

---

# TruckAudioSim v0.17.0 — audio en vivo en Windows

`TruckAudioSim.cmd` permite seleccionar una salida virtual de entrada y
reproducir su mezcla en los auriculares con Dolby Atmos activo. Los
ejecutables Windows x64 están incluidos. La [guía W1](docs/W1_SALIDA_VIRTUAL_WINDOWS.md)
explica cómo probar la ruta con un endpoint existente. El controlador propio
está en etapa de laboratorio y su instalación firmada sigue pendiente.

---

# TruckAudioSim v0.16.0 — Guía rápida

Para la primera prueba W0 en Windows 11 x64, activa **Dolby Atmos para auriculares** en la salida física y abre `Probar-Dolby-Atmos.cmd` desde la carpeta extraída. La [guía de validación Windows](docs/VALIDACION_WINDOWS.md#probar-dolby-atmos-para-auriculares) explica cómo configurar Dolby Access, leer el registro y reproducir un WAV con el perfil Scania S.

**No ejecutes instaladores de controladores.** Esta versión trae código fuente y una interfaz WDM experimental que todavía necesita Windows Driver Kit; no incluye `.sys` firmado ni tarjeta virtual instalable.

Para probar el núcleo (Linux, o Windows con Visual Studio/CMake):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Para estudiar el nuevo dispositivo de control WDM, lee `docs/SYSVAD_CONTROL_DEVICE_V016_ES.md`.

---

# TruckAudioSim v0.15.0 — Inicio rápido

Esta versión NO instala controladores y NO sustituye todavía a Trucker AUX.

## Guía de pruebas anteriores (Windows 11)

Esta es una **versión experimental de código fuente**. No instales controladores, no sustituyas dispositivos de Windows y no esperes que capture Spotify sin duplicación todavía.

1. Instala Visual Studio 2022 con **Desktop development with C++** y el Windows SDK.
2. Extrae el ZIP y abre `x64 Native Tools Command Prompt for VS 2022` en su carpeta.
3. Ejecuta `tools\build-windows.cmd`.
4. Identifica las salidas con `build\Release\tas_spatial.exe --list-devices`.
5. Abre `docs\LIVE_WASAPI_SHM_ES.md` para probar la nueva captura de audio real mediante tres terminales. Elige **origen y salida física diferentes**.

**Sin Windows Sonic/Dolby/DTS activado en la salida física**, el renderizador puede informar que no tiene objetos 3D dinámicos. Esta aplicación no activa ni concede licencias espaciales.

Para las pruebas antiguas con un WAV en lugar de audio en vivo, sigue `docs\SHARED_PCM_BRIDGE_ES.md`; el formato original continúa funcionando.

**Verificación histórica de una versión anterior:** 17 pruebas automatizadas del núcleo en Linux; herramientas y audio Windows pendientes de verificar en Windows nativo. **No hay driver `.sys` instalable**.


## Prueba del nuevo transporte del driver (sin instalar nada)

Para comprobar que la simulación de WaveRT conserva los ocho canales:

```bat
python tools\make-demo-surround.py
build\Release\tas_mock_wavert.exe --input demo_7_1_48k.wav --output mock_loopback_7_1.wav
python tools\check-loopback.py --captured mock_loopback_7_1.wav --mode 7_1
```

En los últimos mensajes debe aparecer `PASS: PCM por canal conservado`. Este resultado no implica que ya haya un controlador `.sys` instalable. Ver `docs\DRIVER_PROTOCOLO_ES.md`.


## Nueva prueba WaveRT DMA (v0.11.0)

Compila en Windows con Visual Studio (consola x64) o Linux:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Para una prueba 7.1 de la ruta DMA (desde `build/Release` en MSVC o `build` en Linux):

```powershell
python tools/make-demo-surround.py
.\build\Release\tas_dma_tap_demo.exe --input demo_7_1_48k.wav --output dma_tap_7_1.wav
python tools/check-loopback.py --captured dma_tap_7_1.wav --mode 7_1
```

**No instala controladores ni reproduce Spotify desde una salida virtual.**
Ver `docs/SYSVAD_DMA_TAP_ES.md` para el estado real de la integración.


## Prueba nueva de v0.12.0 (sin driver)

Desde una terminal con CMake y compilador C++20:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Estas pruebas incluyen el paso de audio 7.1 por el extractor de DMA y una
cola de paquetes de tamaño fijo. No requieren Windows.

En Windows, el nuevo `tas_driver_client.exe` **no podrá abrir un driver** hasta
que terminemos una implementación WDK firmada. No intentes instalar SysVAD
de demostración en tu PC principal para probar esta versión.

Documentación: `docs/DRIVER_IO_CONTROL_ES.md`.


## v0.13.0: probar lectura tipo DeviceIoControl (SIN driver)

En Linux (en Windows, usar `build\Release\tas_ioctl_demo.exe` si la compilación MSVC funciona):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
python tools/make-demo-surround.py
./build/tas_ioctl_demo --input demo_7_1_48k.wav --output ioctl_loopback_7_1.wav
python tools/check-loopback.py --captured ioctl_loopback_7_1.wav --mode 7_1
```

La demostración usa el despachador `tas_ioctl_dispatch` en **modo usuario**. **No invoca DeviceIoControl real de Windows**, no crea salidas de sonido ni instala drivers. Sirve para comprobar el contrato de paquetes y la conservación del PCM mientras desarrollamos la integración WDK.

Consulta `docs/WDK_BOUNDARY_V013_ES.md`. El código WDM de ejemplo no se ha compilado en el Windows Driver Kit.


## v0.15.0: verificar el control de sesión (sin driver)

En Linux:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

En Windows, desde una consola de Visual Studio:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

El test `wdm_single_reader_session_lifecycle` comprueba que los paquetes PCM solo llegan a la sesión autorizada en el modelo portable y que se borren al cerrar.

**Precaución:** estos comandos no crean ni instalan un driver de Windows. `src/wdk/tas_wdm_session_adapter.cpp` es una plantilla para una integración futura con SysVAD, no una DLL ni un `.sys` compilable con el CMake convencional. La compilación Windows no se ha verificado todavía.

Lee `docs/WDK_SESSION_V014_ES.md` para el estado real y los siguientes requisitos.


## Prueba nueva de v0.15.0

Compila el núcleo y ejecuta solamente el nuevo test:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build -R passive_wavert_session_coordinator --output-on-failure
```

Prueba 2/3/6/8 canales, reconexión, reinicio WaveRT y limpieza por PCM inválido. El adaptador WDK sigue siendo una **plantilla no compilada**; no instalar controladores en la PC principal. Lee `docs/SYSVAD_PASSIVE_CAPTURE_V015_ES.md` para los pasos de WDK.
