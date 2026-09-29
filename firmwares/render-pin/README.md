# Lumen Pin — Constellation Drift

A procedural 3D starfield for the Waveshare ESP32-S3-Touch-AMOLED-1.91
(SKU 28596). Small constellations glide toward you through layers of bright white
stars. A few points have yellow or red accents. Thin luminous connections,
traveling glints, larger star cores, and restrained halos stand out against the AMOLED's black background. Everything is drawn at
536 × 240 from geometry; there are no stored animation frames.

![Constellation flythrough](preview.gif)

[Simulated tilt, rotation, and a light wave](preview-motion.gif) ·
[Three palettes](preview-palettes.png) · [Still frame](preview.png)

## Controls

- **Tilt:** changes the camera perspective and introduces depth parallax.
- **Rotate:** the gyro axes are mapped to the horizontal display: sensor X
  steers the horizontal view and sensor Y steers the vertical view. Rotation
  around the screen's normal banks the sky. A wider lens and stronger tilt
  response let you explore
  more of the field. Movement does not change brightness.
- **Tap:** sends a light wave from the touch point and cycles Ice, Warm White,
  and Silver: one star color and one connection accent at a time.

The startup pose sets the neutral accelerometer reference. Gyro pitch/yaw
respond immediately, then ease back toward the gravity-based view over roughly
two seconds. Tilt continues to respond promptly. Roll is integrated with a
small deadband. This is an expressive
virtual camera, not an absolute orientation tracker. If the IMU is absent,
procedural flight and gentle camera drift continue.

## Build and flash

With ESP-IDF 5.5.x installed:

```sh
source /path/to/esp-idf/export.sh
idf.py -p /dev/ttyACM0 build flash monitor
```

Panel setup follows the local [board guide](Waveshare-ESP32-S3-Touch-AMOLED-1.91-SKU28596-ESP-IDF-Guide.md)
and [project lessons](ESP32-S3-Touch-AMOLED-1.91-Project-Lessons.md): landscape
SH8601 QSPI initialization, byte-swapped RGB565, and interrupt-driven FT3168 touch.

## Rendering and performance

- 640 independent stars, including 390 distant points distributed around a sphere
  so large camera movements keep the sky populated.
- 12 procedurally varied constellations, each with 6 stars and 5 connections.
- Approximately 96° horizontal field of view, with motion steering up to 64°
  horizontally and 52° vertically. Subpixel star placement and antialiased connections.
- Precomputed star masks enlarge the stars without per-pixel blur calculations.
- Near/far fades hide depth recycling; different depths produce different speeds.
- Independent stars and constellations advance through depth about 16% faster
  than in the preceding version.
- Geometry is projected once per frame. Rendering clips to 60-row DMA strips.
- Two internal DMA buffers overlap drawing with QSPI transfers. No PSRAM or
  full-screen framebuffer is required; one-buffer fallback is supported.

The firmware targets **33 fps** (30 ms frames). On the attached ESP32-S3, the
final firmware sustained 33 fps across three 100-frame windows, with about
**23–25 ms** of drawing and transfer work per frame, **4 ms** reported strip-render
time, and about **194 KiB** free heap. Every logged frame obtained an IMU sample. Timing varies
with visible geometry and light-wave effects. These are USB runtime measurements;
battery runtime has not been measured.

## Previews and checks

Build the portable renderer:

```sh
gcc -std=c11 -O2 -Wall -Wextra -Werror -I main \
    tools/preview.c main/renderer.c -lm -o /tmp/pin-preview
/tmp/pin-preview 0 2200 /tmp/aurora.ppm
```

Arguments are palette (0–2), time in milliseconds, and output PPM. Optional
arguments are view X/Y, roll in 1/1024 turns, legacy motion energy (ignored), light-wave age
in milliseconds (-1 disables it), and light-wave X/Y. Regenerate the PNG/GIF
previews with `python tools/render_previews.py` (requires Pillow and ffmpeg).
GIF previews run at 15 fps for compact files; the board targets 33 fps.

```sh
gcc -std=c11 -O2 -Wall -Wextra -Werror -I main \
    tests/renderer_test.c main/renderer.c -lm -o /tmp/renderer-test
/tmp/renderer-test
gcc -std=c11 -O2 -Wall -Wextra -Werror -I main \
    tests/motion_test.c main/motion.c -lm -o /tmp/motion-test
/tmp/motion-test
```

Renderer checks cover full-frame/strip equivalence, guard boundaries, visual
responses to controls, two minutes of continuous star travel, and continued
constellation visibility. Motion checks exercise accelerometer tilt, each gyro
axis, pitch/yaw recovery, and rejection of tiny stationary rates.
