# v0.16.0 — dispositivo de control WDM (experimental)

**Estado:** este paquete **no contiene** un controlador `.sys`, `.inf`, instalador, certificado, ni un endpoint de reproducción de Windows. `src/wdk/tas_wdm_control_device.cpp` es un módulo WDK aún no compilado; **no debe instalarse en un sistema personal**. La parte portátil fue probada en Linux, no el WDM real.

## Archivos nuevos

- `src/wdk/tas_wdm_control_device.h/.cpp` — integración WDM **no compilada** que crea `\\Device\\TruckAudioSimPcm` y `\\DosDevices\\TruckAudioSimPcm`, con ACL para `LocalSystem` y administradores, y enruta cuatro operaciones (`CREATE`, `CLEANUP`, `CLOSE`, `DEVICE_CONTROL`) al sistema de sesiones.
- `TAS_IOCTL_GET_STATUS = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS)` — handshake de 40 bytes definido en `tas_ioctl_contract.h`.
- `tests/control_status.cpp` — verifica el handshake, los cuatro formatos, identificación de fuente, aislamiento de sesiones y tamaño de búfer.
- `src/windows/driver_client.cpp` — valida el handshake antes de recibir PCM; no cambiará ni instalará drivers.

## Flujo previsto de integración con SysVAD

1. Compilar el SysVAD original con Visual Studio, Windows SDK y WDK, sin modificaciones. Asegurar un equipo de prueba aislado antes de instalarlo.
2. Después de `PcInitializeAdapterDriver`, construir/integrar el módulo de control **con los handlers originales de PortCls preservados**. El módulo debe enlazar los archivos `tas_wdm_control_device.cpp`, `tas_wdm_capture_hooks.cpp`, `tas_wdm_session_adapter.cpp` y `src/driver_port/*.c`. Validar los símbolos WDK en compilación real.
3. Configurar **el endpoint de reproducción** con los formatos y máscaras de canal correctos. El control device NO añade por sí mismo los modos de reproducción 2.0/5.1/7.1.
4. Desde el render miniport, obtener muestras de PCM **ya mezcladas** en un anillo estable. SysVAD original puede mantener flujos simultáneos separados; el programa todavía no mezcla dichos flujos. La lectura directa de un DMA activo no garantiza consistencia: crear un anillo sombra/instantánea, con propiedad y vida útil garantizadas.
5. En transición `RUN` invocar `TasWdmControlStreamStart`; en `STOP/PAUSE`, `TasWdmControlStreamStop`; en un *worker* a `PASSIVE_LEVEL`, invocar `TasWdmControlPassiveTick` con el anillo estable y el contador *absoluto* de frames completados. Nunca tomar el `FAST_MUTEX` desde DPC o una rutina a DISPATCH_LEVEL.
6. Resolver la identidad de dispositivo: `sourceToken` debe coincidir con el que calcula el cliente para el endpoint de reproducción virtual una vez creado. Este emparejamiento todavía está pendiente; hasta entonces no habrá audio funcional de extremo a extremo.
7. Probar autenticación y ciclo de vida con el sistema de permisos de Windows, streaming con varias aplicaciones, sleep/resume, cambios 48kHz/7.1, suspensión, cierre de dispositivo y actualización de drivers.
8. Firmar y certificar el driver con los mecanismos requeridos por Windows antes de distribuir una versión instalable. No desactivar protecciones de Windows como solución de distribución.

## Protocolo de estado TAST

Todos los enteros se codifican **little-endian**, sin estructuras expuestas del kernel. Longitud exacta: 40 bytes.

| Offset | Tipo | Contenido |
| ---: | --- | --- |
| 0 | u32 | Magic `TAST` (`0x54534154`) |
| 4 | u16 | Versión `1` |
| 6 | u16 | Longitud `40` |
| 8 | u32 | Sample rate `48000` |
| 12 | u32 | Máscara de canales Windows |
| 16 | u16 | Número de canales |
| 18 | u16 | Formato `1 = float32` |
| 20 | u32 | Token de identidad de endpoint (no credencial) |
| 24 | u64 | Generación de sesión |
| 32 | u32 | Paquetes pendientes |
| 36 | u32 | Paquetes descartados, saturados a u32 |

`GET_STATUS` requiere un handle de lector activo, y devuelve `BUFFER_TOO_SMALL` sin modificar ni exponer audio cuando el búfer es insuficiente. El cliente validará el token, la versión y la frecuencia de muestreo antes de realizar `GET_PACKET`.

## Riesgos abiertos

- **No existe endpoint SysVAD adaptado.** El módulo añadido solo crea un **dispositivo de control**, distinto de una salida de sonido.
- No se ha compilado el código WDK en Windows; rutas de driver, símbolos y ciclo de vida requieren revisión en ese entorno.
- El prototipo maneja un único stream/lector; un dispositivo real necesitará mezclador PCM para varios streams, con sincronización y control de DRM.
- La inicialización y desinstalación del control device están diseñadas para **arranque y descarga únicos**. No invocar la descarga mientras haya IRPs o callbacks activos.
- Requiere privilegios administrativos/LocalSystem: una futura aplicación de usuario corriente se comunicaría mediante un servicio supervisado, no abriendo directamente el dispositivo.
- No hay procesamiento Dolby/DTS/Sonic validado, captura de Spotify sin duplicados ni cámara ETS2 conectada a la interfaz.

## Pruebas incluidas

`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build && ctest --test-dir build --output-on-failure`

El nuevo test se llama `wdm_control_status_handshake` y no requiere un controlador instalado.
