# Requisitos verificables del futuro driver WaveRT (v0.6.0)

La [guía W1](W1_SALIDA_VIRTUAL_WINDOWS.md) describe el driver de laboratorio.
Este documento mantiene los criterios de aceptación para comprobar que el
endpoint entrega las muestras originales de las aplicaciones.

## Punto de partida oficial

- [Microsoft SysVAD](https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad)
- [Microsoft SysVAD — limitación de mezcla/loopback](https://learn.microsoft.com/en-us/samples/microsoft/windows-driver-samples/sysvad-virtual-audio-device-driver-sample/)
- [WASAPI Loopback Recording](https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording)
- [Spatial Sound](https://learn.microsoft.com/en-us/windows/win32/coreaudio/spatial-sound)

SysVAD es una base de desarrollo de un endpoint Windows; su ruta de loopback
no entrega automáticamente PCM mezclado y puede generar tonos. Por ello no
basta con poner el nombre TruckAudioSim en el INF del ejemplo.

## Contrato de producto

1. Windows reconoce **TruckAudioSim Virtual Audio** como render endpoint.
2. Se admiten **2.0, 5.1 y 7.1** a 48 kHz con máscaras `0x3`, `0x60F`/`0x3F`
   y `0x63F` respectivamente. 2.1 se resuelve dentro del motor como estéreo +
   subwoofer opcional; no se exige 3ch como modo universal de Windows.
3. PCM entregado al endpoint de reproducción debe atravesar a la ruta de
   captura loopback preservando canales **sin mezclas, tonos sustituidos, EQ
   ni bass boost**. La mezcla de aplicaciones es responsabilidad de un motor
   de audio correctamente diseñado, no un simple `memcpy` de streams independientes.
4. La salida espacial final es un **endpoint físico distinto** del virtual.
   Evitar por ID el mismo endpoint antes de abrir cualquier captura.
5. Desinstalación limpia, sin ocultar drivers ni desactivar protección de
   firmas/integridad de Windows. Firma y pruebas HLK antes del uso público.
6. Frente a fallos del servicio, el driver no debe dejar a Windows bloqueado;
   debe tener tratamiento de underrun, desbordamiento y teardown de streams.
7. El audio original de ETS2 seguirá en la salida física; Spotify puede
   dirigirse al endpoint virtual desde el mezclador de volumen.

## Probar si el loopback transporta PCM real

1. Generar los archivos de prueba: `python tools/make-demo-surround.py`.
2. Configurar un endpoint **de prueba** a 48 kHz y salida 7.1 real en Windows.
3. Abrir la captura de ese endpoint por índice, guardando a WAV:

   `tas_endpoint_capture.exe --input-index N --seconds 18 --save-wav recorded.wav`

4. Reproducir `demo_7_1_48k.wav` hacia el endpoint capturado, **sin añadir
   ecualizadores o upmixers**. Iniciar poco después de activar la captura.
5. Evaluar canales y detectar tonos sustituidos:

   `python tools/check-loopback.py --captured recorded.wav --mode 7_1`

El verificador examina la frecuencia piloto original de cada canal en una
ventana de un segundo, requiere potencia suficiente y detecta fugas superiores
al umbral. Devuelve código 0 únicamente si **todos** los canales superan la
prueba; códigos 1/2 indican fallo en contenido o formato.

**Limitaciones:** se requiere entrada WAV PCM16 o float32, 48 kHz y máscara
correcta, y un archivo grabado de al menos 1 segundo. Es una prueba de
identidad de canales, no una medición de latencia profesional, distorsión,
respuestas al impulso ni conformidad HLK. La prueba 2.1 sirve para nuestros
archivos; el driver final puede exponer solo 2.0/5.1/7.1 al sistema.

## Orden de construcción pendiente

1. Partir del SysVAD oficial bajo sus condiciones y licencias.
2. Implementar transporte real PCM de render a loopback, mezclando streams
   de manera adecuada y con buffers/temporización seguros.
3. Anunciar formatos y topologías correctos.
4. Validar cada canal con el verificador de esta entrega.
5. Comprobar la captura en vivo `tas_endpoint_capture` y la salida espacial
   `tas_spatial` en **dos endpoints distintos**.
6. Preparar INF/instalador, firma de producción, pruebas HLK y recuperación.

**No ejecutar utilidades de instalación de drivers del ejemplo oficial en tu
Windows habitual sin una compilación verificada y un entorno de pruebas.**
