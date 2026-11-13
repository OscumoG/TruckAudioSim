# Salida virtual de TruckAudioSim para Windows 11 x64

El motor incluido en `bin-x64` toma el PCM de un endpoint de reproducción
de Windows y lo renderiza en otra salida mediante Microsoft Spatial Sound.
Usa el formato espacial que ya seleccionaste para los auriculares, como Dolby
Atmos para auriculares. El lanzador `TruckAudioSim.cmd` abre el perfil Scania S
y mantiene la sesión abierta hasta pulsar Ctrl+C.

| Componente | Estado de esta entrega |
| --- | --- |
| Diagnóstico Atmos W0 en la salida Realtek | Funcionó en la prueba física reportada por el usuario. |
| Motor con captura y salida espacial en un proceso | Compilado y enlazado para Windows x64. Falta la escucha en vivo con dos endpoints. |
| Driver propio WaveRT con INF | Compilado y enlazado para Windows x64 con headers y bibliotecas del WDK 26100. Falta compilar el proyecto MSBuild en Windows, verificar el INF, instalar el paquete firmado en VM y probar audio real. |
| Estéreo, 5.1 y 7.1 a 48 kHz | Declarados por el pin WaveRT. Windows debe exponerlos y se debe comprobar el PCM de cada canal en la VM. |
| Instalador y firma de distribución | Pendientes. El archivo de laboratorio tiene formato PE x64 nativo y carece de firma de distribución. |
| Telemetría ETS2/ATS, cámara y control integrado de Spotify | Pendientes. |

## Probar la ruta de usuario

Con un endpoint virtual de reproducción ya disponible, activa Dolby Atmos
para auriculares en la **salida física**. Dirige solo la aplicación de música
al **endpoint virtual** desde **Configuración → Sistema → Sonido → Mezclador de
volumen**. Mantén la salida física como salida habitual de Windows y del juego.

1. Extrae el ZIP completo y abre `TruckAudioSim.cmd`.
2. Introduce el número de la salida virtual como entrada y el de tus
   auriculares como salida. El motor comprueba que sean dispositivos distintos.
3. Reproduce un WAV en VLC o música en Spotify. La consola debe mostrar
   `WASAPI_LOOPBACK_TO_SPATIAL_DYNAMIC` y el número de canales detectados.
   Pulsa Ctrl+C para cerrar y ver los contadores de captura.

Para usar IDs persistentes y elegir otro perfil, ejecuta lo siguiente desde
la raíz extraída. El formato de entrada admitido es PCM16 o float32 a 48 kHz,
con máscara reconocida de 2, 3, 6 u 8 canales.

```powershell
.\bin-x64\tas_spatial.exe --list-devices
$entrada = Read-Host 'ID de la salida virtual'
$salida = Read-Host 'ID de los auriculares'
.\bin-x64\tas_spatial.exe --preset .\profiles\Scania_S_7_1_Fidelidad.json --input-id $entrada --output-id $salida
```

En el equipo donde se probó Atmos también apareció **Steam Streaming Speakers**.
Si sigue activo, puede usarse como entrada para intentar la prueba en vivo
sin instalar el driver de laboratorio. Usa Realtek como salida, selecciona
Steam Streaming Speakers solo para VLC o Spotify y evita una sesión de Steam
Remote Play durante la comprobación. La captura depende de que ese endpoint
exponga una mezcla compatible por WASAPI. Al terminar, restaura la salida
de la aplicación en el mezclador.

## Cómo llega el PCM al motor

El driver se basa en Microsoft SysVAD. Registra un par de filtros de topología
y render WaveRT con el nombre **TruckAudioSim Virtual Audio** y un identificador
de hardware propio. El pin de render anuncia PCM16 estéreo, 5.1 lateral,
5.1 trasero y 7.1, todos a 48 kHz. El INF solicita estéreo como formato inicial
y evita que el endpoint tome la salida predeterminada de Windows.

El filtro prescinde de offload y de un pin de loopback de hardware. En esa
configuración, WASAPI copia al cliente de captura la mezcla que el motor de
audio de Windows entrega al pin WaveRT. Esto conserva la mezcla de varias
aplicaciones en modo compartido sin generar el tono del loopback de ejemplo.
La captura se consume en memoria y la misma instancia envía los canales a
objetos 3D. Las rutas exclusivas quedan fuera de este flujo.

Esta ruta usa WASAPI en lugar del canal IOCTL experimental de v0.16.0. El
controlador nunca abre archivos de grabación. El modo `--save-wav` de la
herramienta de diagnóstico solo guarda muestras cuando se solicita
explícitamente. Consulta la [documentación de loopback WASAPI](https://learn.microsoft.com/windows/win32/coreaudio/loopback-recording).

## Compilar el controlador y preparar la VM

Requiere Visual Studio 2022, Windows SDK y WDK 10.0.26100.0. Abre **x64 Native
Tools Command Prompt** y ejecuta desde la raíz del proyecto.

```bat
tools\build-wdk.cmd
```

El proyecto y su plantilla INF están en `driver/sysvad`. La salida de MSBuild
se configura en `build-wdk/x64/Release`. El archivo en `driver/lab-x64` es un
resultado de enlace cruzado para inspección. La prueba de instalación se
realiza en una VM aislada con el paquete que se haya validado y firmado para
esa VM. Windows exige una firma de controlador válida para cargar el
controlador. Consulta [firma de controladores](https://learn.microsoft.com/windows-hardware/drivers/install/windows-driver-signing-tutorial).

En una VM que ya dispone de ese paquete firmado, usa un símbolo del sistema
elevado. `DevGen.exe` es una herramienta de pruebas del WDK que se obtiene
de Microsoft.

```bat
pnputil /add-driver C:\ruta\firmado\TruckAudioSimVirtualAudio.inf
pnputil /enum-devices /deviceid "ROOT\TruckAudioSimVirtualAudio"
```

Si la enumeración indica que el dispositivo aún no existe, crea una sola
instancia en la VM. Después verifica que el dispositivo inicia y que aparece
la salida con `bin-x64\tas_spatial.exe --list-devices`.

```bat
devgen /add /bus ROOT /instanceid TruckAudioSimVirtualAudio /hardwareid "ROOT\TruckAudioSimVirtualAudio"
pnputil /add-driver C:\ruta\firmado\TruckAudioSimVirtualAudio.inf /install
pnputil /enum-devices /deviceid "ROOT\TruckAudioSimVirtualAudio"
```

Para retirar el driver de la VM, cierra TruckAudioSim, restaura en el
mezclador la salida de las aplicaciones que cambiaste y copia el ID de
instancia que devolvió la enumeración. Usa ese ID en `/remove-device`.
Con `pnputil /enum-drivers` localiza el nombre publicado `oemNN.inf` cuya
entrada corresponda a TruckAudioSim.

```bat
pnputil /remove-device "ID_DE_INSTANCIA"
pnputil /delete-driver oemNN.inf /uninstall
```

## Comprobar que salen las muestras correctas

En la VM configura el endpoint a 48 kHz y realiza una pasada para cada
disposición 2.0, 5.1 y 7.1. Genera los WAV con `python tools/make-demo-surround.py`.
Usa `tas_endpoint_capture.exe --input-id ID --seconds 18 --save-wav prueba.wav`
mientras VLC reproduce el WAV correspondiente en el endpoint virtual.
Valida los canales con `tools/check-loopback.py` según
[el procedimiento de aceptación](DRIVER_ACCEPTANCE_ES.md).

Repite con dos aplicaciones en modo compartido. Confirma que ambas llegan
al motor y que cada una se oye una vez por Realtek con Atmos activo. Verifica
también que dos IDs iguales producen `AUDIO LOOP BLOCKED` y que, al quitar
el dispositivo de entrada, el motor cierra la captura con el error de Windows.
