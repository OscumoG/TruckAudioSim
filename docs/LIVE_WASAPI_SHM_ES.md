# Audio real a través del puente compartido (v0.9, Windows 11)

**Prototipo experimental.** `tas_shm_capture.exe` obtiene una **copia** por WASAPI Loopback del audio mezclado en una salida de Windows. **No crea un driver, no redirige Spotify ni silencia la reproducción original.** Por ello puedes escuchar sonido duplicado. Evita subir mucho el volumen mientras experimentas.

## Condiciones

- Compilar con Visual Studio 2022 + Windows SDK usando `tools\build-windows.cmd`.
- El endpoint capturado debe utilizar 48 kHz, PCM16 o float32, y máscara de altavoces 2.0/2.1/5.1/7.1 compatible. No hay remuestreo automático en este adaptador.
- Debes utilizar **dos dispositivos distintos**: uno como origen de loopback y otro como **salida física espacial**. La protección impide usar el mismo identificador de endpoint en ambas rutas.
- Configurar Windows Sonic, Dolby Atmos o DTS (si está habilitado) en la salida **física**. La aplicación utiliza Microsoft Spatial Sound, pero **no identifica ni modifica la licencia ni el procesador seleccionado**.

## Paso 1. Enumerar los dispositivos

```bat
build\Release\tas_spatial.exe --list-devices
```

Ejemplo: supongamos **índice 0** como dispositivo capturado y **índice 1** como auriculares físicos. Son ejemplos: los índices reales varían. Lo más seguro es utilizar identificadores `--input-id`/`--source-id`/`--output-id` persistentes.

## Paso 2. Abrir tres consolas en este orden

**Terminal 1: el renderizador espacial sobre los auriculares físicos.**

```bat
build\Release\tas_spatial.exe --preset profiles\Scania_S_7_1_Fidelidad.json --live --output-index 1
```

**Terminal 2: búfer compartido con identificación del origen.**

```bat
build\Release\tas_shm_bridge.exe --channels 2 --source-index 0 --seconds 90
```

**Terminal 3: captura en vivo del mismo dispositivo de origen.**

```bat
build\Release\tas_shm_capture.exe --input-index 0 --seconds 90
```

Reproduce una fuente de audio hacia el endpoint 0. El capturador llena el búfer compartido y el puente entrega PCM al renderizador espacial. La reproducción original sigue presente en el endpoint 0: este flujo no sirve por sí solo para sustituir a VB-CABLE.

Si el origen es 7.1, selecciona `--channels 8`; para 5.1 usa `--channels 6` (o añade `--5.1-back` al puente si los canales son BL/BR en lugar de SL/SR). El capturador rechaza los formatos que no coinciden con lo anunciado por el puente, para evitar mezclar canales indebidamente.

## Comprobaciones de seguridad y estabilidad

- Antes de iniciar la captura, se comprueban **máscara de canal, frecuencia, número de canales y token de identificación del origen**.
- Un mutex de sesión impide lanzar dos emisores simultáneos sobre el mismo búfer compartido.
- Se rechaza un intento de capturar del mismo endpoint físico utilizado para renderizar, cuando ambos identificadores están presentes.
- `dropped > 0` indica desbordamiento y pérdida de audio; `padded > 0` indica períodos sin muestras. Ambos deben medirse antes de decidir la latencia y el tamaño óptimos.
- El reloj de WASAPI y el del renderizador pueden derivar con el tiempo: **todavía no existe compensación de deriva o resampling adaptativo**, por lo que esta versión está limitada a pruebas experimentales.

## Limitaciones

No hay driver WaveRT/SysVAD, no hay enrutamiento automático de aplicaciones, no hay control en vivo de cámara o electrónica de ETS2. No se ha compilado ni ejecutado `tas_shm_capture.exe` en un PC Windows en este entorno. Las pruebas automatizadas realizadas corresponden al núcleo portátil, no a la reproducción audible de esta ruta completa.
