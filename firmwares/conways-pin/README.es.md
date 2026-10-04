# Conway

[English](README.md) · [Español](README.es.md)

El juego de la vida de Conway en una cuadrícula toroidal de 89 × 40: los bordes
opuestos están conectados. Cada celda ocupa 6 × 6 píxeles en la pantalla de
536 × 240. La simulación avanza diez generaciones por segundo y comienza con
cuatro patrones dispersos de gliders o R-pentominós. Se agregan gliders
periódicamente y después de un tiempo sin actividad.

![Juego de la vida](preview.gif)

## En el dispositivo

Esta grabación muestra el juego de la vida de Conway en la pantalla Waveshare.

![Juego de la vida de Conway en el dispositivo físico](device-demo.gif)

Tocá la pantalla para colocar un R-pentominó, B-heptominó, Diehard o cuadrado de
3 × 3. Sólo el comienzo del contacto coloca el patrón; mantener o arrastrar el
dedo no deja un rastro. Las células vivas son cian, los nacimientos destellan en
blanco y las muertes se desvanecen en magenta. Mantené BOOT durante 1,5 segundos
y soltalo para volver al menú.

`main/life.c` implementa las reglas y coloca patrones; `main/palette.c` genera
los mosaicos RGB565. El dispositivo envía las franjas de 60 filas que cambiaron
y espera la finalización DMA antes de reutilizar el búfer.

Desde la raíz:

```sh
./tools/test.sh --app conways-pin
python3 tools/make_previews.py conways-pin
idf.py -C firmwares/conways-pin build
```

Usá el [script raíz](../../build-and-flash.sh) para instalar la colección.
El [contrato de la placa](../../documentation/AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.es.md)
describe la pantalla y el tacto.
