# AGENTS.md — TruckAudioSim · prioridad absoluta Windows

> **Instrucción principal para Codex:** TERMINA PRIMERO UNA VERSIÓN REAL Y UTILIZABLE EN **WINDOWS 11 x64**. No desarrolles versiones para Linux ni macOS, ni comiences una reescritura multiplataforma, hasta que el MVP de Windows esté implementado, compilado, probado con audio real y documentado. Mantén el código portable ya existente, pero todo el trabajo nuevo debe priorizar Windows.

## Objetivo del producto

Crear **TruckAudioSim**, un sistema de sonido virtual para Euro Truck Simulator 2 y American Truck Simulator que:

1. Instale o configure una **salida de audio virtual propia** (sin VB-CABLE) donde el usuario pueda enviar Spotify, YouTube, VLC y otras aplicaciones. Debe poder transportar **PCM multicanal verdadero**. Ofrece estéreo, 5.1 y 7.1; 2.1 puede implementarse como estéreo más gestión interna de graves si la configuración del endpoint de Windows no admite 2.1 de forma fiable.
2. Reproduzca ese audio en los **auriculares/altavoces físicos elegidos**, sin crear un bucle ni duplicar la reproducción original.
3. Sitúe los altavoces virtuales dentro de la cabina utilizando **Microsoft Spatial Sound** (`ISpatialAudioClient` y objetos dinámicos cuando estén disponibles). Debe respetar el **renderizador espacial activado en el dispositivo físico** (Windows Sonic, Dolby Atmos for Headphones o DTS Headphone:X). No debe simular tener una licencia, activarla silenciosamente ni incluir DSP propietario.
4. Mantenga una **respuesta tonal lo más neutra posible**: sin bass boost, EQ exagerado, compresión de loudness ni generación artificial de LFE en modo de referencia. Gestiona la suma de fuentes para evitar clipping y haz comparaciones A/B a volumen igualado.
5. Mantenga compatibilidad de importación/exportación con perfiles **JSON de Trucker AUX**, incluyendo el perfil de Scania S incluido y el editor de altavoces. Si amplías el formato, conserva la compatibilidad hacia atrás.
6. Use telemetría de ETS2/ATS y, cuando haya un método seguro y probado para obtener la cámara, actualice **posición/orientación del oyente**. Al apagar la electrónica del camión, pause el reproductor compatible y reanúdelo únicamente si lo pausó el programa; ofrece modo 'solo silenciar'.
7. Pueda publicarse como software libre bajo **MIT**, respetando avisos de copyright, licencias de dependencias y derechos de terceros.

**No declares que una función está terminada porque compila un modelo o simulador.** Solo cuenta como completa cuando se demuestra con el dispositivo de audio de Windows y audio real según los criterios de aceptación.

## Estado verificado del repositorio entregado (v0.16.0)

- Núcleo C/C++20, importador de perfiles, ruteo PCM 2/3/6/8 canales, transporte por memoria compartida, modelos WaveRT, cola de paquetes y contrato IOCTL.
- 26 pruebas CTest **aprobadas en Linux**; pueden ejecutarse con CMake. Esta cifra NO demuestra funcionamiento en Windows.
- `src/windows/main.cpp` incluye un prototipo de salida espacial y otros archivos de `src/windows/` implementan rutas experimentales de captura/control de audio; hay que compilar, corregir y probar en Windows.
- `src/wdk/` contiene piezas **WDM/PortCls experimentales, no compiladas con WDK**. Aún NO hay endpoint virtual de reproducción instalable, `.sys` funcional ni instalador firmado.
- El extractor `src/driver_port/` parte de PCM ya disponible/mezclado: **faltan la integración con SysVAD real y la mezcla de múltiples streams**.
- `editor/` es un editor web de prototipo, no necesariamente la interfaz original completa de Trucker AUX. No afirmes que el editor exacto fue reutilizado sin verificar la procedencia y licencias.
- La captura WASAPI por proceso/loopback puede **duplicar** el sonido; no equivale a redirección mediante endpoint virtual.

Consulta `README.md`, `CHANGELOG.md`, `docs/SYSVAD_CONTROL_DEVICE_V016_ES.md`, `docs/DRIVER_ACCEPTANCE_ES.md` y `ATTRIBUTION.md` antes de implementar.

## Plan de trabajo obligatorio — EN ESTE ORDEN

### Fase W0 — Auditoría y compilación Windows (bloqueante)

- Revisa el árbol real: `CMakeLists.txt`, `.github/workflows/build.yml`, `src/windows/`, `src/wdk/`, `src/driver_port/`, `src/common/`, `editor/` y `tests/`.
- En Windows 11 x64 con Visual Studio 2022, Windows SDK y CMake: **compila y ejecuta todos los tests nativos**. Corrige errores reales de MSVC/WinRT/COM/SDK; no presupongas que el CI está en verde.
- Construye un smoke test que abra el dispositivo de salida físico seleccionado, consulte soporte espacial y reproduzca un WAV con un objeto 3D móvil. Si el sistema no admite objetos dinámicos, informa claramente y utiliza un fallback identificable.
- Crea un registro de resultados reproducibles en `docs/VALIDACION_WINDOWS.md` con comandos, versiones, dispositivos, resultado y limitaciones. **Nunca inventes evidencias de pruebas**.

### Fase W1 — Endpoint de audio virtual REAL (principal prioridad)

- Utiliza el ejemplo oficial Microsoft **SysVAD WDM/PortCls** solo como punto de partida, de acuerdo con su licencia. No pegues un driver KMDF encima del ejemplo sin adaptar el modelo.
- Crea un proyecto WDK compilable y una configuración INF/desinstalación adecuada. Mantén el proceso de instalación fuera del camino normal de compilación del núcleo. No cargues ni instales drivers no validados automáticamente.
- Expón un endpoint de **REPRODUCCIÓN** 'TruckAudioSim Virtual Audio' que reciba audio de aplicaciones; anuncia correctamente formatos y máscaras de canales para estéreo, 5.1 y 7.1 a 48 kHz (otros formatos solo tras pruebas). El modo 2.1 puede ser gestión interna de subwoofer. No anuncies 7.1 si realmente solo llegan dos canales.
- Implementa transporte de **PCM real** desde la ruta de render de WaveRT al servicio usuario, sustituyendo el tono simulado del ejemplo. Resuelve mezcla y sincronización para **varias aplicaciones/streams simultáneos**; no leas un búfer DMA que puede cambiar sin una instantánea estable. Respeta niveles IRQL, propiedad de memoria y ciclo de vida.
- Reutiliza, si encaja, `src/driver_port/{wavert_dma_tap,tas_capture_coordinator,tas_packet_fifo,tas_device_session,tas_ioctl_dispatch}.*` y el contrato de `tas_ioctl_contract.h`, pero adáptalos a las restricciones reales del WDK. No expongas direcciones/punteros kernel a modo usuario.
- Aísla el servicio PCM: un único consumidor autorizado, ACL restringidas, manejo seguro de `METHOD_BUFFERED`, tamaños de buffers, cancelaciones de IRPs, desconexión, suspensión/reanudación y limpieza de muestras consumidas. Usa un servicio Windows con permisos apropiados si la GUI no puede abrir el control device.
- Compila y prueba con WDK en **VM de Windows 11 aislada** antes de intentar usar el driver en un equipo personal. Resuelve los requisitos legítimos de firma y distribución Microsoft; **no recomiendes desactivar protecciones de Windows para instalarlo**.
- Prueba Spotify/VLC enviando la reproducción al endpoint virtual y verifica una captura sin duplicación del audio original, con 2/6/8 canales verdaderos cuando la fuente es multicanal.

### Fase W2 — Audio espacial del dispositivo físico

- Conecta el PCM capturado con el renderizador de `src/windows/main.cpp` o reemplázalo por una implementación MS Spatial Sound correcta y comprobable.
- Separa **origen virtual** y **salida física**: si coinciden, rechaza la ruta para evitar realimentación. Detecta cambios de dispositivo, pérdida de soporte espacial y uso de auriculares frente a altavoces físicos.
- Selecciona el renderizador que Windows YA tiene activo; no intentes elegir Dolby/DTS mediante APIs no documentadas. Cuando Spatial Sound no esté disponible, ofrece fallback estéreo/multicanal y dilo en la UI.
- Convierte canales a objetos dinámicos manteniendo la distribución de cada fuente. Usa ejes y unidades correctos, transformaciones de posición/orientación y gestión estable del buffer; comprueba que el número de objetos soportados no se exceda.
- En archivos estéreo, 7.1 significa **distribución virtual**, no información 7.1 discreta. Conserva la identidad de 8 canales cuando el contenido sí sea 7.1.
- Añade pruebas A/B de nivel, respuesta en frecuencia, clipping, latencia, underruns y movimiento espacial; el LFE no se refuerza ni inventa en modo neutro.

### Fase W3 — ETS2/ATS + editor + control de reproducción

- Integra el SDK de telemetría SCS mediante plugin `.dll` independiente, compatible con ETS2/ATS x64, sin inyección del proceso del juego. Instala mediante ruta explícita y reversible; registra estados reales (vehículo, motor/electrónica cuando estén expuestos).
- Para seguimiento exacto de cámara, investiga el SDK y la compatibilidad de soluciones existentes. Si hay que depender de técnicas frágiles no documentadas, aíslalas en un plugin opcional y explica riesgos; no declares compatibilidad universal.
- Cuando la cámara gire, recalcula posiciones de fuentes relativas a la cabeza sin mover los altavoces en coordenadas de cabina. Soporta cámara interior y exterior, con transición suave y aislamiento opcional.
- Al perder electrónica, realiza fade corto y pausa mediante sesiones multimedia de Windows si el reproductor acepta el comando; reanuda solo sesiones pausadas por TruckAudioSim. Prueba pausa manual, cambio de app y cierre del juego.
- Editor: conserva/compatibiliza formatos JSON de Trucker AUX; importa/exporta presets, crea 2.0/5.1/7.1, permite posición 3D, nombre, canal y ganancia. Añade vista previa, validaciones y guardado seguro. No distribuyas código o assets externos sin licencia compatible.

### Fase W4 — Instalador y versión Windows pública

- Entrega instalador o paquete Windows x64 que incluya interfaz, motor, plugin de ETS2/ATS y **driver correctamente firmado y autorizado**; separación clara de instalaciones opcionales. Incluye desinstalación que restaure la salida predeterminada del usuario y no borre otros plugins.
- El usuario debe poder seleccionar Spotify como salida `TruckAudioSim Virtual Audio`, elegir auriculares físicos como destino, importar un preset y escuchar 3D sin VB-CABLE y sin duplicación.
- Documenta configuración, solución de fallos, instalación/desinstalación, requisitos de licencia de Dolby/DTS, escenarios sin Spatial Sound y privacidad. Prueba cambios en caliente, cortes de energía/sueño y reinicio de app.
- Revisa derechos MIT, atribuciones, fuentes originales, dependencias, no empaquetar archivos de Trucker AUX/ETS2/SCS/Dolby/DTS sin permiso.

## Criterios de aceptación — Windows primero

Marcar `WINDOWS MVP COMPLETO` **solo si TODOS** los puntos siguientes son verdaderos y demostrables:

- [ ] Compilación Windows x64 completa (MSVC + SDK + WDK) y tests automáticos aprobados, con registro.
- [ ] Un dispositivo de reproducción virtual aparece en el panel de sonido de Windows; permite estéreo/5.1/7.1 comprobados (2.1 solo si está verdaderamente soportado o se declara como bass management).
- [ ] Spotify/VLC/otra aplicación envían música real a ese dispositivo **sin VB-CABLE y sin duplicación**.
- [ ] Audio PCM conserva canales, contenido, sincronización, sin clipping relevante y sin EQ / bass boost no deseados.
- [ ] Se reproduce en una salida física distinta seleccionada y se utiliza la API espacial cuando la salida lo permite; Windows Sonic probado, Atmos/DTS probados si se dispone de licencias y hardware, o documentados expresamente como no probados.
- [ ] El editor funciona con el preset Scania S y permite importar/exportar un JSON de Trucker AUX sin perder posiciones y asignación de canales.
- [ ] Al cambiar cámara dentro de ETS2 se actualiza el escenario 3D; si depende de un plugin opcional, se comunica claramente.
- [ ] Al apagar electrónica compatible, pausa y recupera la música sin reanudar sesiones que estaban manualmente pausadas.
- [ ] Instalación y desinstalación del driver se han probado en VM y en un entorno Windows adecuado; la distribución cumple requisitos de firma y no requiere desactivar defensas.
- [ ] README Windows, guía de usuario, guía de compilación, limitaciones y atribuciones actualizadas.

Si algún punto no puede verificarse, **no digas que el proyecto está terminado**. Entrega lo implementado, logs y bloqueos concretos.

## Reglas de colaboración para Codex

1. **Primero Windows.** Linux y macOS: solo mantener compilación de componentes portables cuando proceda, sin nuevas funciones, planes de distribución ni puertos hasta completar el MVP de Windows.
2. No reemplaces código probado por simulaciones para inflar métricas. Las pruebas de mocks no validan el driver. Conserva resultados reproducibles, evita marcar éxitos ficticios.
3. Trabaja por hitos ejecutables y commits pequeños. Después de cada cambio ejecuta compilación y pruebas disponibles; registra TODO lo pendiente.
4. Si el entorno de Codex no tiene Windows + WDK, desarrolla el código y los tests posibles, pero declara claramente que **no está verificado en Windows** y entrega instrucciones de validación; no prometas una salida `.sys` funcional.
5. No implementes métodos para saltarte firmas, DRM, licencias ni protecciones del sistema o del juego.
6. Prioriza uso de CPU razonable, mínima latencia, estabilidad, recuperaciones ante errores, privacidad local y fidelidad tonal.
7. No pidas permiso para cada refactor rutinario: sigue estas prioridades. Pide intervención humana solo cuando sea inevitable (por ejemplo, una licencia de distribución o hardware Windows no accesible).

## Comandos iniciales y archivos clave

```powershell
# En Windows con Visual Studio C++ y CMake:
cmake -S . -B build-win -A x64
cmake --build build-win --config Release
ctest --test-dir build-win -C Release --output-on-failure
```

Para pruebas portables:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Archivos: `src/windows/main.cpp`, `src/windows/{endpoint_capture,device_select,media_control,driver_client}.cpp`, `src/wdk/tas_wdm_control_device.cpp`, `src/wdk/tas_wdm_capture_hooks.cpp`, `src/driver_port/`, `src/common/`, `editor/`, `profiles/`, `docs/DRIVER_ACCEPTANCE_ES.md`.

**Entrega final solicitada a Codex:** repositorio actualizado, binarios Windows verificables cuando sea posible, documento `docs/VALIDACION_WINDOWS.md`, guía para instalar/desinstalar, lista precisa de funciones verificadas/no verificadas y release Windows solo cuando pase la aceptación.
