# Validación de TruckAudioSim v0.22.1

- Se recompiló el motor espacial para Windows x64 con Clang/LLD 19 y Windows SDK 26100.
- Pasaron 31 pruebas CTest del motor y 237 pruebas Node. La prueba de interfaz con IPC simulado comprobó el deslizador al 400% y el valor enviado al motor y al guardado. También se verificó que el ajuste persiste al volver a cargarlo desde disco.
- La prueba de escucha con esta compilación queda pendiente en Windows.

# Validación de TruckAudioSim v0.22.0

- La interfaz deriva de Trucker AUX v0.4.0, commit 90d7718441db92d92b95fc48a75d02ba360f7a8f. Los dos DLL de juego proceden del ZIP oficial de esa versión. Se verificaron sus exportaciones `scs_telemetry_init` y `scs_telemetry_shutdown` y su arquitectura Windows x64.
- Se compilaron `tas_spatial.exe` y `tas_media_control.exe` para Windows x64 con Clang/LLD 19, SDK de Windows 26100 y CRT estático. La pausa usa Windows.Media.Control.
- Pasaron 31 pruebas CTest del motor y 236 pruebas Node. La prueba de interfaz con IPC simulado comprobó el botón de instalación, el perfil Scania, la posición exterior, el silencio a distancia y el estado eléctrico enviado al controlador.
- Falta ejecutar el paquete en Windows y comprobar en ETS2 la instalación, la cámara real, el audio y la pausa de Spotify o VLC. La prueba de interfaz simuló el motor y no ejecutó WebGL.

Para recompilar con CMake y Visual Studio desde Codigo-Fuente:

```powershell
cmake -S . -B build-win -A x64
cmake --build build-win --config Release --target tas_spatial tas_media_control
Copy-Item build-win\Release\tas_spatial.exe,build-win\Release\tas_media_control.exe bin-x64\
```

El proyecto `desktop` contiene el código de la interfaz. Ejecuta `npm ci` y
`npm test` allí. `npm run dist` construye el ZIP x64 con ambos motores y los plugins.

## v0.23.0

Se compiló el motor espacial para Windows x64. Pasaron 31 pruebas CTest y
237 pruebas Node. La colección cargó los 186 perfiles nuevos sin errores.
Se comprobó el nivel de 0 dB de cada bocina, el orden de canales, la
importación y exportación y la conservación del tipo de perfil al
enviarlo al motor. Las pruebas del motor verificaron las bandas de la
central y del subwoofer. Falta la prueba de escucha en ETS2/ATS con Windows.
