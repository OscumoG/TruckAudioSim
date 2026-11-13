# Preparación de endpoint virtual 2.0 / 5.1 / 7.1 (v0.7)

## Lo que ya tenemos

- `tas_endpoint_probe`: comprueba, con `IAudioClient::IsFormatSupported`, si un endpoint **existente** anuncia exactamente audio de 48 kHz, float32, 2.0 / 5.1 (side o back) / 7.1.
- `tas_endpoint_capture`: captura la señal real de la mezcla WASAPI y escribe el identificador de su endpoint en la cabecera local PCM.
- `tas_spatial`: rechaza por defecto una fuente con el **mismo identificador** que su dispositivo físico de salida.
- Los nombres y las posiciones de los altavoces vienen de presets; este cambio no aplica EQ ni mejora artificial de graves.

## Qué falta todavía para el driver

NO existe un `TruckAudioSim.sys` instalable, ni se debe instalar el controlador de muestra SysVAD tal como está. La muestra oficial **genera tonos** para simular el loopback y no transmite la música reproducida.

El controlador real necesitará, como mínimo:

1. Un endpoint de **render** WaveRT reconocible por Windows, con su propia identidad hardware/PNP y nombre TruckAudioSim.
2. Topologías correctas para 2.0, 5.1 (side/back) y 7.1, con máscara `WAVEFORMATEXTENSIBLE` correspondiente y **48 kHz**.
3. Transferencia PCM desde la mezcla real de reproducción al loopback capturable por WASAPI, sin sustituir audio por tonos.
4. Manejo de latencia, silencio, reinicios, cambios de formato, múltiples streams, límites de memoria y cierre seguro del servicio de usuario.
5. Restricciones de acceso/seguridad en cualquier interfaz de control añadida, más instalador y **firma del driver** compatible con Windows 11.
6. Pruebas HLK y del motor espacial físico. No asumir que Dolby Atmos/DTS/Windows Sonic estén activados.

## Orden de aceptación en Windows 11 (NO ejecutar con drivers de prueba en un sistema principal)

1. Compilar un fork del ejemplo SysVAD con Visual Studio + WDK en un equipo de pruebas.
2. Confirmar la identidad del endpoint con `tas_endpoint_probe --list-devices`.
3. Consultar `tas_endpoint_probe --input-index N` y verificar que se anuncien exactamente los formatos diseñados.
4. Reproducir un WAV con señales diferentes por canal y grabar la salida virtual: `tas_endpoint_capture --input-index N --seconds 10 --save-wav capture.wav`.
5. Usar `tools/check-loopback.py` para comparar el WAV original y el de salida; si se obtiene un tono sintético o silencio, **no pasa**.
6. Verificar que `tas_spatial --live --output-index N` **rechaza** recibir audio del mismo endpoint N.
7. Repetir con entrada de 6/8 canales y salida **física distinta** con Spatial Sound habilitado.

## Detalles del protocolo

La cabecera IPC `TAS3` mantiene su tamaño **24 bytes** y versión 1. El antiguo campo reservado de los bytes 20–23 ahora indica un token de origen de dispositivo (FNV-1a de su ID UTF-16). El valor 0 indica origen desconocido y es compatible con los emisores anteriores.

Este token sirve como protección contra configuración errónea, **no es una credencial de seguridad**; una fuente deliberadamente maliciosa puede falsificarlo. La implementación definitiva verificará los dispositivos también mediante identificadores completos del sistema.

## Referencias

- SysVAD: https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad
- Spatial Sound: https://learn.microsoft.com/en-us/windows/win32/coreaudio/spatial-sound
- WASAPI: https://learn.microsoft.com/en-us/windows/win32/coreaudio/wasapi
