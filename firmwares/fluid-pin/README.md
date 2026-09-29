# Fluid Pin — ESP32-S3 AMOLED

A backpack pin with a 2D PIC/FLIP fluid simulation for the Waveshare
ESP32-S3-Touch-AMOLED-1.91. It runs a 38 × 18 MAC grid with 573 particles and
renders a 67 × 30 logical image as crisp 8 × 8 RGB565 pixels on the 536 × 240
AMOLED. Wi-Fi, Bluetooth, touch and LVGL are unused.

The current solver revision fixes wall separation and speeds up gravity-driven
motion. The user confirmed the wall-release correction in `2df7ab5` as a stable
point, saved in checkpoint `ea0b123`. Density reconstruction and fluid-color
corrections for idle dark gaps are flashed for validation. See the
[physics review](docs/physics-review.md) for causes,
changes, test coverage, and remaining device checks.

## Simulation

Behavioral constants live in [fluid_config.h](main/fluid_config.h). Fill is
0.40, FLIP ratio 0.72, time scale 1.60, gravity 30 cells/s², and velocity damping
0.990 per step. The solver uses two particle separation passes and 22 pressure
passes per 60 Hz physics update.

1. Advect particles, separate overlaps, and resolve collisions.
2. Transfer particle velocities to the grid and calculate density.
3. Save the unforced grid, add external acceleration, and apply boundaries.
4. Solve pressure, allowing wall cells to release liquid without suction.
5. Extend separating wall velocities for gathering so the edge row releases.
6. Gather PIC/FLIP velocities, apply bounded motion assists, and publish.

The renderer receives immutable snapshots and filters sparse particle density
to prevent holes inside idle liquid. The filter conserves density and preserves
wall contact. Particle splat weights sum to one; particle count and render mass
are checked independently of solver stability.
Water color follows velocity; variations in marker spacing do not darken the
liquid's interior.
The solver, renderer, snapshots, and two 48-row DMA display bands use internal
SRAM. There are no steady-state heap allocations. Rendering overlaps DMA.

## Build and flash

```sh
source /home/juan/.local/share/esp-idf/export.sh
idf.py -D FLUID_DIAGNOSTICS=1 build
idf.py -p /dev/ttyACM0 flash
```

For quiet production firmware, build explicitly with
`idf.py -D FLUID_DIAGNOSTICS=0 build` and flash after device validation. CMake
remembers these options, so specify the intended value when changing modes.

## Host validation

```sh
make -C tools all
/tmp/fluid-pin-host/test-motion
/tmp/fluid-pin-host/test-tilt
/tmp/fluid-pin-host/test-boundaries
/tmp/fluid-pin-host/test-render
/tmp/fluid-pin-host/test-shake
/tmp/fluid-pin-host/test-walk
/tmp/fluid-pin-host/test-axis-translation --soak
/tmp/fluid-pin-host/v9-validation --soak
```

The two `--soak` runs each simulate 30 minutes. `test-boundaries` checks immediate
acceleration away from all four walls, drainage of a vertical side column, and
clearing of the actual rendered edge row after reversals and rapid rotations.
`test-render` checks one-minute idle holds in eight poses, with no enclosed
black pixels after settling, plus air gaps, clearing, and density conservation.
Three five-minute holds also check interior brightness to catch dark seams
that a binary hole test misses, including holds immediately after startup.
`test-walk` measures changed occupied pixels in the actual rendered output,
rather than accepting only a brightness change. Translation tests cover
horizontal and vertical upright slides and face-up slides. The core validation
also checks invalid-state recovery and writes `/tmp/v9-{seed,settled,right}.ppm`.

## Sensors and device validation

The QMI8658 uses ±8 g acceleration and ±1024 degrees/s rotation. Its configured
ODR is about 224.2 Hz in six-axis mode; fresh paired samples are read at 125 Hz.
The complementary filter tracks gravity with the gyro, separates linear
acceleration, and learns stationary gyro bias. The input curve emphasizes small
walking motions. A pitch guard limits false vertical translation during tilt.

`IMU_TO_SCREEN_GRAVITY` is the explicit, previously verified board mapping:

- upright: positive screen y gravity;
- left edge down: negative screen x gravity;
- right edge down: positive screen x gravity;
- face-up: nearly zero gravity in the screen plane.

Capture a diagnostic build with:

```sh
python tools/capture.py --seconds 30 --out /tmp/fluid-device.log
```

Telemetry includes physics/render/IMU rates, solver phase times, maximum step
time, internal memory, resets, filtered input, and fluid centroid/mean velocity.
Motion and centroid vectors in the log are scaled by 1000.

Before production, check both side walls with a quick return to upright and an
opposing sideways shift, repeat hard shakes, and try normal backpack walking.
Confirm smooth display motion and sustained physics scheduling under those
loads. The latest color correction measured 60.03 physics Hz and 59.84 render
FPS in a 60-second, mostly stationary capture, with 145,788 bytes free internally.
The V-shaped band's disappearance and backpack walking still need physical
confirmation.
