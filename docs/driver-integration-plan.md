# Fase de driver — ruta mediante WASAPI loopback (v0.6)

## Decisión

No implementar una API privada de kernel→usuario hasta necesitarla. Primero aprovechar **el loopback estándar de Windows** sobre nuestro futuro dispositivo virtual. El usuario enviará audio al endpoint render virtual, y la aplicación de TruckAudioSim leerá la mezcla digital mediante `IAudioCaptureClient`, usando el `tas_endpoint_capture` desarrollado en v0.6.

## Qué queda del driver SysVAD

El ejemplo SysVAD no transporta la mezcla de manera útil para nuestro motor; su ruta de loopback de demostración puede sintetizar tonos en vez de entregar el PCM reproducido. El trabajo de kernel tiene que implementar un endpoint WaveRT de reproducción y un loopback compatible con Windows que exponga la mezcla **real**, con las propiedades correctamente anunciadas (estéreo, 5.1 surround, 7.1 surround; 48 kHz). No bastará cambiar nombres/INF.

El controlador de producción también necesita firma, instalación/desinstalación segura, restricciones de acceso y pruebas de estabilidad/HLK. Evitar que falle la reproducción de audio normal si el servicio de usuario se cierra.

## Arquitectura objetivo

```
Spotify ----> TruckAudioSim Virtual Render [SysVAD derivado]
                        | mezcla PCM real 2/6/8 canales
                        v
                   WASAPI loopback (tas_endpoint_capture)
                        | named pipe local PCM
                        v
                   tas_spatial / mixer + ETS2 pose
                        | ISpatialAudioClient, OBJETOS DINAMICOS
                        v
                   Salida física DISTINTA: Sonic/Dolby/DTS
```

## Riesgos y criterios de aceptación

- **Bucle de realimentación**: la salida del motor espacial no puede ser el mismo endpoint que captura el loopback. Añadir detección por ID persistente antes de distribución.
- **Máscaras multicanal correctas**: preservar FL/FR/FC/LFE/SL/SR/BL/BR, no duplicar LFE desde estéreo y no convertir indiscriminadamente canales.
- **Latencia**: medir desde render hasta salida espacial; usar búferes con backpressure e instrumentación de underruns.
- **Sin HRTF doble**: desactivar cualquier espacializador propio al entregar objetos a Microsoft Spatial Sound.
- **Desconexiones**: recuperación frente a cambio de dispositivo predeterminado, suspensión de Windows, cierre de Spotify, cierres inesperados de procesos.
- **Seguridad y firma**: no instalar drivers no firmados, ni solicitar desactivar integridad de memoria u otras protecciones del sistema.
- **Pendiente**: pruebas en Windows con tarjetas multicanal y la futura muestra modificada de SysVAD.


## v0.6 — Prueba concreta

`tas_endpoint_capture --save-wav` permite grabar PCM de una salida de
prueba; `tools/check-loopback.py` exige una frecuencia piloto distinta por
canal y falla ante tonos simulados, silencio y canales intercambiados.
Ver `DRIVER_ACCEPTANCE_ES.md` para el procedimiento. El driver WaveRT sigue
pendiente; no hay archivos .sys ni scripts de instalación en esta versión.
