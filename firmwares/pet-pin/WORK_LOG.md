# Miso pin work log

## Objective and completion gates

Create and flash a cute, legible pixel pet on the attached Waveshare
ESP32-S3-Touch-AMOLED-1.91. Verify autonomous activity, all animation states,
touch gestures and motion, sustained frame timing, bounded memory, boot and
hardware I/O. Preserve a reproducible host preview, tests, firmware build,
hardware test log and usage documentation.

## Hardware evidence

- Read both supplied board guides. Device is `/dev/ttyACM0`, serial
  `A0:85:E3:E7:A7:F0`, accessible by juan.
- ESP-IDF v5.5.1 is installed at `/home/juan/.local/share/esp-idf`.
- Adjacent conways-pin and render-pin contain previously working panel and
  touch bring-up. Reuse the exact QSPI map, RGB565 byte swap and 0xF0 landscape
  orientation. Consulted Waveshare's official current GitHub examples too.

## Design

- Miso: mint woodland cat with sprout, peach scarf, large expressive eyes.
- 134 x 60 logical pixels, crisp 4x expansion to 536 x 240. Original graphics
  expressed as small raster drawing routines; no image decoding or asset heap.
- Autonomous idle, walk, sniff flowers, snack, nap, affection, firefly chase,
  surprise and wave. Smooth motion with a six-minute ambient world cycle.
- Implemented: touch pet for affection, background to feed, swipe to play, hold
  to nap/wake; tilt gaze/balance and shake reaction with cooldown.
- Hardware-independent simulation/paint/input; ESP-IDF board layer; fixed
  internal DMA strips; target stable 30 fps; wireless disabled.

## Completed — 2026-09-25

Miso 1.0.0 is built, flashed and running autonomously at medium brightness.
The serial monitor is closed. All completion gates passed:

- User confirmed the panel appearance, animations, tap, background feed, swipe,
  hold, tilt, shake and physical BOOT brightness cycling.
- The final USB reset left FT3168 asleep; real taps at 273.2 and 279.0 seconds
  woke it and produced affection events. The user confirmed the reaction.
- Host ASan/UBSan/leak checks passed, including 24 simulated hours, all gestures,
  full-width feeding trips, 2,160 rendering frames and every expanded RGB565 pixel.
- Generated 1,350 preview frames across all nine animations; automated animation
  checks passed and host/device contact sheets were inspected.
- Final eight-minute hardware run passed `tools/check_run.py`: 30.001–30.041 fps,
  21 ms maximum frame work, no late frames, stable 261,088-byte free heap,
  no sensor errors or dropped inputs, 23,930 motion samples and all nine states.
- Runtime ELF identity matches the packaged firmware. Firmware checksums pass.

The final report is `TESTING.md`; machine-readable metrics and raw evidence are
`artifacts/hardware-final.json` and `artifacts/hardware-final.log`. Controls,
architecture, build and reproduction instructions are in `README.md`. The release
archive contains source, exact firmware, previews and the referenced evidence.

## Findings resolved during validation

- Fixed startup neutral-pose calibration and brief valid taps.
- Removed FT3168 idle polling on the shared I2C bus, following datasheet §2.3.
  Always arm the touch wake IRQ even when a sleeping controller NACKs its probe.
- Separated unsupported touch reports from actual I2C transaction errors.
- Fixed sleeping scarf/sprout alignment and trigonometric phase discontinuities.
- Extended feeding travel time to include a full-width trip and velocity reversal.
- Heap tracing identified retained newlib `_dtoa_r` / `_Balloc` allocations in
  floating-point logs. Integer logging removed them; a fresh 15-second diagnostic
  hardware trace recorded zero allocations/frees, and the final release heap
  stayed unchanged across the eight-minute run.
- Suppress runtime logs without a USB host to avoid the console transmit timeout
  during battery/power-bank use.

Earlier logs remain as development evidence; the final hardware report is the
release gate. `sdkconfig.trace` enables an optional diagnostic heap-trace build;
release tracing is disabled. Local sanitizer runtimes are cached in
`build/host/sanitizers/usr/lib64` because the system GCC lacks those libraries.

Battery endurance, sunlight visibility and manufacture of an enclosure or
backpack fastening were outside the measurements performed; see TESTING.md.
