# Driver virtual — fase siguiente (no implementado)

**Esta carpeta no es un driver. No contiene `.sys`, paquete INF instalable ni firma.**

La v0.5 introduce `tas_endpoint_capture`, que utiliza la API pública WASAPI para leer la mezcla PCM del loopback de un endpoint de audio. En lugar de añadir inmediatamente IOCTL propietarios para hablar con un kernel driver, el primer prototipo de SysVAD se limitará a exponer un endpoint render con loopback **real y validado**, que nuestro capturador puede leer como cualquier otra salida de audio.

1. Partir del ejemplo oficial [Microsoft SysVAD](https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad), que no implementa un cable virtual listo para utilizar.
2. Crear un endpoint virtual WaveRT de reproducción, sin instalar filtros en los dispositivos de salida existentes.
3. Anunciar correctamente 2.0, 5.1 y 7.1 a 48 kHz. La opción 2.1 será del editor: no es una topología de Windows universal.
4. Completar loopback PCM real. Microsoft advierte que el loopback de SysVAD puede producir un **tono de demostración**, lo que NO serviría para Spotify.
5. Utilizar `tas_endpoint_capture` para enviar los canales hacia `tas_spatial` por la tubería PCM en modo usuario, sin agregar caminos kernel→usuario privados si no son necesarios.
6. Impedir que la salida de espacialización sea el mismo dispositivo que genera el audio de entrada. Usar IDs de endpoint persistentes y protegerse frente a reconexión/cambio de dispositivos.
7. Implementar limpieza, suspensión/recuperación, firma Microsoft, pruebas HLK y desinstalación. No desactivar la seguridad de Windows como parte de las instrucciones a usuarios.

No se dará por completada esta fase sin verificar en Windows que la salida virtual contiene las **muestras reales** y que no se producen bucles ni pérdidas de audio tras cerrar TruckAudioSim.
