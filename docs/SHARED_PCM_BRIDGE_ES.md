# Puente PCM compartido — prototipo v0.9.0

**Experimental. No instala un driver. No captura Spotify ni modifica ETS2.**

Esta ruta WAV continúa disponible en la v0.9. Para audio de un dispositivo existente, consulta `LIVE_WASAPI_SHM_ES.md`.

Esta entrega valida el flujo de *frames intercalados* y el ritmo de lectura/escritura antes de conectar un controlador WaveRT real. Usa memoria compartida de Windows limitada a la sesión del usuario y un único productor/consumidor. No es una interfaz de kernel; el controlador necesitará un contrato IOCTL propio y una política de acceso explícita.

## Ruta de prueba

```text
WAV estéreo / 2.1 / 5.1 / 7.1, float32 o PCM16
   │  tas_shm_sender (conversión a float32 en WavReader)
   ▼
Local\\TruckAudioSim_UserModeBridge_v1  [SPSC, 8192 frames, 48kHz]
   │  tas_shm_bridge (480 frames cada 10ms)
   ▼
\\.\pipe\TruckAudioSimPcm       [protocolo TAS3 existente]
   │  tas_spatial --live
   ▼
Windows Sonic / Dolby Atmos / DTS, cuando el dispositivo físico admita objetos espaciales
```

## Cómo probarlo (Windows 11 / consola de desarrollador VS 2022)

Compilar con `tools\build-windows.cmd`. En tres terminales, desde la raíz del proyecto:

**Terminal 1 — salida física seleccionada:**

```bat
build\Release\tas_spatial.exe --preset profiles\Scania_S_7_1_Fidelidad.json --live --output-index 1
```

Antes consulta `tas_spatial --list-devices` para elegir el índice **de tus auriculares físicos**. Necesitarás activar la opción espacial en esa salida. Nunca selecciones como salida el endpoint que estás capturando.

**Terminal 2 — crear el búfer y enviar al motor:**

```bat
build\Release\tas_shm_bridge.exe --channels 2 --seconds 30
```

**Terminal 3 — fuente PCM de prueba:**

```bat
build\Release\tas_shm_sender.exe --wav demo_stereo_48k.wav
```

Para probar 7.1 real, genera el archivo de muestra con `python tools\make-demo-surround.py` y utiliza `--channels 8` en el puente. El archivo WAV debe tener los mismos canales, máscara y frecuencia que el puente; para 5.1 también admite `--5.1-back`.

La consola del puente informa frames descartados o completados con silencio. Los *padded frames* de inicio y fin son esperables. Los *dropped frames* durante audio activo indican que el emisor adelanta al lector.

## Comportamiento y limitaciones

- PCM de punto flotante a 48 kHz, sin filtros EQ, compresión, bass boost ni síntesis de LFE.
- 8192 frames de capacidad máxima (~171 ms a 48 kHz); no supone una latencia real de 171 ms, que dependerá de la ocupación y del dispositivo de salida.
- Intercambio de un productor con un consumidor. Escritura y lectura separadas con cursores atómicos de 64 bits. Si no hay datos, la lectura entrega silencio. Si el búfer está lleno, descarta los **frames nuevos**, no sobrescribe los pendientes.
- El ejemplo utiliza memoria compartida creada por *un proceso de usuario*. No expone PCM del sistema ni crea un endpoint visible como dispositivo de reproducción de Windows.
- El puente **no puede utilizarse directamente desde kernel**: el controlador firmado requerirá un protocolo de transferencia diferente, validación de rangos y bloqueo de memoria.
- La compilación de estas dos herramientas en Windows no se ha comprobado en este entorno Linux.

## Próximos pasos reales del driver

1. Crear un endpoint render WaveRT con formatos 2.0/5.1/7.1 explícitos, basado en SysVAD con licencia y atribución revisadas.
2. Sustituir el loopback simulado de SysVAD por un transporte de PCM real y seguro hacia el servicio en usuario; no compartir directamente estructuras C++ ni punteros de kernel.
3. Admitir varias aplicaciones: mezcla gestionada por el motor de Windows / captura del mix bajo condiciones explícitas, no copiar búferes de un stream arbitrario.
4. Medir continuidad, separación de canales, saturación, interrupciones y restauración del audio físico.
5. Construir con Visual Studio + Windows Driver Kit, revisar firma y validar en una VM de pruebas **antes** de instalar cualquier `.sys` en el equipo principal.

Documentación de Microsoft: https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad
