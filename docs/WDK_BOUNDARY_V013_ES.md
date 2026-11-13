# TruckAudioSim v0.13.0 — Contrato IOCTL (histórico)

**Aviso:** desde v0.14.0 el adaptador `tas_wdm_ioctl_adapter.*` fue retirado y reemplazado por `tas_wdm_session_adapter.*`, que controla el ciclo de vida de un lector. Ver [WDK_SESSION_V014_ES.md](WDK_SESSION_V014_ES.md).

## Qué existe

- `src/driver_port/tas_ioctl_dispatch.c/.h`: despachador portable para una solicitud `TAS_IOCTL_GET_PACKET` (0x00226000), sin API de Windows. Ya está unido a la cola de audio 2/3/6/8 canales. Validaciones: código correcto, entrada vacía, longitud de salida, ausencia de punteros ajenos, cola vacía y formato de paquete. El test `ioctl_buffered_dispatch_simulation` verifica entrega bit a bit y que un fallo no consume el paquete.
- `src/driver_port/tas_packet_fifo.c`: limpieza de los bytes almacenados al retirar o sobrescribir paquetes y al reiniciar la cola; esto reduce el tiempo de permanencia de audio de otras aplicaciones en la memoria.
- `src/wdk/tas_wdm_ioctl_adapter.cpp/.h`: **plantilla de callback WDM** que mapea el resultado portable a `NTSTATUS`, obtiene el `SystemBuffer` de un IRP `METHOD_BUFFERED`, utiliza `FAST_MUTEX` y finaliza el IRP. **No se compila con CMake, no se ha compilado con WDK, no instala nada y no es un `.sys`.**

## Qué falta para que el driver sea real

1. Partir de SysVAD *WDM/PortCls* y preparar un dispositivo de control separado **con nombre, descriptor de seguridad restrictivo e identidad de dispositivo**. Esta plantilla **no** crea el control device. No utilizar la API KMDF `WdfControlDeviceInitAllocate` directamente dentro de un driver WDM/PortCls.
2. En la instancia del control device, implementar y conservar el dispatch de `IRP_MJ_DEVICE_CONTROL` correspondiente. Los IRP destinados a PortCls deben seguir su ruta habitual: no sustituir su dispatcher global sin una correcta delegación y revisión.
3. Conectar la ruta de reproducción real ya mezclada en WaveRT con `tas_stream_adapter_poll`, serializar productor/consumidor bajo el **mismo** mecanismo de sincronización y usar búferes no paginados con ciclo de vida claro. El productor no debe ejecutar copias grandes en DPC. No está implementado el origen de muestras de SysVAD.
4. El `sourceToken` es un identificador para evitar bucles, **no una credencial de seguridad**. El control device debe tener ACL para el servicio autorizado, aplicar consumidor único y limpiar los búferes al cerrar.
5. Probar en VM de Windows 11 con Visual Studio, Windows SDK y WDK; luego verificaciones de IRQL, suspensión/reanudación, formatos de canales, privacidad y Driver Verifier. Solo después preparar INF, firma y distribución. No instalar en PC principal ni desactivar protecciones de firma para esta fase.

## Probar el código portable (sin Windows)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Criterios de aceptación antes de declarar un driver funcional:

- Windows detecta un endpoint propio con 2.0/5.1/7.1 anunciados.
- La música enviada al endpoint se recupera por el servicio sin duplicados ni tonos falsos.
- Ocho canales PCM independientes conservan su identidad al atravesar WaveRT/IOCTL y el motor espacial.
- Solo el proceso autorizado puede extraer muestras del control device.
- Una desconexión o suspensión no deja memoria accesible ni produce bloqueos.

**Estado actual: solo la capa portable y una plantilla WDM; NO hay controlador Windows instalable ni reproducción comprobada con Dolby Atmos/Windows Sonic/DTS.**

Fuentes de diseño:
- https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad
- https://learn.microsoft.com/en-us/windows-hardware/drivers/wdf/controlling-device-access-in-kmdf-drivers
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfio/nc-wdfio-evt_wdf_io_queue_io_device_control
