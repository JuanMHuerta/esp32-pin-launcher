# Historical app validation

These records describe earlier firmware and layouts. Their flash addresses and
menu shortcuts are historical; use the current generated partition table.

## Dungeon

### Axe correction validation — 2026-10-01

The ready axe's bent shaft and backward-facing blade were replaced in both
poses. The corrected master is `assets/masters/axe-right.png`; its consumed
palette-locked sheet is `assets/source/axe-right.png`. Per-sprite hash comparison
confirmed that only `AXE` and `AXE_SWING` changed. Artwork sources and rebuild instructions are in `firmwares/dungeon-pin/assets/ART_DIRECTION.md`.

Host tests, preview generation and the pinned ESP-IDF 5.5.1 build passed.
The installed binary remains 743,648 bytes; SHA-256:
`27b90bc5381728e84511120ac810c82402dd23914ef491a3f56cc03e77aa97d9`.
The connected ESP32-S3 revision 0.2 partition table confirmed the dungeon slot
at `0x5a0000`. The dungeon application image was written and explicitly verified
against flash, then OTA slot 5 was selected.

A 25-second serial observation confirmed boot from `0x5a0000` into
`dungeon_seed`, two 30 fps heartbeats, exploration and a completed four-hit
encounter. No panic, abort or brownout appeared. The four rendered axe timing
frames were visually reviewed in `preview-axe.png`; the physical display was
not visually reviewed.

### Connected-crawl revision validation — 2026-10-01

Host tests, preview generation and the ESP-IDF build passed. The installed
binary is 743,648 bytes (29% of the 1 MiB slot remains); SHA-256:
`a3fe411019677e781b7122503f7bc64b4d42b53d8614c95ea3dcd6694cd18e57`.
The connected ESP32-S3 revision 0.2 partition table confirmed dungeon at
`0x5a0000`; only its application image and OTA selection were written.
Explicit flash readback verification matched.

A 115-second serial observation confirmed boot from `0x5a0000`, exploration,
four- and five-hit ordinary encounters, an eight-hit boss exchange with one HP
remaining, and progression into the overgrown-ruins biome. Player HP persisted
at 69 on the second descent; staff MP decreased through repeated casts.
All eleven heartbeats reported 30 fps, with average paint times of approximately
5.9–6.7 ms. No panic, abort or brownout appeared. This is runtime evidence on
one connected board, not a physical visual review.

### Previous static-camera revision validation — 2026-10-01

Host tests and the ESP-IDF build passed. The installed binary is 727,264 bytes
(31% of the 1 MiB slot remains); SHA-256:
`92f0cab95f4d587b7a1c886aee58f0e49bf8f661dc7a9de91405bd5da1694d97`.
The connected ESP32-S3 revision 0.2 partition table confirmed dungeon at
`0x5a0000`; only its application image and OTA selection were written.
Explicit flash readback verification matched. A 35-second serial observation
confirmed successful boot, room/crypt/boss progression, three 30 fps heartbeats
and approximately 5.0–5.1 ms average paint time, without panic or brownout.
This is runtime evidence on one connected board, not a physical visual review.

## Maze

Device validation (2026-10-01, ESP-IDF 5.5.1): the 262,080-byte app was
flashed at `0x6a0000`, verified against flash, and launched with serial shortcut
`7`. On the connected pin, steady operation measured 40 FPS, about 4.6–4.8 ms
render time and 18 ms display transfer, with 6,156 bytes of task stack headroom.
Visual review used the generated stills and motion sequence.

## Wayfarer

Validated on the connected SKU 28596 on 2026-10-01 with ESP-IDF 5.5.1 and
the existing SH8601-compatible display setup. Firmware v2.0.0 is 654,464 bytes
and fits the 1 MiB app slot. Flash readback matched the built image.
The final device run held 30.0 fps for over four minutes, including all six
random event types; every reported maximum frame-render time stayed below
30 ms. The host checks above passed, and native-resolution event previews
were reviewed for artwork, palette, instrument placement, and motion.

## Miso

### Original verification record

Date: 2026-09-25. Board: Waveshare ESP32-S3-Touch-AMOLED-1.91,
ESP-IDF v5.5.1, ESP32-S3 at 240 MHz.

### Host checks

`bash tools/test.sh` passes using GCC 16.2.1 with AddressSanitizer,
UndefinedBehaviorSanitizer and leak detection. See
`artifacts/host-tests.log` (local validation artifact).

- 24 simulated hours, 2,592,000 updates, 17,287 autonomous activity transitions.
  All seven autonomous states visited; bounded finite positions and jump heights.
- Tap affection, feeding and arrival, hold/nap/wake, swipe play, shake cooldown,
  bad state rejection and non-finite tilt handling.
- Full-width feeding trips in both directions, starting with chase velocity in
  the opposite direction. Increased the food-approach timeout to include travel,
  reversal and easing into arrival.
- All four raw touch corners, out-of-range reports, quick contact rejection,
  one event per hold, no tap after hold, swipe classification and 32-bit time wrap.
- Stationary and slow tilt inputs do not trigger shake; sustained movement does;
  cooldown and invalid acceleration handling verified.
- 2,160 rendered animation frames spanning the world cycle with buffer guards.
- Every expanded physical pixel of every DMA strip checked for correct scale,
  position and byte order; invalid strip bounds rejected.
- Compiler warnings treated as errors in host tests. Firmware build has no
  application compiler warnings or errors.

### Visual checks

`python3 tools/preview.py` generated 1,350 frames at 30 fps across nine animation
sequences. Inspected the pose contact sheet and the real-device captures for
silhouette, color, readable expressions, screen boundaries and prop placement.
Fixed the nap scarf/sprout alignment and removed a once-per-minute discontinuity
from trigonometric animation phases. Reviewed the regenerated images.

- Host pose sheet (local validation artifact)
- Animation video (local validation artifact)
- Actual ESP32 framebuffer captures (local validation artifact)

The 134 × 60 canvas expands exactly to 536 × 240, with no interpolation. The
head is 136 physical pixels wide and the standing silhouette is about 160 pixels
tall. It uses large eyes and a contrasting scarf rather than depending on text
to convey activity. Framebuffer captures verify the image submitted to the
display; physical display appearance was checked separately by the user.

### Physical interactions

The user checked the attached screen and tried tapping Miso, tapping empty space,
swiping, holding, tilting and gently shaking the board, then reported:

> Looks right; all reactions work

The accompanying serial evidence is in `artifacts/hardware-v2.log`: real touch
IRQs and presses/releases, tap events at 32.9 and 39.1 seconds, swipe events at
60.9 and 62.2 seconds, hold at 65.3 seconds, shake at 68.8 seconds, and changing
accelerometer/tilt samples. All nine states had been visited by 80 seconds.

That earlier run exposed FT3168 idle-mode NACKs on the shared I²C bus. The final
firmware retains the proven interrupt/contact path and removes idle reads,
consistent with the FT3168 datasheet §2.3. It also accepts brief valid contacts
and establishes a fresh neutral motion pose during startup. Earlier hardware
logs are retained as development evidence; they are not the final soak result.

After the reset/wake fix, the user repeated idle touch and the physical BOOT
brightness test and confirmed **“Touch and brightness both work.”** The serial
record is `hardware-physical-final.log` (local validation artifact):
13 real contacts, 15 input events including movement, and repeated physical
brightness changes. Heap stayed at 261,088 bytes throughout that session.
Two unsupported/out-of-range touch reports were rejected, with no I²C errors;
the final diagnostic format reports those separately as `ignored_reports`.

The final release run also exercised the specific warm-reset case: startup
reported `FT3168 awaiting touch wake interrupt`, followed by real contacts at
273.2 and 279.0 seconds. Both produced tap events and the `love` reaction;
`touch` changed from 0 to 1 with no sensor errors, dropped events or heap change.
The user separately confirmed that Miso reacted. This evidence is in
`artifacts/hardware-final.log`.

### Final hardware run

Heap tracing identified all observed initial runtime allocations in newlib's
`_dtoa_r` / `_Balloc` number-formatting paths from `status_print()`. See
`heap-trace-symbols.txt` (local validation artifact). Replaced floating-point
diagnostic formatting with integers; simulation/rendering still use the ESP32's
hardware floating-point unit. A fresh 15-second hardware trace after this change
recorded **zero allocations and zero frees**, including startup status reporting
and a periodic performance/status report. See
`heap-trace-fixed-verified.log` (local validation artifact).
Also fixed warm-reset touch detection: a controller
still in monitor mode can NACK its startup probe, so the firmware always keeps
the touch wake interrupt armed instead of disabling touch for that boot.

The completed eight-minute exercise/soak is recorded in
`artifacts/hardware-final.log`. It verifies each animation, captures each on the
ESP32, exercises diagnostic interactions, color bars and brightness, then leaves
the pet running autonomously. The run checks timing, stable heap, sensor errors,
reset count, visited states and firmware ELF identity.

**PASS:** `python3 tools/check_run.py artifacts/hardware-final.log` passed all
gates. The machine-readable result is
`hardware-final.json` (local validation artifact).

| Measurement | Result |
| --- | --- |
| Performance windows | 44 windows of 300 frames |
| Measured frame rate | 30.001–30.041 fps |
| Frame period | 32.000–34.051 ms |
| Maximum frame work | 21.000 ms, within the 33.333 ms budget |
| Late frames | 0 |
| Steady free heap | 261,088 bytes in every performance window |
| Sensor errors / dropped inputs | 0 / 0 |
| Accelerometer samples | 23,930 |
| Visited animation states | All 9 |
| Captures | 9 animation frames plus verified RGB565 color bars |
| Unexpected resets | 0 |
| Application binary | 330,144 bytes |

The running firmware's boot identity matches the built and packaged ELF:
`07bee2500f9e0ebb295d40006d73ebd3e0931f402a3f50869dff57d32fffecf2`.
All packaged firmware SHA-256 checksums pass. The serial monitor is closed;
Miso remains in autonomous mode at medium brightness. The saved final device
contact sheet was regenerated from this run and visually inspected.

### Scope of evidence

Firmware was built and flashed over USB; esptool verified written flash hashes.
Physical appearance/interactions have user confirmation, and the final automated
run validates execution on the attached hardware. No enclosure or physical
backpack fastening was manufactured. Battery endurance and sunlight visibility
were not measured. The firmware supports the board's normal battery power path
and offers three display brightness levels.

## Lumen

Source-project measurement, recorded before this repository cleanup:

The firmware targets **33 fps** (30 ms frames). On the attached ESP32-S3, the
final firmware sustained 33 fps across three 100-frame windows, with about
**23–25 ms** of drawing and transfer work per frame, **4 ms** reported strip-render
time, and about **194 KiB** free heap. Every logged frame obtained an IMU sample. Timing varies
with visible geometry and light-wave effects. These are USB runtime measurements;
battery runtime has not been measured.
