# 0.11 — Extracción de PCM desde el búfer WaveRT

## Función nueva (sí implementada)

`src/driver_port/wavert_dma_tap.c/.h` es un extractor pequeño en **C** sin
asignaciones dinámicas, excepciones, objetos C++ ni dependencias Win32. Puede
leer PCM float32/48 kHz ya mezclado desde un búfer circular de reproducción,
convertir sus muestras al formato `TAS1` existente (40 bytes de cabecera + PCM
intercalado) y registrar frames perdidos por sobrescritura.

- Topologías: estéreo, 2.1 experimental, 5.1 lateral/trasera y 7.1.
- El canal LFE se **conserva**, nunca se fabrica a partir de estéreo.
- **Sin EQ, compresión, remuestreo ni normalización**: la copia conserva los
  bits de cada muestra float32 finita.
- Se comprueba la máscara de canales, longitud del búfer, contadores de
  frame, capacidad de salida, versiones del protocolo y valores NaN/Inf.
- La salida es una secuencia de paquetes `TAS1`, de hasta 960 frames cada
  uno, que `decodeDriverPacket` puede leer de forma idéntica.
- Si el consumidor llega tarde y la posición antigua fue sobrescrita, se
  notifica la pérdida y se salta al frame más antiguo todavía disponible.
- Si el consumidor lee antes de que haya audio listo, se devuelve `EMPTY`.

### Prueba reproducible (sin WDK)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
python tools/make-demo-surround.py
./build/tas_dma_tap_demo --input demo_7_1_48k.wav --output dma_tap_7_1.wav
python tools/check-loopback.py --captured dma_tap_7_1.wav --mode 7_1
```

La prueba `wavert_dma_ring_extractor` verifica buffers que giran, pérdida de
frames, `TAS1` compatible con el descodificador C++, muestras no finitas y
las topologías 2/3/6/8 canales. `dma_tap_pcm_end_to_end` genera y verifica
una señal 7.1 completa a través de la ruta nueva.

## Lo que **todavía no** hace

No crea un endpoint de audio ni un `.sys`, no contiene una modificación
compilable de SysVAD y no transporta PCM desde el kernel a un proceso.
En `tas_dma_tap_demo`, un escritor de prueba actúa en lugar del mezclador de
Windows: **no es reproducción real desde Spotify**.

## Integración con Microsoft SysVAD pendiente

Microsoft documenta que SysVAD **genera tonos sintéticos en loopback** y no
mezcla ni refleja las muestras enviadas a reproducción. Fuente oficial:
https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad

Puntos técnicos identificados en `EndpointsCommon/minwavertstream.cpp`:

1. La clase `CMiniportWaveRTStream` posee el buffer DMA mapeado (`m_pDmaBuffer`)
   y su tamaño (`m_ulDmaBufferSize`). **No basta con leer bytes arbitrarios**:
   un adaptador debe validar que son audio intercalado float32 y que el endpoint
   elegido es un pin de reproducción de sistema, no de loopback/captura.
2. La posición de reproducción se actualiza mediante `UpdatePosition` y
   `GetPosition`, que tiene un bloqueo de posición propio. El extractor exige
   un contador **absoluto y monotónico en frames completados**, no solo el
   offset circular (`PlayOffset`). El módulo no transforma por sí mismo el
   desplazamiento circular a tiempo absoluto.
3. Antes de llamar a `tas_dma_tap_poll` se debe obtener una **instantánea
   consistente** del búfer y del contador; el mapeo debe permanecer válido
   durante la copia. Hay que resolver concurrencia con el motor de audio y
   los cambios de estado `RUN/PAUSE/STOP`.
4. Los paquetes deben ir a una **cola acotada de kernel**, desde la cual un
   servicio de usuario autorizado pueda leerlos con una interfaz de dispositivo
   con ACL. **Nunca** se deben exponer punteros DMA ni publicar la dirección
   del búfer de kernel en un paquete o memoria compartida de usuario.
5. No se debe realizar I/O bloqueante, asignaciones paginables, esperas ni
   llamadas de usuario desde DPC o bajo spinlock. El diseño de la cola, su
   sincronización, cancelación y desmontaje se validará con WDK/Driver Verifier.
6. La salida física de Microsoft Spatial Sound debe ser **otro dispositivo**
   distinto del endpoint de entrada; de lo contrario puede haber realimentación.
7. Se requieren pruebas en máquina virtual, firmas de controlador y HLK para
   distribución. **No desactivar protecciones de Windows para instalarlo.**

Este extractor es el **primer componente C reutilizable** de un futuro
adaptador kernel, no un controlador completo. Aún quedan por desarrollar
la cola, los IOCTL, la semántica de stream, el empaquetado y la firma.
