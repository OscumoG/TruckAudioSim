# Perfiles de cabina

Abre TruckAudioSim y busca el modelo en **Layout → Preset** o en el árbol del
camión. Cada configuración de cabina incluida en el catálogo de referencia
tiene tres variantes. Son 27 configuraciones de ETS2 y 35 de ATS, para un
total de 186 perfiles de 41 modelos. Se distinguen las cabinas y distancias
de quinta rueda cuando el perfil original las identifica.

| Variante | Posición y reproducción |
| --- | --- |
| Realista | Conserva las ubicaciones de referencia de Trucker AUX en el juego. Elimina duplicados exactos del mismo canal en la misma posición. Mantiene las bocinas que trae ese diseño, incluido el subwoofer si estaba presente. |
| 5.1 | Dos frontales siguiendo los puntos del diseño de cabina, central en el tablero, subwoofer en posición baja y un par envolvente. Aprovecha las ubicaciones traseras existentes cuando están disponibles. |
| 7.1 | El mismo frente y subwoofer, con pares laterales y traseros separados. Aprovecha las ubicaciones existentes en el perfil de referencia y estima los puntos adicionales a partir de las dimensiones de la cabina. |

Todas las bocinas quedan a **0 dB** en el editor. El motor aplica su reserva de
nivel global y los filtros por tipo. En 5.1 y 7.1, la central Midrange usa la
banda 500–2500 Hz y el subwoofer usa un paso bajo de 120 Hz. Las frontales
de rango pequeño usan un paso alto de 120 Hz. Con una fuente estéreo, el
central recibe la mezcla L/R filtrada y el subwoofer los graves de esa
misma mezcla. Las pistas 5.1 y 7.1 conservan su canal central y LFE
discretos. El par envolvente del perfil 5.1 admite las máscaras lateral
y trasera de Windows.

Steam Streaming Speakers puede seguir entregando una mezcla estéreo. En ese
caso se oyen los frontales, el centro y el subwoofer de los perfiles
envolventes. Para oír todos los canales discretos, la fuente y la salida
de captura deben entregar PCM multicanal.

## Referencias de posición

Los puntos de partida son los presets de Trucker AUX 0.4.0, ajustados por
su autor dentro de ETS2 y ATS. Sus coordenadas son referencias del
simulador y no mediciones OEM certificadas. Los puntos envolventes añadidos
son estimaciones de instalación por cabina. El equipamiento real varía por
año, cabina y paquete de audio.

- [Trucker AUX, repositorio de referencia](https://github.com/SergeyDoes/trucker-aux/tree/v0.4.0)
- [Volvo Trucks, diagramas eléctricos con posiciones de puerta, tablero y pared trasera](https://www.volvotrucks.us/media/vtna/files/shared/body-builder/wiring-diagrams/dec-2020-p6605-22622040061w.pdf)
- [Freightliner Cascadia, guía del sistema premium con altavoces en el dormitorio y subwoofer bajo la litera](https://www.scribd.com/document/419824080/Audio-System)

El generador reproducible está en `tools/generate-cab-presets.py`.
