# Conway's Pin

Conway's Game of Life for the Waveshare **ESP32-S3-Touch-AMOLED-1.91** (SKU 28596).
The landscape 536 × 240 display shows a centered 89 × 40 toroidal grid: each cell is
6 × 6 physical pixels, with a 1-pixel black margin on each side. The world advances
at a target of 10 generations per second.

Cells follow the standard B3/S23 rules with wraparound edges. At boot the world
starts with spaced, rotated gliders and R-pentominoes. It is never cleared while
running. Every 150 generations (about 15 seconds), a glider enters from the top,
right, bottom, then left edge in turn. If the world becomes quiet, another glider
arrives after about eight seconds with fewer than 12 live/dead cell changes per
generation. Entry lanes are chosen from clear areas, so patterns can move
inward before meeting the existing world.

The AMOLED uses 6 x 6 cyber LED tiles: a bright cyan rim and core for live cells,
a white birth flash, and a deep-red-rimmed hot-magenta core that fades over four
frames on death. A tiny top-right `G` readout shows the current generation. The
firmware has no planned stop.

Tap the touch screen to place a randomly chosen R-pentomino, B-heptomino,
Diehard, or solid 3 x 3 square centered near the selected Life cell. The first
three use one of four random rotations. Only the initial touch-down places a
stamp, so a drag or held finger does not paint a trail across the grid. The
new cells flash white, then join the cyan Life animation.

![Simulated generation preview](preview.png)

The program uses the board's QSPI display and the official Waveshare initialization
sequence documented in its [ESP-IDF example](https://github.com/waveshareteam/ESP32-S3-AMOLED-1.91/blob/main/02_Example/ESP-IDF/03_LVGL_V8_Test/LVGL_Test/main/example_qspi_with_ram.c).
It uses one 60-row DMA strip rather than a full framebuffer; the two Life state
arrays, strip, and color tile table use about 100 KiB in total. Wi-Fi and PSRAM
are not needed.
The renderer skips strips whose cell states have not changed, reducing
QSPI traffic when the pattern becomes sparse or settled. Serial logs report the
average work time, strips sent, and frame rate every 100 frames.

The previous 4 × 4 build on the attached ESP32-S3 (revision v0.2) held 10 frames
per second through more than 1,000 generations without a reset. The 6 × 6 build
uses a 60-row DMA strip; performance measurements for this layout are pending.

## Build and flash

Install ESP-IDF 5.5.x, then source its `export.sh`. With the board connected by
USB-C and a writable serial port:

```sh
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

For a different serial device, replace `/dev/ttyACM0`. Exit the monitor with
Ctrl+]. If the board is not detected for flashing, use its BOOT/reset sequence
as described in the [board guide](Waveshare-ESP32-S3-Touch-AMOLED-1.91-SKU28596-ESP-IDF-Guide.md).

The host-only Life check can be run with:

```sh
cc -O2 -Wall -Wextra -I main tests/life_test.c main/life.c -o /tmp/conways-pin-life-test
/tmp/conways-pin-life-test
```

The host-only palette check can be run with:

```sh
cc -O2 -Wall -Wextra -I main tests/palette_test.c main/palette.c -o /tmp/conways-pin-palette-test
/tmp/conways-pin-palette-test
```

The touch coordinate mapping check can be run with:

```sh
cc -O2 -Wall -Wextra -I main tests/touch_map_test.c main/touch_map.c -o /tmp/conways-pin-touch-map-test
/tmp/conways-pin-touch-map-test
```

To inspect an exact-color desktop preview of a generation, compile the shared
palette and Life code with `tests/preview.c`:

```sh
cc -O2 -I main tests/preview.c main/life.c main/palette.c -o /tmp/conways-pin-preview
/tmp/conways-pin-preview 400 /tmp/conways-pin-400.ppm
```
