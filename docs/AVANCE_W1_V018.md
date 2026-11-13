# TruckAudioSim v0.18.0 · continuación Windows W1

Fecha: 11 de noviembre de 2026. **No es una release de controlador instalable.**

## Cambios implementados

1. `src/common/spatial_mix.cpp` lleva cada fuente a su objeto mono siguiendo
   la máscara de canales del preset. Cuando la fuente WASAPI o el pipe entrega
   menos frames que los pedidos por Windows, el bloque restante se pone a cero.
   Se verifican longitudes, ganancia, valores no finitos y recorte. No hay EQ,
   compressor ni creación de LFE.
2. `src/windows/main.cpp` conecta la nueva función al flujo espacial,
   registra frames faltantes, clips y picos y detecta objetos invalidados.
   No restaura automáticamente el stream tras un cambio físico de dispositivo;
   esto sigue pendiente.
3. `driver/sysvad/TruckAudioSimVirtualAudio.vcxproj` ahora declara el
   controlador **WDM** (`PortCls/SysVAD`), no KMDF. Se retiró el servicio KMDF
   del `.inx`, porque este controlador no inicializa un dispositivo WDF.
   Es una corrección de configuración; hace falta ejecutar **MSBuild + InfVerif**.
4. `tas_endpoint_verify` admite `--expect-channels 2|6|8` para fallar antes de
   comenzar si el formato de mezcla de Windows no es el esperado. Con
   `--require-clean`, una discontinuidad o frame perdido invalida la prueba.
   El verificador existente ya genera dos sesiones compartidas con tonos
   diferentes para comprobar separación de canales.
5. `tools/w1-verify-endpoint.ps1` crea logs fechados con SHA-256 del ejecutable
   y conserva la salida/exit code de la prueba. No instala nada ni cambia la
   configuración del sistema.

## Evidencia realmente disponible

- Plataforma de pruebas nueva: Linux x86_64, GCC y CMake, compilación Release.
- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DTAS_REQUIRE_PYTHON_TESTS=ON`
- `cmake --build build -j4`
- `ctest --test-dir build --output-on-failure`
- **28/28 pruebas aprobadas**, incluido `spatial_object_buffer_integrity` y
  `wdm_inf_wdm_manifest_static`.
- Estas pruebas no representan WASAPI/PortCls instalado. No se ejecutó WDK,
  MSVC, InfVerif, Inf2Cat ni audio real con el código v0.18.0 en este entorno.
- La escucha con Dolby Atmos reportada anteriormente pertenece a W0/v0.16.

## Probar en Windows 11 x64

En una **VM aislada**, instala Visual Studio 2022, Windows SDK/WDK 26100,
CMake y Python 3.8+. Compila desde las fuentes (los ejecutables preempaquetados
son anteriores):

```powershell
cmake -S . -B build-win -A x64 -DTAS_REQUIRE_PYTHON_TESTS=ON
cmake --build build-win --config Release
ctest --test-dir build-win -C Release --output-on-failure
# Desde x64 Native Tools Command Prompt:
# tools\build-wdk.cmd
# InfVerif /w <ruta-del-INF-generado>
```

Después de **validar y firmar por las vías oficiales un paquete para la VM**,
verifica que el endpoint realmente aparece y permite estéreo, 5.1 y 7.1 a
48 kHz. Para cada configuración, abre las propiedades avanzadas de salida de
Windows y selecciona el número de canales. Ejecuta en PowerShell:

```powershell
.\tools\w1-verify-endpoint.ps1 -Channels 2
.\tools\w1-verify-endpoint.ps1 -Channels 6
.\tools\w1-verify-endpoint.ps1 -Channels 8
# Opcional: -RequireClean para exigir cero pérdidas o discontinuidades.
```

El script pide el ID del endpoint, ejecuta el `tas_endpoint_verify.exe`
**recién compilado** y guarda los logs en `build-win/validation`. Si un modo
no aparece en Windows, **NO** se da por aprobado solo porque `taswavtable.h`
lo enumera. No ejecute tres modos sin cambiar la configuración de Windows
entre pruebas.

## Bloqueos reales restantes

- El proyecto WDK y `.inx` necesitan verificación nativa y correcciones basadas
  en errores concretos de compilación/InfVerif; el `.sys` sin firma de laboratorio
  no es un paquete instalable.
- Demostrar la ruta de audio multicanal REAL con dos reproductores y ausencia
  de duplicación en la VM (y luego el equipo físico adecuado).
- Medir el orden 2/6/8, jitter, latencia, clipping y operación con Dolby Atmos.
- Comprobar cambios de endpoint en caliente, reinicio/suspensión y manejo de
  recursos de objetos dinámicos. Después, telemetría ETS2/ATS y el editor W3.
- Firma oficial de distribución, instalador/desinstalador y pruebas W4.

## Seguridad y licencia

No instala controladores automáticamente, no modifica Secure Boot, ni evita
firmas o licencias. Se conservan MIT para el motor y MS-PL para los archivos
basados en Microsoft SysVAD (consulta `ATTRIBUTION.md`).
