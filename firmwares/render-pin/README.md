# Lumen

A procedural 3D starfield for the Waveshare ESP32-S3-Touch-AMOLED-1.91
(SKU 28596). Small constellations glide toward you through layers of bright white
stars. A few points have yellow or red accents. Thin luminous connections,
traveling glints and halos are drawn at 536 × 240 from geometry.

![Constellation flythrough](preview.gif)

[Simulated tilt, rotation, and a light wave](preview-motion.gif) ·
[Three palettes](preview-palettes.png) · [Still frame](preview.png)

## Controls

- **Tilt:** changes the camera perspective and introduces depth parallax.
- **Rotate:** the gyro axes are mapped to the horizontal display: sensor X
  steers the horizontal view and sensor Y steers the vertical view. Rotation
  around the screen's normal banks the sky. Movement does not change brightness.
- **Tap:** sends a light wave from the touch point and cycles Ice, Warm White,
  and Silver: one star color and one connection accent at a time.

The startup pose sets the neutral accelerometer reference. Gyro pitch/yaw
respond immediately, then ease back toward the gravity-based view over roughly
two seconds. Tilt continues to respond promptly. Roll is integrated with a
small deadband; the camera does not track absolute orientation. If the IMU is absent,
procedural flight and gentle camera drift continue.

## Build

From the repository root, after sourcing ESP-IDF 5.5.x:

```sh
idf.py -C firmwares/render-pin build
./tools/test.sh --app render-pin
python3 firmwares/render-pin/tools/render_previews.py
```

Use the root [build script](../../build-and-flash.sh) to install the collection.
Display and touch rules are in the shared [board contract](../../documentation/AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.md).

## Rendering and performance

- 640 independent stars, including 390 distant points distributed around a sphere
  so large camera movements keep the sky populated.
- 12 procedurally varied constellations, each with 6 stars and 5 connections.
- Approximately 96° horizontal field of view, with motion steering up to 64°
  horizontally and 52° vertically. Subpixel star placement and antialiased connections.
- Precomputed star masks enlarge the stars without per-pixel blur calculations.
- Near/far fades hide depth recycling; different depths produce different speeds.
- Stars and constellations advance continuously through depth.
- Geometry is projected once per frame. Rendering clips to 60-row DMA strips.
- Two internal DMA buffers overlap drawing with QSPI transfers. No PSRAM or
  full-screen framebuffer is required; one-buffer fallback is supported.

The firmware targets 33 fps (30 ms frames). Historical device measurements
are in [app validation](../../documentation/APP_VALIDATION.md).

## Previews and checks

`tools/render_previews.py` compiles the actual C renderer and generates the
previews above with Pillow and FFmpeg. GIFs run at 15 fps; the board targets 33 fps.

Host tests check full-frame/strip equivalence, buffer bounds, responses to controls,
continuous star travel and constellation visibility. Motion tests cover tilt,
each gyro axis, pitch/yaw recovery and rejection of tiny stationary rates.
