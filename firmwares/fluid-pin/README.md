# Fluid

[English](README.md) · [Español](README.es.md)

A 2D PIC/FLIP water simulation that responds to tilt and motion. A 38 × 18
staggered grid and 573 particles drive a 67 × 30 image, rendered as 8 × 8 pixel
tiles on the display. The QMI8658C supplies acceleration and rotation.

![Water with simulated tilt](preview.gif)

Hold BOOT for 1.5 seconds and release to return to the launcher. Touch is unused.

## Implementation

`main/fluid.c` handles particle integration, separation, transfers to and from
the grid, pressure projection and wall collisions. `main/motion.c` filters
sensor input and separates gravity from linear acceleration. Constants live in
`main/fluid_config.h`.

The renderer reads immutable snapshots. Density reconstruction conserves mass;
speed determines water color. The solver, snapshots and two DMA bands use
internal SRAM. Physics and rendering run in separate tasks without steady-state
heap allocation. See the [physics notes](docs/physics-review.md) for the wall
boundary treatment and remaining physical checks.

## Development

From the repository root:

```sh
./tools/test.sh --app fluid-pin
./tools/test.sh --app fluid-pin --soak
python3 tools/make_previews.py fluid-pin
idf.py -C firmwares/fluid-pin -D FLUID_DIAGNOSTICS=0 build
```

The soak option includes two 30-minute simulated runs. Tests cover conservation,
wall release, settling, density reconstruction, tilt, translation, shaking and
invalid-state recovery. GIF previews use simulated gravity input.

Set `FLUID_DIAGNOSTICS=1` for telemetry. CMake remembers this option, so specify
the intended value when switching modes. `tools/capture.py` records diagnostic
output from the device. Use the root [build script](../../build-and-flash.sh)
to install the collection; hardware rules are in the shared
[board contract](../../documentation/AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.md).
