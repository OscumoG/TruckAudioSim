# v0.14.0 — Sesiones de control WDM y privacidad del audio

**Esta entrega NO contiene un `.sys`, una tarjeta de sonido virtual ni un instalador de driver.** Es código C portable validado y una plantilla WDM **sin compilar con el WDK**, destinada a la futura integración en un SysVAD derivado.

## Qué se implementó

- `src/driver_port/tas_device_session.c/.h`: una sola lectura activa por dispositivo, asociada al identificador de `FILE_OBJECT` proporcionado por el I/O Manager. El identificador NO sustituye la comprobación de permisos o el ACL del dispositivo.
- `tas_session_open`: deniega una segunda sesión, limpia el audio antiguo antes de abrir la nueva y aumenta el contador de generación.
- `tas_session_read`: deniega lecturas de otros `FILE_OBJECT`, valida código IOCTL, longitud, búfer y formatos mediante el despachador PCM anterior.
- `tas_session_close` y `tas_session_shutdown`: eliminan el estado del lector, limpian todos los paquetes almacenados y no alteran otra sesión activa.
- `tas_session_push` y `tas_stream_adapter_poll_session`: no almacenan PCM si no hay consumidor activo; el productor nunca debe utilizar la ruta antigua `tas_stream_adapter_poll` para saltarse esta comprobación.
- `src/wdk/tas_wdm_session_adapter.cpp/.h`: *plantilla sin probar en WDK* para `IRP_MJ_CREATE`, `IRP_MJ_CLEANUP`, `IRP_MJ_CLOSE` e `IRP_MJ_DEVICE_CONTROL`. Se eliminó la plantilla IOCTL anterior que no comprobaba el propietario.

## Flujo esperado, cuando exista un controlador real

```text
Aplicación -> virtual WaveRT render (SysVAD modificado)
                  │
                  ▼
   PCM mezclado / snapshot estable, PASSIVE_LEVEL
                  │
                  ▼
 tas_stream_adapter_poll_session (bajo un solo lock)
                  │
                  ▼
  cola de paquetes TAS1 acotada y protegida
                  │
  Control device WDM separado, ACL restrictivo
                  │
                  ▼
 IRP_MJ_CREATE (reserva un lector)
 IRP_MJ_DEVICE_CONTROL (GET_PACKET del mismo FILE_OBJECT)
 IRP_MJ_CLEANUP/CLOSE (borra todo el PCM)
                  │
                  ▼
             Servicio C++20
                  │
                  ▼
        Microsoft Spatial Sound
```

## Requisitos que todavía debe implementar el integrador WDK

1. Crear el dispositivo de control **separado** con permisos restrictivos aplicados en kernel. Autorizar solo el servicio apropiado y proteger el handle de lectura. Los tokens de ruta y el puntero `FILE_OBJECT` no acreditan identidad por sí solos.
2. Encaminar **únicamente** los IRP de dicho dispositivo a estas funciones, manteniendo intacto el despacho WDM/PortCls de los dispositivos de audio de SysVAD. La plantilla no modifica `DriverEntry` ni crea `DEVICE_OBJECT` o enlace simbólico.
3. Inicializar `tas_packet_fifo` en memoria no paginada y `tas_device_session` por instancia; el ciclo de vida de ambos debe cubrir los IRP pendientes y la retirada del dispositivo.
4. Sincronizar productor, consumidor, limpieza y suspensión con el **mismo** lock. `FAST_MUTEX` requiere `PASSIVE_LEVEL`, por lo que NO se debe ejecutar esta ruta desde DPC, ISR o a IRQL elevado. Asegurar snapshots consistentes del búfer WaveRT antes de leer PCM.
5. Limpiar el estado en `IRP_MJ_CLEANUP`, al reiniciar la transmisión, al cerrar el servicio y al retirar el dispositivo. Un `IRP_MJ_CLOSE` antiguo puede llegar después de abrir otro lector: nunca debe cerrar la nueva sesión.
6. Implementar la fuente PCM mezclada auténtica en SysVAD (el sample estándar simula el loopback con un tono); no asumir que el dispositivo virtual ya transmite audio real.
7. Compilar con Visual Studio y WDK en Windows, probar en entorno de laboratorio/VM, ejecutar Driver Verifier y comprobar fallos de permisos, suspensión, formatos, latencia y desconexión antes de plantear la firma y distribución.

## Prueba actual (Linux o Windows, sin WDK)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

`wdm_single_reader_session_lifecycle` verifica acceso exclusivo, lecturas denegadas, cierre obsoleto, PCM no retenido sin lector, limpieza de memoria y cuatro formatos. `wave_dma_to_bounded_fifo` verifica que el productor de PCM no almacena música sin una sesión activa.

**Validado localmente:** 24/24 pruebas CTest en Linux Release. Los tres tests relacionados con la interfaz IOCTL/lector/productor también pasaron con AddressSanitizer y UndefinedBehaviorSanitizer. Esto NO prueba compilación WDK ni la seguridad de un driver real.

Referencias oficiales:
- https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad
- https://learn.microsoft.com/en-us/samples/microsoft/windows-driver-samples/sysvad-virtual-audio-device-driver-sample/
