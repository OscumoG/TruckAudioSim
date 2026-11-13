# Validación de TruckAudioSim en Windows 11 x64

Registro del 11 de noviembre de 2026. **Fase W0 en curso.**

| Comprobación | Resultado |
| --- | --- |
| Compilar y enlazar el código de usuario para Windows x64 | 35 ejecutables Release con MSVC 19.44.35230.0 (Visual Studio 2022) y Windows SDK 10.0.26100 dentro de Windows 11 Enterprise LTSC Evaluation x64, compilación 26100.1742. Incluye el cliente WinRT y los ejecutables CTest. |
| Pruebas automatizadas del núcleo en Windows | 26/26 aprobadas en la VM con CMake 3.31.10, Ninja 1.13.2 y Python 3.12.10. Se incluye el JSON Scania S y la conservación de los ocho canales en las pruebas simuladas. |
| Reproducción WASAPI estéreo en Windows | La salida emulada `Speakers (High Definition Audio Device)` recibió los 192000 cuadros del WAV de 48 kHz. La captura mostró 3.8 segundos consecutivos con 440 Hz a la izquierda y 660 Hz a la derecha, con una separación mínima medida de 45.5 dB. |
| Reproducción espacial y escucha física | El usuario confirmó que la prueba con Dolby Atmos para auriculares funcionó en la salida física Realtek. Windows devolvió cero objetos dinámicos en la salida usada en la VM. Windows Sonic y DTS Headphone:X quedan por confirmar por separado. |
| Endpoint virtual, WDK e instalación | Pendiente en W1. El árbol contiene módulos WDM experimentales. Aún faltan el proyecto SysVAD integrado, la mezcla real de varias apps y la instalación firmada. |
| Integración ETS2/ATS y aceptación del MVP | Pendiente. No se han verificado la cámara, la pausa y reanudación de Spotify, el editor completo ni la reproducción sin duplicación. |


## Probar Dolby Atmos para auriculares

1. Abre [Dolby Access en Microsoft Store](https://apps.microsoft.com/detail/9n0866fs04w8?gl=MX) y configura Dolby Atmos para auriculares si aún no está habilitado.
2. En **Configuración → Sistema → Sonido**, abre la salida física que usas y selecciona **Dolby Atmos para auriculares** en **Sonido espacial**. Si tus auriculares están conectados por jack, el nombre de la salida puede ser **Altavoces (Realtek Audio)**.
3. Extrae el ZIP completo y abre `Probar-Dolby-Atmos.cmd`. Escribe el número junto a la salida que acabas de configurar, por ejemplo `0` si aparece como `[0] Altavoces (Realtek High Definition Audio)`. También puedes pegar su ID completo. El ejecutable incluido requiere el [runtime Microsoft Visual C++ x64](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist/).
4. Escucha **un pitido a la izquierda y dos a la derecha**. El registro queda en `build-win/validation/w0-dolby-atmos.log` y se renueva en cada ejecución. Para completar la prueba, comprueba el formato seleccionado en Windows, las líneas `SPATIAL_DYNAMIC` y `192000 input frames`, y el orden audible.

El lanzador usa `--require-spatial`, así que termina con un error si la salida no admite el objeto dinámico. `SPATIAL_DYNAMIC` confirma el uso de objetos espaciales. El proveedor se confirma en las propiedades de esa misma salida en Windows. Microsoft Spatial Sound entrega los objetos de TruckAudioSim al [renderizador espacial elegido para la salida](https://learn.microsoft.com/windows/win32/coreaudio/spatial-sound).

En Windows 11 x64, compilación 26300.9974, el primer intento con la salida `[0] Altavoces (Realtek High Definition Audio)` falló en `GetDevice(stable ID)` porque el lanzador pasó `0` como ID. Tras corregir la selección se ofreció el lanzador actualizado y el comando directo de abajo. El usuario confirmó «Funcionó». El registro disponible corresponde al primer intento. Del intento exitoso se dispone de la confirmación del usuario, sin un recuento de objetos o cuadros ni un registro detallado de los lados que oyó.

```powershell
.\validacion-x64\tas_spatial.exe --smoke-test --wav .\validacion-x64\prueba_izquierda_derecha_48k.wav --output-index 0 --require-spatial
```

Para usar el perfil de cabina con un WAV PCM16 o float32 de 48 kHz, copia el ID de la misma salida y abre PowerShell en la carpeta extraída.

```powershell
$id = Read-Host 'Pega el ID de la salida con Dolby Atmos para auriculares'
$wav = Read-Host 'Ruta del WAV de 48 kHz, sin comillas'
.\validacion-x64\tas_spatial.exe --preset .\profiles\Scania_S_7_1_Fidelidad.json --wav $wav --output-id $id
```

## Escuchar la prueba con Windows Sonic

Extrae el ZIP y abre PowerShell en la carpeta extraída. La prueba guiada usa el ejecutable x64 y el WAV incluidos.

```powershell
.\tools\validate-windows.ps1 -GuidedSonic
```

Elige tus auriculares de la lista, activa Windows Sonic en esa salida y vuelve a la consola. El diagnóstico usará el ID de la salida que elegiste y pedirá que indiques si percibiste el recorrido izquierda → derecha → izquierda. El archivo `build-win/validation/w0-*.log` registra tanto la ruta y los cuadros reproducidos como tu observación. Si Windows devuelve cero objetos dinámicos o hay un error de reproducción, el registro conserva el error para diagnosticarlo.

### Ejecutar el diagnóstico directamente

1. Extrae el ZIP y abre PowerShell en la raíz del proyecto. Los dos ejecutables son de diagnóstico, se pueden ejecutar como usuario normal y requieren el [runtime Microsoft Visual C++ x64](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist/).
2. En Windows 11, abre **Configuración → Sistema → Sonido**, selecciona tus auriculares físicos y activa **Windows Sonic para auriculares** en las propiedades de ese dispositivo. También puedes usar el proveedor espacial que ya tengas habilitado. Anota el proveedor seleccionado, ya que la API de objetos no devuelve su nombre.
3. Lista las salidas, copia el ID que aparece bajo tus auriculares y ejecuta la prueba. Puedes usar `--output-index N` en lugar del ID si acabas de listar los dispositivos.

```powershell
.\validacion-x64\tas_spatial.exe --list-devices
$id = Read-Host 'Pega el ID de tus auriculares'
.\validacion-x64\tas_spatial.exe --smoke-test --wav .\validacion-x64\demo_stereo_48k.wav --output-id $id --require-spatial
$LASTEXITCODE
```

4. Espera el mensaje `SPATIAL_DYNAMIC` y un total de **192000 frames**. El objeto debe empezar a la izquierda, pasar a la derecha y volver a la izquierda durante los cuatro segundos del WAV. Escucha que sale por los auriculares elegidos y registra si el desplazamiento se distingue. El rango X aproximado debe ser de −1 a +1 m. Un código 2 indica que la ruta espacial requerida no estaba disponible y el WAV no se reprodujo. Un código 1 indica un fallo y la consola muestra el paso y, cuando corresponde, su HRESULT.
5. Para comprobar el respaldo, desactiva el sonido espacial en ese mismo endpoint y ejecuta el comando sin `--require-spatial`. Espera `WASAPI_STEREO_FALLBACK`, 192000 frames y el WAV estéreo original sin movimiento. Después puedes volver a activar tu proveedor. Dolby Atmos y DTS se prueban por separado si ya están disponibles en el dispositivo.

La prueba admite otros WAV estéreo de 48 kHz en PCM16 o float32. En la ruta espacial convierte los dos canales en un objeto mono móvil. La ruta WASAPI reproduce los dos canales originales. La prueba comprueba selección, streaming y movimiento, y deja las mediciones de fidelidad y la comparación A/B a nivel igualado para W2.

Si quieres guardar el resultado de tus auriculares en un registro, ejecuta lo siguiente desde la raíz del ZIP. Usa el ID que copiaste en `$id`. `-AudioOnly` usa el ejecutable y el WAV incluidos, así que puedes correrlo antes de instalar Visual Studio, CMake o Python.

```powershell
.\tools\validate-windows.ps1 -ListAudioDevices
.\tools\validate-windows.ps1 -AudioOnly -OutputId $id -RequireSpatial
# Después de apagar el sonido espacial en esa misma salida
.\tools\validate-windows.ps1 -AudioOnly -OutputId $id
```

El script deja los registros `w0-*.log` en `build-win/validation/` y anota las huellas SHA-256 de los archivos que usó. Acompaña los registros con lo que escuchaste y el proveedor espacial seleccionado.

### Verificar izquierda y derecha por separado

En la primera escucha física se percibió un desplazamiento de derecha a izquierda. Queda por confirmar si se oyó así desde el inicio y qué ruta imprimió la consola. La trayectoria original va de izquierda a derecha durante los primeros dos segundos y vuelve a la izquierda durante los dos siguientes. Para identificar cada lado, el WAV de referencia da **un pulso a la izquierda y dos a la derecha**.

Usa el mismo ID de auriculares en ambas pruebas. Primero desactiva el sonido espacial de esa salida y ejecuta el siguiente comando. Comprueba que indique `WASAPI_STEREO_FALLBACK`.

```powershell
.\validacion-x64\tas_spatial.exe --smoke-test --wav .\validacion-x64\prueba_izquierda_derecha_48k.wav --output-id $id
```

Vuelve a activar Windows Sonic en esa salida y ejecuta el perfil de dos fuentes fijas, con el oyente centrado. Espera `Finished (192000 input frames consumed).` La prueba requiere dos objetos dinámicos para que ambas fuentes suenen.

```powershell
.\validacion-x64\tas_spatial.exe --preset .\validacion-x64\prueba_izquierda_derecha.json --wav .\validacion-x64\prueba_izquierda_derecha_48k.wav --head-x 0 --output-id $id
```

Con Sonic todavía activo, usa el mismo WAV en la prueba móvil para escuchar el orden de los extremos. El primer pulso suena cerca de la posición inicial y los dos siguientes cerca del extremo opuesto. El regreso queda en silencio después de los pulsos.

```powershell
.\validacion-x64\tas_spatial.exe --smoke-test --wav .\validacion-x64\prueba_izquierda_derecha_48k.wav --output-id $id --require-spatial
```

Registra en qué oído sonó el pulso único y en cuál sonaron los dos pulsos para cada ruta, junto con la salida de la consola. Las posiciones de referencia siguen la [convención de ejes de Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/spatialaudioclient/nf-spatialaudioclient-ispatialaudioobject-setposition), con X negativa a la izquierda y X positiva a la derecha.

## Compilar y registrar el resultado nativo

Usa Windows 11 x64 con Visual Studio 2022, la carga **Desarrollo para el escritorio con C++**, Windows SDK 10.0.26100 o compatible, CMake 3.21 o posterior y Python 3.8 o posterior en PATH. El WDK corresponde a la fase del controlador.

En PowerShell, desde la raíz:

```powershell
.\tools\validate-windows.ps1
.\tools\validate-windows.ps1 -OutputId $id -RequireSpatial
```

La primera invocación configura, compila y ejecuta CTest. La segunda repite esas comprobaciones, genera el WAV, abre la salida física y ejecuta la prueba espacial. También admite `-OutputIndex N`. Los registros `w0-*.log` y `w0-*.xml` quedan en `build-win/validation/`. Agrega al registro el modelo de auriculares, el proveedor configurado en Windows, el resultado audible, si se percibió el movimiento y el resultado del respaldo estéreo. La evidencia automatizada registra el PCM enviado, mientras que el resultado audible se confirma por separado.

También puedes ejecutar los comandos directamente:

```powershell
cmake -S . -B build-win -G 'Visual Studio 17 2022' -A x64 -DTAS_REQUIRE_PYTHON_TESTS=ON
cmake --build build-win --config Release
ctest --test-dir build-win -C Release --output-on-failure --no-tests=error --output-junit build-win/windows-x64-ctest.xml
```

La suite completa contiene 26 pruebas, incluidas cuatro que requieren Python. El flujo de GitHub Actions está configurado para compilar y ejecutar esa suite con MSVC en `windows-2022` si se ejecuta en un repositorio. El flujo de CI cubre la compilación y las pruebas automáticas. La reproducción audible se realiza en Windows 11 con el dispositivo físico.

## Reproducir la validación de la VM

La prueba usó una VM QEMU 10.0.13 con 4 vCPU, 6 GiB de RAM, firmware UEFI con claves de Secure Boot, TPM virtual, Intel HDA emulado y red desconectada. Desde una consola x64 de Visual Studio 2022 con Ninja en PATH, se puede repetir la compilación con la misma configuración Release:

```powershell
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL -DTAS_REQUIRE_PYTHON_TESTS=ON
cmake --build build-release -j 4
ctest --test-dir build-release -C Release --output-on-failure --no-tests=error --output-junit build-release/windows-x64-ctest.xml
.\build-release\tas_spatial.exe --list-devices
.\build-release\tas_spatial.exe --smoke-test --wav .\validacion-x64\demo_stereo_48k.wav --output-index 0
```

El índice 0 identificaba la única salida activa de la VM. La captura PCM16 de 48 kHz se obtuvo del dispositivo HDA emulado. Se analizaron los tonos de 440 y 660 Hz en ventanas de 100 ms, comparando la amplitud de cada tono en ambos canales. La captura demuestra la salida estéreo WASAPI en esta configuración. La escucha con Windows Sonic se hace en los auriculares físicos.

## Retirar la prueba y continuar con el controlador

Para retirar esta entrega, cierra los ejecutables y elimina el directorio extraído. Los ejecutables de diagnóstico se usan directamente desde esa carpeta. Si cambiaste el formato espacial desde Configuración, vuelve a elegir el que usabas.

El registro del intento exitoso con Dolby Atmos permitiría verificar el modo espacial, los cuadros y el orden audible de los pitidos. Para W1, empieza por compilar el [SysVAD oficial de Microsoft](https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad) con el [WDK compatible con Visual Studio 2022](https://learn.microsoft.com/windows-hardware/drivers/other-wdk-downloads). La adaptación del endpoint, el transporte PCM y la instalación/desinstalación del controlador deben verificarse en una VM Windows 11 aislada. Los requisitos de canales, aislamiento y prueba con apps están en [aceptación del controlador](DRIVER_ACCEPTANCE_ES.md).

Microsoft documenta [cómo activar Windows Sonic](https://support.microsoft.com/es-es/windows/hardware/drivers/how-to-turn-on-spatial-sound-in-windows), los [objetos de Spatial Sound](https://learn.microsoft.com/en-us/windows/win32/coreaudio/render-spatial-sound-using-spatial-audio-objects) y la [reproducción WASAPI](https://learn.microsoft.com/en-us/windows/win32/coreaudio/rendering-a-stream).

## Continuación v0.18.0: prueba portable y limitación Windows

El 11-nov-2026 se compiló el núcleo Release en Linux con Python habilitado y
pasaron **28/28** tests CTest, incluyendo el relleno de silencio al recibir
bloques incompletos y la comprobación estática del manifiesto WDM. Este
resultado **no** significa que se haya compilado o instalado el driver WDM de
v0.18.0 con WDK ni que se haya probado el nuevo verificador con audio Windows.
El plan de validación específica y los comandos se encuentran en
`docs/AVANCE_W1_V018.md`.

## v0.23.3 — nivel de referencia y control al 500%

- El motor tas_spatial.exe se compiló para Windows x64. Pasaron 31 pruebas
  CTest y 237 pruebas del editor en Node.
- La comparación con tonos estéreo de amplitud 0,1 a 100, 1000, 2500 y
  8000 Hz dio +11,6 dB al 100% para el Scania S Realista y +8,3 dB para
  los perfiles 5.1 y 7.1, medidos en los objetos PCM antes de la salida
  espacial. El valor 500% dio cinco veces la amplitud de 100% en estos
  tonos sin recorte. Con material más fuerte el límite PCM puede recortar.
- Queda pendiente comparar el nivel percibido en Windows, con la misma
  pista, salida física y configuración de Dolby Atmos, y probar ETS2.
