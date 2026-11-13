# v0.15 — Coordinador de PCM y ruta PASSIVE_LEVEL para SysVAD

**NO hay todavía driver instalable.** Este cambio añade un coordinador C compilable en Windows/Linux y dos archivos WDK **experimentales, no conectados ni compilados con WDK**:

- `src/driver_port/tas_capture_coordinator.{h,c}` — módulo portable y probado.
- `src/wdk/tas_wdm_capture_hooks.{h,cpp}` — adaptación para el futuro fork de SysVAD, con bloqueo `FAST_MUTEX` y validación de `PASSIVE_LEVEL`.
- `tests/capture_coordinator.cpp` — reinicio, nueva conexión, canales 2/3/6/8, búfer circular sobrescrito y limpieza de PCM inválido.

## Por qué se requiere

El audio de un stream WaveRT puede actualizarse desde temporizadores y DPC. Nuestra cola, su control de sesiones y su lectura de PCM necesitan un único lock y una ruta de trabajo a **PASSIVE_LEVEL**. Está prohibido llamar `tas_capture_poll()` desde una ISR, DPC o mientras se posee un spinlock. Tampoco es seguro leer directamente un búfer WaveRT que Windows esté modificando simultáneamente: el integrador debe obtener un **snapshot coherente** de muestras ya completadas que se conserve estable hasta terminar de procesarlo.

## Integración futura recomendada

1. En el stream render del fork de SysVAD, localizar la transición `KSSTATE_RUN` y notificaciones de progreso. **No** insertar procesamiento PCM pesado en las rutinas a DISPATCH_LEVEL; limitarse a señalar un evento que despierte un hilo de trabajo PASSIVE_LEVEL con ciclo de vida seguro.
2. En el hilo de trabajo, obtener un snapshot de la región DMA **completada** (no de buffers que todavía escribe Windows) junto a su contador monotónico `completed_frames`. Verificar que el formato negociado es 48 kHz, interleaved float32, con máscara de canales 2.0/5.1/7.1 adecuada. Si se negocia otro formato, implementar un conversor explícito antes de usar esta ruta.
3. Inicializar `tas_packet_fifo`, `tas_device_session`, el coordinador y la memoria staging no paginada **por flujo**; no compartir punteros de streams distintos sin mezclarlos de manera correcta en el audio engine.
4. Usar el **mismo** `FAST_MUTEX` para el productor y `TasControlCreate`, `TasControlCleanup`, `TasControlClose`, `TasControlIoctl`.
5. En `RUN`: `TasWdmCaptureStart(ctx, completed_frames)`; en STOP/PAUSE: `TasWdmCaptureStop(ctx)`; en cada snapshot válido llamar `TasWdmCapturePassiveTick` con un límite interno de 4 paquetes/ciclo. Si hay retraso, planificar otra vuelta en el hilo de trabajo, sin retener el lock de forma indefinida.
6. Atender al contador de generación de sesión: cuando se conecta un lector nuevo se descarta todo PCM anterior. Cuando se reinicia el render o retrocede el contador se limpian los paquetes y se resincroniza sin reproducir audio viejo.
7. Al retirar el dispositivo: primero interrumpir y esperar al hilo de trabajo, cancelar IRP pendientes si existieran, cerrar sesiones, limpiar FIFO y solo entonces liberar buffers. Evitar *use-after-free* durante suspensión y desconexión.

## Limitaciones actuales

- **No** crea `DEVICE_OBJECT`, audio endpoint, topología KS, INF ni `.sys` firmado.
- **No** convierte automáticamente formatos distintos a 48 kHz float32.
- **No** ofrece una captura coherente de DMA desde SysVAD: eso depende del integrador WDK y de la ruta exacta del render.
- **No** prueba Dolby Atmos, DTS, Windows Sonic, Spotify ni cámaras ETS2.
- Los avisos de `status` devueltos por la capa WDK no son un ABI nuevo. Para datos PCM se mantiene `TAS_IOCTL_GET_PACKET`.

## Pruebas verificadas

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

La prueba `passive_wavert_session_coordinator` demuestra preservación bit a bit de PCM float32 con los cuatro formatos admitidos, eliminación de audio previo a una conexión, manejo de reinicio del contador, sobreescritura de búfer y limpieza de datos inválidos.

Referencias Microsoft:
- https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad
- https://github.com/microsoft/Windows-driver-samples/blob/main/audio/sysvad/EndpointsCommon/minwavertstream.cpp
- https://learn.microsoft.com/en-us/samples/microsoft/windows-driver-samples/sysvad-virtual-audio-device-driver-sample/
