# v0.23.7 — sonido en todo el autobús

- Nueve perfiles Realista, 5.1 y 7.1 para Scania Touring, MAN Lion’s Coach y Volvo 9700 DD, con bocinas distribuidas hasta el fondo del salón y en ambos pisos del Volvo.
- La cámara libre sigue escuchando el interior al recorrer el autobús. Fuera, el nivel baja con la distancia a la carrocería.
- El editor y el motor conservan la profundidad completa de los perfiles de autobús.

# v0.23.6 — volumen, vehículos y envolventes

- Volumen principal hasta 1000%, pasos de 10 puntos y aviso visual para los atajos.
- Perfiles para los tres autobuses de Coaches, los cuatro Ford de Road Trip y Škoda Superb / Scout.
- Señal envolvente en 5.1 y 7.1 con entradas estéreo, 2.1 y 5.1.

# v0.23.5 — perfiles de cabina

- Tres perfiles de fábrica por modelo, con referencias de cabina representativas.
- Mejora la respuesta de los frontales y el reparto de canales al usar perfiles Realista y 5.1 con fuentes multicanal.

# v0.23.4 — captura con otras frecuencias de muestreo

- La captura en vivo solicita a WASAPI la conversión a 48 kHz cuando la salida de origen tiene otra frecuencia. Se conserva el mapa de canales.

# v0.23.3 — nivel de referencia y volumen máximo

- La mezcla en vivo considera los filtros de las bocinas al calcular el nivel por canal y eleva el nivel de referencia.
- El control principal admite hasta 500% y la atenuación exterior se aplica después de la amplificación.

# v0.23.2 — instalador y cámara

- Instalador de Windows x64, bandeja de Windows y reintentos de cámara.

# v0.23.1 — limpieza del catálogo

- El selector incluye las variantes Realista, 5.1 y 7.1 de cabina. Se retiraron los presets de fábrica antiguos, Scania S 7.1 espacial y ETS2 Live | Stereo. Auto prefiere Realista para la cabina correspondiente.

# v0.23.0 — perfiles de cabina

- 186 perfiles Realista, 5.1 y 7.1 para 62 configuraciones de cabina de 41 modelos de ETS2 y ATS.
- Filtros por tipo de bocina en los nuevos perfiles y nivel de 0 dB por bocina.

# v0.22.1 — volumen hasta 400%

- Amplía el volumen principal de 0 a 400%, incluido el ajuste guardado y los atajos. Por encima de 100% el motor amplifica las muestras PCM.

# v0.22.0 — cámara exterior y electrónica

- Reutiliza los DLL de cámara y telemetría de Trucker AUX 0.4.0 y muestra el instalador en Camera.
- Usa la posición y orientación reales en cámara exterior o libre. La música se atenúa hasta desaparecer a 40 m de la cabina.
- El controlador de Windows pausa Spotify o VLC compatible al apagar la electrónica y solo reanuda la sesión que pausó. El modo Off conserva el silenciado.

# v0.21.0 — interfaz de escritorio de Trucker AUX

- Integra el editor original, los presets y la lectura de telemetría de Trucker AUX 0.4.0.
- Sustituye la captura y reproducción del panel Audio por WASAPI y Windows Spatial Sound.
- Actualiza posición, canal, ganancia, solo, mute y pose en vivo. Añade selección de canales discretos y prueba WAV 7.1.
- La beta usa una salida de captura de Windows existente. Queda por verificar la escucha en vivo en Windows y el controlador virtual propio.

# v0.20.0 — ETS2 Live (beta)

- Lanzador `Iniciar-ETS2.cmd` compatible con PowerShell 5.1 (ExecutionPolicy solo a nivel de proceso).
- Selecciona origen/destino de forma interactiva y bloquea que sean iguales; usa IDs persistentes.
- Verifica 48 kHz y profundidad/canales compatibles antes de abrir Spatial Sound.
- Abre el Mezclador de volumen para asignar el sonido de ETS2 a Steam Streaming Speakers.
- Perfil nuevo de **estéreo fiel a dos objetos**, sin afirmar 7.1 inexistente.
- No instala controladores, no altera dispositivos por defecto y no silencia aplicaciones.
- Ejecutables Windows **sin recompilar**: motor preexistente de v0.17.0. Validación real ETS2 pendiente.
- Pruebas estáticas del lanzador y perfil añadidas a CTest; no representan una prueba WASAPI nativa.

## v0.19.0 — 2026-11-11 — diagnóstico espacial discreto 7.1

- Nuevo generador y WAV 7.1 WAVEFORMATEXTENSIBLE de ocho tonos secuenciales,
  sin contenidos de terceros; prueba automática de identidad de cada canal.
- Nueva opción fuente Windows `--channel-test`: ocho objetos discretos,
  sin downmix ni fallback y con telemetría de muestras no nulas, RMS y picos.
- Perfil diagnóstico importable por ejecutables anteriores y prueba CMD
  compatible con el paquete v0.18.0; no necesita instalar driver.
- CMake Linux Release: 30/30 CTest. No hubo compilación MSVC/WDK en esta entrega.

# Registro de cambios

## v0.18.0 — integridad PCM y preparación de pruebas W1 (11-nov-2026)

- `src/common/spatial_mix.*`: la ruta de objetos espaciales acepta bloques parciales de WASAPI/pipe y llena la cola faltante con silencio; controla NaN, valida límites y registra muestras recortadas. Sin ecualización ni refuerzo de graves.
- `tas_spatial`: telemetría de cuadros sin origen, pico antes/después de limitación, clipping y cuadros renderizados; alerta si Windows invalida un objeto espacial.
- El proyecto WDK SysVAD PortCls cambia `DriverType` a **WDM**, elimina la sección KMDF del INF y añade regresión estática. Este cambio requiere **compilar con WDK y comprobar InfVerif en Windows**, todavía no se ha verificado su instalación.
- `tas_endpoint_verify` exige opcionalmente 2/6/8 canales y permite fallo por discontinuidades o pérdida en modo estricto; nuevo script seguro `tools/w1-verify-endpoint.ps1` guarda registros y SHA-256 sin instalar drivers.
- **28/28 pruebas portables CTest aprobadas en Linux**. Binarios anteriores de `bin-x64` y `driver/lab-x64` corresponden a v0.17.0 y NO se reconstruyeron; no hay evidencia de ejecución Windows para este cambio.



## v0.17.0 — avance Windows W1

- Driver WaveRT basado en SysVAD con un endpoint de reproducción propio y PCM16 2.0, 5.1 y 7.1 a 48 kHz.
- El motor toma la mezcla WASAPI de una salida virtual y la reproduce como objetos espaciales en una salida física distinta.
- Compilación y enlace Windows x64 del motor y del binario de laboratorio del driver. Instalación firmada y reproducción en vivo pendientes de comprobar.

## v0.16.0 — diagnóstico de Windows W0

- El usuario confirmó que la prueba con Dolby Atmos para auriculares funcionó en la salida Realtek de su PC con Windows 11.
- El lanzador de Atmos acepta el número de salida mostrado en la lista y también el ID completo.
- `Probar-Dolby-Atmos.cmd` permite elegir la salida física, exigir objetos espaciales y registrar la escucha con Dolby Atmos para auriculares. Funciona desde el Explorador o desde PowerShell como un comando de Windows.
- El WAV de referencia y el perfil de dos fuentes fijas permiten verificar el orden izquierda/derecha en WASAPI y Windows Sonic.
- El diagnóstico rechaza WAV truncados y tamaños PCM que terminan en un cuadro incompleto.
- La lista de salidas de Windows conserva los nombres con caracteres no ASCII y usa UTF-8 al guardarse en un registro.
- `validate-windows.ps1 -AudioOnly` ejecuta la prueba con los binarios y el WAV incluidos. Registra el dispositivo seleccionado, las huellas SHA-256 y el resultado del proceso sin requerir el entorno de compilación.
- `validate-windows.ps1 -GuidedSonic` lista las salidas, conserva el ID seleccionado y guarda la observación del movimiento junto con el resultado de la prueba espacial.
- En una VM Windows 11 x64 se construyeron 35 ejecutables con MSVC y pasaron las 26 pruebas de CTest. La captura del dispositivo emulado confirmó la reproducción estéreo WASAPI. La escucha con Windows Sonic en auriculares físicos queda pendiente.

## v0.16.0 — avance de validación W0

- `tas_spatial --smoke-test` reproduce un WAV estéreo de 48 kHz con un objeto dinámico que se mueve de izquierda a derecha. Si el endpoint carece de objetos dinámicos, informa de la causa y usa WASAPI estéreo. `--require-spatial` exige la ruta espacial.
- Los IDs de dispositivo ahora se verifican como salidas de reproducción. La compilación Windows deja de repetir las macros de configuración del SDK.
- La compilación de validación exige Python para ejecutar la suite completa y guarda evidencia CTest. Se enlazaron los ejecutables x64 con Clang/LLD y el SDK de Windows, y el núcleo pasó 26/26 pruebas en Linux. La prueba audible y la ejecución nativa con MSVC siguen pendientes.

## v0.16.0 — control device WDM experimental y verificación del protocolo

- `src/wdk/tas_wdm_control_device.cpp/.h`: módulo para crear un dispositivo de control WDM con ACL restrictiva, preservar los IRP handlers anteriores de PortCls y reenviar `CREATE`, `CLEANUP`, `CLOSE` e `IOCTL` a la sesión PCM. Código **no compilado ni probado con WDK**.
- `TAS_IOCTL_GET_STATUS` con 40 bytes, versión del protocolo, canales, máscara, origen y generación de sesión; no entrega PCM.
- `tas_driver_client` consulta el estado y comprueba la identidad del dispositivo antes de emitir audio.
- Nuevo test `wdm_control_status_handshake` que prueba acceso, formato, tamaño de búfer y reconexión para 2/3/6/8 canales.
- **26/26 CTest Linux Release**; el código del controlador Windows real permanece sin compilar. **No hay driver `.sys` ni endpoint de audio instalable.**

## v0.15.0 — Coordinador de captura WaveRT a nivel pasivo

- Nuevo `tas_capture_coordinator` (C11), que conecta el extractor de ring WaveRT con el control de sesión y FIFO de PCM existentes.
- Descartar audio anterior a la conexión del lector y resíncronizarse al cambio de generación de sesión.
- Manejar transición RUN/STOP, reinicio del contador de frames y sobreescritura del búfer circular.
- Limitar trabajo por iteración (1–8 paquetes máximos) y registrar pérdidas de muestras.
- Vaciar el FIFO ante audio inválido y al detener el stream, sin aplicar filtros de audio.
- Archivo experimental WDK `tas_wdm_capture_hooks.*`: bloqueo `FAST_MUTEX`, control `PASSIVE_LEVEL`, sin integración real ni firma de driver.
- Nueva prueba CTest `passive_wavert_session_coordinator` con señal multicanal 2/3/6/8, exactitud de PCM, reconexión y recuperación tras fallo.
- Documentación `docs/SYSVAD_PASSIVE_CAPTURE_V015_ES.md` con reglas de sincronización, snapshot estable y pasos pendientes.

**Estado:** solo núcleo y simulaciones comprobados. NO incluye dispositivo virtual ni driver `.sys` instalable. El adaptador WDK no se ha compilado en Windows.

## v0.14.0

- Gestión de un lector activo por sesión, limpieza de PCM y control de propiedad de la conexión.
- El código anterior completo se conserva en el proyecto.
