# Protocolo PCM TAS3 de TruckAudioSim — v0.3 experimental

Esta especificación describe **solamente** el enlace entre dos procesos de usuario Windows. **No es** un protocolo WaveRT/WDK ni un controlador de sonido. No permite aún capturar Spotify o audio de ETS2.

## Conexión

- Servidor: `tas_spatial.exe --preset <perfil.json> --live`.
- Cliente: `tas_stream_sender.exe --wav <archivo.wav>`.
- Transporte: Named Pipe de Windows local, en modo byte, con nombre `\\.\pipe\TruckAudioSimPcm`. Se rechazan clientes remotos con `PIPE_REJECT_REMOTE_CLIENTS`.
- Una conexión por ejecución. El receptor espera al cliente antes de iniciar la salida espacial.
- Siempre utiliza **float32 PCM intercalado little-endian, 48000 Hz**. No hay compresión, EQ ni codificación Dolby/DTS en este enlace.

## Cabecera de 24 bytes

| Offset | Tamaño | Tipo | Valor |
| --- | --- | --- | --- |
| 0 | 4 | ASCII | `TAS3` |
| 4 | 2 | uint16 LE | Versión: `1` |
| 6 | 2 | uint16 LE | Cantidad de canales: `2`, `3`, `6` u `8` |
| 8 | 4 | uint32 LE | Frecuencia: `48000` |
| 12 | 4 | uint32 LE | Máscara de altavoces Windows |
| 16 | 4 | uint32 LE | Formato: `1` = float32 |
| 20 | 4 | uint32 LE | Reservado: `0` |

Máscaras admitidas:

| Audio | Máscara hexadecimal | Orden intercalado |
| --- | --- | --- |
| Estéreo | `0x00000003` | FL FR |
| 2.1 | `0x0000000B` | FL FR LFE |
| 5.1 back | `0x0000003F` | FL FR FC LFE BL BR |
| 5.1 side | `0x0000060F` | FL FR FC LFE SL SR |
| 7.1 | `0x0000063F` | FL FR FC LFE BL BR SL SR |

**Atención:** el modo 2.1 de este protocolo no implica que Windows deba anunciar 2.1 como endpoint estándar del futuro driver. Puede representarse como una entrada estéreo con gestión de graves o como un flujo de prueba de tres canales.

## Datos

Después de la cabecera inicial, el flujo lleva una secuencia de paquetes:

1. `uint32 LE` con el **número de frames** del siguiente paquete (entre `1` y `4800`, máximo 100 ms a 48 kHz).
2. `frames × channels × 4` bytes, muestras intercaladas float32 en el orden indicado por la máscara de canal.
3. Repetir.
4. Un `uint32 LE` de valor `0` indica fin limpio.

El receptor rechaza paquetes demasiado grandes, limpia `NaN` y valores infinitos y limita las muestras a [-1, 1]. Un hilo de lectura introduce los frames en un ring buffer SPSC de hasta 500 ms. Si el renderizador consume más rápido que el emisor, el ring devuelve **silencio temporal**, no interpolación ni refuerzo de volumen.

En un stream real a futuro harán falta gestión explícita de jitter, reinicio de dispositivo, reconexión, sincronización de reloj y telemetría de underruns. Un Named Pipe como este no debe usarse directamente desde código de interrupción o DPC de un driver de kernel.

## Prueba segura

Desde dos consolas Windows, en el mismo directorio del repositorio, después de compilar:

```bat
build\Release\tas_spatial.exe --preset profiles\Scania_S_7_1_Fidelidad.json --live
```

```bat
build\Release\tas_stream_sender.exe --wav demo_stereo_48k.wav
```

El dispositivo de salida elegido por el prototipo es la **salida física predeterminada** de Windows. Activa en esa salida el procesador espacial disponible. No instales drivers adicionales para esta prueba.

La compilación, la negociación con la salida y el comportamiento de los procesadores espaciales de terceros todavía deben verificarse en una computadora Windows real.
