# 3D Maze

[English](README.md) · [Español](README.es.md)

Un homenaje redibujado al salvapantallas Windows 3D Maze, con recorrido
automático, para la Waveshare ESP32-S3-Touch-AMOLED-1.91. Dibuja a 268 × 120 y
escala a 536 × 240. Un toque de BOOT no tiene acción; mantenelo 1,5 segundos
y soltalo para volver al menú.

El laberinto usa 10 × 10 celdas con paredes finas, ramificaciones, cruces y
callejones sin salida. La cámara sigue la pared derecha a tres celdas por
segundo; cada giro de 90° dura unos 0,24 segundos. Todos los laberintos son
árboles conectados: el recorrido siempre llega a la salida. El movimiento
consume el tiempo completo de cada cuadro, incluso al cruzar una celda o un giro.

Ladrillo rojo, mortero claro, techo moteado, madera continua, cámara baja e
iluminación clara siguen la
[referencia de Windows 3D Maze](https://github.com/coco-monier/Windows-95-3D-Maze).
Las texturas y objetos se dibujan en código. Una cara amarilla con rasgos azules
marca la salida; las paredes bajan y aparece otro laberinto. Algunas vueltas
incluyen una rata que recorre pasillos por separado o un octaedro gris que gira.
Los objetos tienen coordenadas del mundo y las paredes los ocultan. Se alternan
vueltas con objetos y sin ellos. No se implementa el giro de pantalla del original.

![Exploración del laberinto](preview.gif)

## Desarrollo

Desde la raíz:

```sh
./tools/test.sh --app maze-pin
python3 tools/make_previews.py maze-pin
idf.py -C firmwares/maze-pin build
```

Las pruebas cubren 64 semillas, paredes recíprocas, conexión, colisiones de
cámara y rata, repetibilidad, salidas, independencia de fps, oclusión y límites.
Usá el [script raíz](../../build-and-flash.sh) para instalar. En la colección
completa, Maze ocupa `ota_5`; su dirección sale de `partitions.csv`.
Las selecciones web reasignan los subtipos OTA consecutivamente. El atajo USB
es `6`. Los resultados históricos están en
[validación de apps](../../documentation/APP_VALIDATION.md).
