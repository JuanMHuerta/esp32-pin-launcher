# Verification record

Date: 2026-09-25. Board: Waveshare ESP32-S3-Touch-AMOLED-1.91,
USB serial `A0:85:E3:E7:A7:F0`. ESP-IDF v5.5.1, ESP32-S3 at 240 MHz.

## Host checks

`bash tools/test.sh` passes using GCC 16.2.1 with AddressSanitizer,
UndefinedBehaviorSanitizer and leak detection. See
[`artifacts/host-tests.log`](artifacts/host-tests.log).

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

## Visual checks

`python3 tools/preview.py` generated 1,350 frames at 30 fps across nine animation
sequences. Inspected the pose contact sheet and the real-device captures for
silhouette, color, readable expressions, screen boundaries and prop placement.
Fixed the nap scarf/sprout alignment and removed a once-per-minute discontinuity
from trigonometric animation phases. Reviewed the regenerated images.

- [Host pose sheet](artifacts/contact-sheet.png)
- [Animation video](artifacts/all-animations.webm)
- [Actual ESP32 framebuffer captures](artifacts/device-contact-sheet.png)

The 134 × 60 canvas expands exactly to 536 × 240, with no interpolation. The
head is 136 physical pixels wide and the standing silhouette is about 160 pixels
tall. It uses large eyes and a contrasting scarf rather than depending on text
to convey activity. Framebuffer captures verify the image submitted to the
display; physical display appearance was checked separately by the user.

## Physical interactions

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
record is [`hardware-physical-final.log`](artifacts/hardware-physical-final.log):
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

## Final hardware run

Heap tracing identified all observed initial runtime allocations in newlib's
`_dtoa_r` / `_Balloc` number-formatting paths from `status_print()`. See
[`heap-trace-symbols.txt`](artifacts/heap-trace-symbols.txt). Replaced floating-point
diagnostic formatting with integers; simulation/rendering still use the ESP32's
hardware floating-point unit. A fresh 15-second hardware trace after this change
recorded **zero allocations and zero frees**, including startup status reporting
and a periodic performance/status report. See
[`heap-trace-fixed-verified.log`](artifacts/heap-trace-fixed-verified.log).
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
[`hardware-final.json`](artifacts/hardware-final.json).

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

## Scope of evidence

Firmware was built and flashed over USB; esptool verified written flash hashes.
Physical appearance/interactions have user confirmation, and the final automated
run validates execution on the attached hardware. No enclosure or physical
backpack fastening was manufactured. Battery endurance and sunlight visibility
were not measured. The firmware supports the board's normal battery power path
and offers three display brightness levels.
