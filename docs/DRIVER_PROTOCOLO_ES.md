# v0.10 — Protocolo PCM y modelo de loopback WaveRT

## Estado honesto

Estos componentes son un **contrato de transporte y una simulación funcional en modo usuario**. **No crean un dispositivo de Windows, no contienen un `.sys`, no sustituyen SysVAD y no permiten aún elegir TruckAudioSim como salida de Spotify.** Tampoco prueban la ejecución de `ISpatialAudioClient` en Windows.

## Código añadido

- `src/common/driver_wire.hpp/.cpp`: protocolo binario de **40 bytes de cabecera**, serializado explícitamente en little-endian, sin punteros y sin compartir estructuras C++ entre kernel y usuario.
- `src/common/wavert_model.hpp/.cpp`: historial circular que consume PCM de un único stream de reproducción **ya mezclado** y lo presenta como loopback, con índices absolutos de frames. Los saltos y frames aún no emitidos se entregan como silencio; no se reemplaza nada por tonos de prueba.
- `src/common/mock_wavert.cpp`: ejecutable reproducible que conecta lector WAV → paquetes PCM → modelo circular → escritor WAV.
- `tests/driver_wire.cpp`, `tests/wavert_model.cpp`, `tests/mock_driver_end_to_end.py`: comprobación de formato, límites, pérdida de muestras y preservación 7.1.

## Contrato binario provisional para el adaptador del futuro driver

| Offset | Tamaño | Campo |
|---:|---:|---|
| 0 | 4 | Magic `TAS1` |
| 4 | 2 | Versión `1` |
| 6 | 2 | Tamaño de cabecera `40` |
| 8 | 4 | Frecuencia `48000` |
| 12 | 4 | Máscara de altavoces Windows |
| 16 | 8 | Índice absoluto del primer frame |
| 24 | 4 | Frames en el paquete (`1..960`) |
| 28 | 2 | Canales: 2, 3, 6 u 8 |
| 30 | 2 | Formato PCM float32 (1) |
| 32 | 4 | Identificador de origen **no criptográfico** |
| 36 | 4 | Flags: 0 en esta versión |
| 40 | variable | Frames intercalados float32 little-endian |

Formato: 48 kHz; los valores float finitos (incluso mayores que 1) se conservan en este protocolo. No hay EQ, compresión, generación de LFE ni conversión de 2 a 8 canales. La máscara 2.1 (`0xB`) se conserva para los tests, pero **no es una topología Windows que debamos prometer a todas las aplicaciones**.

La cabecera `TAS1` **no es una interfaz kernel todavía**: faltan las operaciones de apertura/cierre, autorización de consumidor, control de generación del stream, sincronización de reloj, gestión de pérdidas, antirretroalimentación y una API de lectura segura. El token de origen no autentica un proceso. Tampoco deben transferirse literalmente `std::vector`, `std::mutex` ni `SharedPcmBridge` a un controlador.

## Ensayo local reproducible, sin driver

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Para observar el audio de prueba en 7.1, genera un WAV con `python tools/make-demo-surround.py` y luego ejecuta:

```bash
./build/tas_mock_wavert --input demo_7_1_48k.wav --output mock_loopback_7_1.wav
python tools/check-loopback.py --captured mock_loopback_7_1.wav --mode 7_1
```

En Windows compilado con MSVC el ejecutable estará normalmente en `build\\Release\\tas_mock_wavert.exe`; el flujo continúa siendo una **simulación en usuario**, no un endpoint.

## Próxima integración real en SysVAD (pendiente)

1. Hacer un fork controlado de la muestra oficial de Microsoft y respetar su licencia separada.
2. Elegir un único render endpoint de prueba y sus formatos WaveRT 2.0/5.1/7.1; verificar máscaras y reloj.
3. Sustituir la ruta de tonos sintéticos de loopback por el **PCM mezclado realmente reproducido**, antes de adjuntar el protocolo a los buffers de usuario. El ejemplo SysVAD no realiza esta mezcla por defecto.
4. Definir una interfaz kernel-usuario con límites por paquete, ACL del dispositivo, estado de stream/generación, cancelación y recuperación de cierres.
5. Compilar y probar en **VM Windows con WDK**; usar validaciones HLK, firma y despliegue seguro antes de pruebas en equipos personales. **Nunca pedir desactivar la seguridad de Windows para instalar el controlador.**

Esta entrega permite validar el tratamiento PCM, pero **no demuestra todavía que SysVAD se haya adaptado ni que Atmos, DTS y Windows Sonic hayan sido probados**.
