# Conway

[English](README.md) · [Español](README.es.md)

Conway's Game of Life on an 89 × 40 toroidal grid. Each cell occupies a 6 × 6
pixel tile on the 536 × 240 display. The simulation advances at ten generations
per second, starting with four sparse glider or R-pentomino patterns. New gliders
enter periodically, and quiet worlds receive another glider after a delay.

![Game of Life](preview.gif)

Tap to place an R-pentomino, B-heptomino, Diehard or 3 × 3 square. Only the first
touch-down places a pattern; holding or dragging does not paint a trail.
Live cells are cyan, births flash white and deaths fade through magenta.
Hold BOOT for 1.5 seconds and release to return to the launcher.

`main/life.c` implements the rules and pattern placement; `main/palette.c`
creates RGB565 tiles. The device renderer sends changed 60-row display strips
and waits for DMA completion before reusing a buffer.

From the repository root:

```sh
./tools/test.sh --app conways-pin
python3 tools/make_previews.py conways-pin
idf.py -C firmwares/conways-pin build
```

Use the root [build script](../../build-and-flash.sh) to install the collection.
See the shared [board contract](../../documentation/AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.md)
for display and touch details.
