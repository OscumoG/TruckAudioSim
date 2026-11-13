# v0.12 — Cola PCM e interfaz de lectura SysVAD (borrador implementable)

**Estado real:** existe código C portable para una cola acotada y su integración
con el extractor WaveRT anterior. **No** existe todavía un `.sys` compilable,
un driver registrado, una interfaz de dispositivo publicada ni un controlador
IOCTL operativo. La utilidad `tas_driver_client.exe` es un cliente que se podrá
probar cuando exista ese controlador; por sí sola no recibe Spotify.

## Camino de audio propuesto

```text
Windows Audio Engine -> endpoint WaveRT (futuro SysVAD modificado)
  -> búfer PCM float32 48k finalizado y snapshot consistente
  -> tas_dma_tap_poll
  -> tas_packet_fifo_push (cola de tamaño fijo)
  -> IOCTL_TAS_GET_PACKET (METHOD_BUFFERED; implementación WDK pendiente)
  -> tas_driver_client.exe (usuario)
  -> PipeWriter -> tas_spatial.exe --live
  -> ISpatialAudioClient del dispositivo físico
```

### Qué sí está programado

- `src/driver_port/tas_packet_fifo.c/.h`: cola circular en C, sin memoria
  dinámica, Win32, hilos ni acceso al dispositivo. Memoria suministrada por el
  llamador. Tamaño fijo de 2 a 64 paquetes, máximo 960 frames y 8 canales.
- `src/driver_port/tas_stream_adapter.c/.h`: combina extracción DMA y cola.
  Solo acepta formatos coherentes; no ecualiza, normaliza, mezcla ni aumenta
  graves. El LFE se conserva si la fuente lo incluye.
- `src/driver_port/tas_ioctl_contract.h`: valor de `TAS_IOCTL_GET_PACKET` igual
  a `CTL_CODE(FILE_DEVICE_UNKNOWN,0x800,METHOD_BUFFERED,FILE_READ_ACCESS)`.
  Aún no hay controlador para responder a ese IOCTL.
- `src/windows/driver_client.cpp`: intenta abrir un dispositivo WDK cuando
  exista, lee un paquete TAS1, valida versión, contenido, origen y secuencia,
  y lo envía al mismo pipe local del motor espacial.

La cola descarta los paquetes *más antiguos* al llenarse, mantiene el contador
`dropped_packets` y rechaza muestras NaN/Inf, paquetes malformados, tokens
ajenos y secuencias que retroceden. Se permiten saltos hacia delante para
representar frames perdidos. **No existe sincronización interna**: el futuro
adaptador WDK debe serializar todas las operaciones (por ejemplo, en un worker
PASSIVE_LEVEL). No ejecutar copias PCM prolongadas dentro de un DPC.

## Contrato de IOCTL propuesto (solo diseño)

| Operación | Valor | Dirección | Resultado |
|---|---|---|---|
| `TAS_IOCTL_GET_PACKET` | `0x00226000` | driver → usuario | Un paquete TAS1 exacto, 40 bytes + muestras float32 |

- Sin datos de entrada. No se aceptan punteros suministrados por el usuario.
- Respuesta exitosa: un paquete de tamaño entre 44 y 30 760 bytes, versión 1,
  a 48 kHz; solo canales y máscaras compatibles.
- Cola vacía: `ERROR_NO_MORE_ITEMS`; el cliente duerme 5 ms y reintenta.
- Búfer demasiado pequeño: `ERROR_INSUFFICIENT_BUFFER`, sin consumir el paquete.
- Un único consumidor permitido inicialmente. Cuando se desconecte, el host
  debe limpiar o reiniciar estado de secuencia coherentemente.
- `sourceEndpointToken` identifica el origen para evitar bucles de audio;
  **no autentica al cliente** y debe coincidir con el endpoint virtual elegido.
- El driver WDK tendrá que validar la capacidad real de los búferes recibidos,
  cancelar solicitudes pendientes al apagar el dispositivo, y copiar a un
  búfer de salida del I/O manager sin exponer memoria DMA de kernel.

### Seguridad y privacidad

El dispositivo debe publicar una **interfaz con ACL restrictiva**; nunca abrir
acceso universal de lectura al audio de otras aplicaciones. El diseño final
necesita un servicio autorizado, control exclusivo del consumidor y límites de
memoria. Validar IRQL, colas, cancelación, cambios de formato y teardown con
WDK/Driver Verifier. No ofrecer aún instalación, modo de pruebas en una PC
personal ni instrucciones de desactivar la firma de controladores.

### Cómo probar lo actual, sin driver

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

`bounded_driver_packet_fifo` prueba formato, canales, pérdida al llenarse y
validación de paquetes. `wave_dma_to_bounded_fifo` comprueba una ruta 7.1 real
*de prueba* desde el búfer circular hasta el decodificador TAS1. El antiguo
`dma_tap_pcm_end_to_end` comprueba la preservación de las señales por canal.

La utilidad Windows *no puede ejecutarse sin el futuro driver*:

```powershell
# Únicamente cuando exista el driver firmado y un endpoint virtual instalado:
# 1) tas_spatial.exe --preset profiles\Scania_S_7_1_Fidelidad.json --live --output-index <salida física>
# 2) tas_driver_client.exe --source-index <endpoint virtual> --seconds 60
```

El primer paso práctico de integración WDK es añadir a SysVAD una interfaz de
control separada con ACL, conectar un worker al pin WaveRT de reproducción,
publicar cada paquete completado y probar la apertura/lectura con
`tas_driver_client` sobre una VM Windows. El `driver_client` y el IOCTL no
crean ese endpoint, ni realizan mezclado multistream: eso sigue pendiente.

Fuentes técnicas oficiales:
- SysVAD: https://learn.microsoft.com/en-us/samples/microsoft/windows-driver-samples/sysvad-virtual-audio-device-driver-sample/
- DeviceIoControl: https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-deviceiocontrol
- WDF IOCTL: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfio/nc-wdfio-evt_wdf_io_queue_io_device_control
