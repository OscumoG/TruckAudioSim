# TruckAudioSim v0.20.0 — ETS2 Live (beta sin controlador)

**Puedes probar el sonido real del juego, en vivo, usando los ejecutables Windows x64 incluidos.**
Es una nueva experiencia de uso basada en el motor WASAPI + Spatial Sound nativo que
ya existia en v0.17.0. No es una compilacion nueva del motor: el ejecutable Windows
se conserva sin cambios; se agrega el lanzador especifico de ETS2, un perfil fiel
y validacion del origen para que la prueba sea repetible.

## Inicio en Windows 11

1. **Extrae el ZIP completo** a una carpeta normal.
2. Conecta los auriculares a **Altavoces (Realtek High Definition Audio)** y mantén
   **Dolby Atmos para auriculares** activo como en tu prueba anterior.
3. Abre **`Iniciar-ETS2.cmd`**. Se mostrarán los endpoints actuales y se sugerirán
   *Steam Streaming Speakers* (origen) y Realtek (destino); confirma los números.
4. El lanzador abre opcionalmente el **Mezclador de volumen**. Con ETS2 abierto y
   emitiendo sonido, en Aplicaciones, cambia **solo la salida de ETS2** a
   *Altavoces (Steam Streaming Speakers)* (o al origen que elegiste).
   **No silencies ETS2**. No cambies la salida predeterminada de Windows.
5. Pulsa Intro para iniciar el renderizador y prueba el audio de ETS2 con
   los auriculares. Deberías ver `Mode: WASAPI_LOOPBACK_TO_SPATIAL_DYNAMIC`.
6. Para terminar pulsa **Ctrl+C** en la consola. Restaura manualmente la salida
   de ETS2 a Realtek/Predeterminada en el Mezclador de volumen.

En el último diagnóstico de tu PC la entrada Steam Streaming Speakers fue
**2 canales 48000 Hz 32 bits**, con 5.1/7.1 solo como formatos alternativos.
**Esta prueba NO proporciona 7.1 real ni identifica posiciones de vehículos
por separado**; emplea los 2 canales del juego y los renderiza como objetos
espaciales frontal-izquierdo y frontal-derecho. El perfil opcional *Cabina*
duplica canales estéreo entre objetos de ambiente y no debe confundirse con
sonido trasero discreto. De momento no hay telemetría ni seguimiento de cámara.

## Solución de problemas

| Síntoma | Qué comprobar |
|--|--|
| No suena nada | ETS2 está abierto y aparece en el Mezclador; su SALIDA es Steam Streaming Speakers, el audio no está silenciado y está a 48 kHz |
| Se sigue escuchando sin TruckAudioSim | ETS2 aún sale directamente por Realtek, o la entrada Steam se monitoriza; deja solo ETS2 en el origen de captura |
| Sonido doble o eco | El audio original también sale por el dispositivo físico. Selecciona dos endpoints distintos y comprueba la asignación por aplicación |
| Se oye a saltos | Cierra otras apps de audio en el endpoint de captura, comprueba que está a 48 kHz y detén la prueba con Ctrl+C. El motor antiguo no compensa deriva de reloj |
| Error de formato | Ejecuta `bin-x64\tas_endpoint_probe.exe --input-index 3` usando el **índice actual** indicado por la lista; exige 48 kHz, PCM16 o float32 |
| DTS/Sonic en lugar de Dolby | Puedes probar otro espacializador seleccionado en el dispositivo físico; el motor utiliza la API de Windows, no la marca directamente |
| No aparece Steam Streaming Speakers | Esta prueba requiere otra salida de reproducción distinta de Realtek; selecciónala en el menú si existe. No instales el .sys experimental para resolverlo |

## Verificación rápida manual (si el lanzador no abre)

```powershell
.\bin-x64\tas_spatial.exe --list-devices
.\bin-x64\tas_endpoint_probe.exe --input-index 3
.\bin-x64\tas_spatial.exe --preset .\profiles\ETS2_Estereo_Fiel.json --input-index 3 --output-index 0
```

**Los índices 3 y 0 eran los de tu prueba anterior: podrían cambiar.**
No elijas el mismo dispositivo para entrada y salida. Asegúrate de haber
asignado el audio de ETS2 al dispositivo `3` antes del tercer comando.

## Qué está validado y qué no

- Validado previamente por el usuario: Dolby Atmos y una prueba WAV de 8
  objetos con el ejecutable Windows del proyecto.
- Validado en esta entrega: integridad de binarios PE, contenido del perfil,
  checks del lanzador y pruebas portables en Linux.
- **No probado aún en este entorno:** escucha en vivo de ETS2 y el enrutamiento
  de Steam Streaming Speakers en tu instalación concreta de Windows.
- **No incluido:** controlador virtual propio firmado, instalación en Windows,
  audio 7.1 discreto del juego, telemetría, estimación de latencia o adaptación
  a largo plazo de los relojes WASAPI.

El programa no instala drivers, no cambia servicios del sistema y no modifica
la salida de ninguna aplicación automáticamente; solo solicita que tú elijas
un origen distinto para ETS2 en la interfaz de Windows.
