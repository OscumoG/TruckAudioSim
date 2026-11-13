# Perfiles de cabina

Abre TruckAudioSim y busca el modelo en **Layout → Preset** o en el árbol del
camión. Hay 50 combinaciones de juego y modelo con tres perfiles cada una, para
un total de 150 perfiles. Cada camión utiliza una cabina representativa.
Se incluyen Scania Touring, Volvo 9700 DD y MAN Lion’s Coach de ETS2,
los cuatro Ford de Road Trip y el Škoda Superb / Scout en ambos juegos.
Los autobuses y el Škoda usan posiciones estimadas y se identifican por
el nombre o un alias de telemetría.

| Variante | Posición y reproducción |
| --- | --- |
| Realista | Conserva las ubicaciones de referencia de Trucker AUX en el juego. Elimina duplicados exactos del mismo canal en la misma posición. Mantiene las bocinas que trae ese diseño, incluido el subwoofer si estaba presente. |
| 5.1 | Dos frontales siguiendo los puntos del diseño de cabina, central en el tablero, subwoofer en posición baja y un par envolvente. Aprovecha las ubicaciones traseras existentes cuando están disponibles. |
| 7.1 | El mismo frente y subwoofer, con pares laterales y traseros separados. Aprovecha las ubicaciones existentes en el perfil de referencia y estima los puntos adicionales a partir de las dimensiones de la cabina. |

Todas las bocinas quedan a **0 dB** en el editor. El motor conserva
los filtros según el tipo de bocina y calcula el margen de la mezcla.

En 5.1 y 7.1, el central Midrange reproduce de 500 a 2500 Hz y el
subwoofer usa un paso bajo a 120 Hz. Los frontales son de rango completo
para conservar graves incluso cuando el LFE de la fuente está vacío.
Con música estéreo, el centro y el subwoofer reciben una mezcla L/R
a nivel reducido. Las bocinas envolventes también reciben una señal
atenuada de su lado con fuentes estéreo y 2.1. Con PCM multicanal, el
perfil 5.1 combina los laterales y traseros de una pista 7.1 en su par
envolvente. El perfil 7.1 reproduce una pista 5.1 en ambos pares
envolventes y mantiene ocho canales discretos cuando la fuente los entrega.

Realista usa las posiciones de referencia del modelo. El motor mezcla
el centro y los envolventes hacia las bocinas laterales de la cabina
cuando la fuente es multicanal. En algunos modelos se ajusta el tipo
del altavoz frontal para cubrir más frecuencias sin moverlo.

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
