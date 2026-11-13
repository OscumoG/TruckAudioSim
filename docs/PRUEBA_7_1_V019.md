# TruckAudioSim v0.19.0 — prueba de ocho objetos espaciales discretos (Windows)

## Qué se implementó

- `src/common/channel_layout.cpp`: esquema diagnóstico sin mezcla, con una fuente
  mono por canal WAVEFORMATEXTENSIBLE. Respeta el orden de bits `FL, FR, FC,
  LFE, BL, BR, SL, SR` para señales 7.1; también admite estéreo, 2.1 y 5.1
  (side/back). Posiciona cada objeto en coordenadas relativas al oyente.
- `--channel-test` en `src/windows/main.cpp`: opción **nueva del código fuente**,
  que exige salida física explícita, intenta activar tantos objetos dinámicos
  como canales originales, no descarta canales cuando faltan objetos y registra
  por objeto número de muestras no nulas, RMS y pico. No hay fallback estéreo.
  Necesita compilar v0.19.0 con Visual Studio/Windows SDK.
- `tools/make-channel-identity.py`: produce tonos **originales** en una señal
  PCM16 de 8 segundos a 48 kHz, con máscara de canales 7.1 `0x63F`.
  Solo un canal suena durante cada segmento de un segundo, por orden estándar.
- `profiles/Diagnostico_7_1_Objetos.json`: ocho altavoces con identificadores
  discretos. Compatible con el **ejecutable anterior** ya incluido, que acepta
  `--preset` y `--wav`. En ese ejecutable los niveles pasan por la ganancia
  conservadora del perfil, no por el modo especial `--channel-test`.
- `Probar-7.1-8-objetos.cmd`: menú de selección de salida física + prueba con
  los ejecutables previos. No instala nada ni modifica el sonido predeterminado.

## Prueba inmediata sin compilar nada

1. Extraer el ZIP completo y seleccionar Dolby Atmos para auriculares en la
   **salida física Realtek**, como en la prueba v0.18.0.
2. Abrir `Probar-7.1-8-objetos.cmd`, seleccionar el índice de esa salida
   física (en la última sesión era 0; podría cambiar) y mantener el volumen
   moderado. No seleccionar el endpoint a capturar.
3. Alternativa desde PowerShell, en la carpeta raíz del ZIP:

   ```powershell
   .\bin-x64\tas_spatial.exe --preset .\profiles\Diagnostico_7_1_Objetos.json --wav .\validacion-x64\prueba_7_1_canales_48k.wav --output-index 0
   ```

   El ejecutable preempaquetado **no incluye** `--channel-test` nuevo.
   Solo permite probar que ocho objetos reciben un WAV multicanal mediante
   un preset. Busca: `Rendering ... input=8ch mask=0x63f as 8 moving spatial objects`
   y `Finished (384000 input frames consumed)`.

## Qué escuchar (uno por segundo)

| Intervalo | Canal discreto | Posición virtual |
|---|---|---|
| 0–1 s | FL | Frontal izquierda |
| 1–2 s | FR | Frontal derecha |
| 2–3 s | FC | Frontal centro |
| 3–4 s | LFE | Graves, posición baja central; poco localizable |
| 4–5 s | BL | Detrás izquierda |
| 5–6 s | BR | Detrás derecha |
| 6–7 s | SL | Lateral izquierda |
| 7–8 s | SR | Lateral derecha |

La percepción de la posición varía entre auriculares y usuarios; la escucha
subjetiva no verifica por sí sola la pureza eléctrica ni la ruta del driver.

## Prueba al compilar v0.19.0 en Windows 11

```powershell
cmake -S . -B build-win -A x64 -DTAS_REQUIRE_PYTHON_TESTS=ON
cmake --build build-win --config Release
ctest --test-dir build-win -C Release --output-on-failure
.\build-win\Release\tas_spatial.exe --channel-test --wav .\validacion-x64\prueba_7_1_canales_48k.wav --output-index 0 --require-spatial
```

Nota: según generador de CMake, el ejecutable puede quedar en otra carpeta
(`build-win\Release` corresponde al generador de Visual Studio multi-config).
Busca `Activated discrete objects: 8/8`, las ocho estadísticas `Object ...`,
`Discrete channel source check: PASS` y 384000 frames.

### Validación local real de esta entrega

- Linux GCC Release, CMake + CTest: **30/30** pruebas completas.
- La prueba `discrete_channel_object_identity` inyecta datos en un solo canal
  cada vez y comprueba que solo su correspondiente objeto mono recibe audio,
  para todos los esquemas admitidos (2.0, 2.1, 5.1 side/back y 7.1).
- `packaged_discrete_7_1_wav` comprueba máscara, formato y cero contaminación
  entre canales durante los ocho segundos del WAV distribuido.
- **No se ha compilado ni ejecutado el cambio C++ nuevo en Windows en este
  entorno; no se ha validado todavía el driver `.sys` en Windows/WDK.**
- El `.sys` dentro de `driver/lab-x64` continúa siendo experimental y NO debe
  instalarse en el equipo principal.

## Próximos bloqueos

1. Compilar Windows x64 y registrar la prueba `--channel-test` nativa.
2. Validar la carga y el transporte PCM de la salida virtual en una VM con WDK.
3. Probar en vivo con fuentes reales y los perfiles de Trucker Aux.
4. Integración de telemetría ETS2/ATS y gestión de cambios de dispositivo.
