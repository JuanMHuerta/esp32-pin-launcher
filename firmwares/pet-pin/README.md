# Miso — a little woodland friend for your backpack

![Miso on the pin](artifacts/preview.png)

Miso wanders, sniffs flowers, snacks on berries, chases fireflies, waves, and curls
up for a nap. A mint coat, peach scarf, big eyes, and a tiny sprout keep the
character readable on the 1.91-inch screen. Its woodland shifts through a
six-minute day/night cycle. No menus, network connection, account, or external
assets are needed.

**Hardware:** Waveshare ESP32-S3-Touch-AMOLED-1.91, SKU 28596, landscape 536 × 240.
The attached board has been flashed. See [verification](TESTING.md) for evidence.
The final eight-minute hardware run passed at 30 fps with no late frames and
constant free heap; host checks include 24 simulated hours.

## Play

| Action | Reaction |
| --- | --- |
| Tap Miso | Happy hops and floating hearts |
| Tap empty space | Drop a berry; Miso walks over and eats it |
| Swipe | Chase a firefly toward that side |
| Hold for 0.8 seconds | Nap; hold again or tap to wake |
| Tilt | Eyes follow and the body leans |
| Gently shake | A surprised hop and orbiting stars; six-second cooldown |
| Press and release BOOT | Cycle medium → bright → dim brightness |

Keep the board still for the first half-second after boot to establish its
neutral tilt. Miso also plays by itself. Brightness starts at medium each boot.
Nap keeps the pet visible and animated; it is an animation, not deep sleep.

## Animation previews

- [All nine animations, 30 fps](artifacts/all-animations.webm)
- [Pose contact sheet](artifacts/contact-sheet.png)
- [Idle](artifacts/idle.gif), [walk](artifacts/walk.gif), [sniff](artifacts/sniff.gif),
  [snack](artifacts/eat.gif), [nap](artifacts/sleep.gif), [hearts](artifacts/love.gif),
  [play](artifacts/play.gif), [surprise](artifacts/surprise.gif), [wave](artifacts/wave.gif)

These are rendered by the same C code used on the device, with exact RGB565
quantization and nearest-neighbor scaling. Graphics are original raster drawing
routines, so there is no image compression, decoder, or sprite allocation cost.

## Build and flash

ESP-IDF **5.5.1**, Python environment and ESP32-S3 toolchain are installed on this
machine. The SH8601 driver is pinned in `main/idf_component.yml` and
`dependencies.lock`.

```bash
. /home/juan/.local/share/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyACM0 flash
idf.py -p /dev/ttyACM0 monitor
```

Exit the monitor with Ctrl+]. Only one process should own the serial port.
The durable port name on this machine is
`/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_A0:85:E3:E7:A7:F0-if00`.
On another workstation, source its ESP-IDF `export.sh` and select its serial port.

The [firmware package](artifacts/firmware/README.md) contains the exact flashed
application, bootloader, partition table, debugging ELF, configuration and SHA-256
checksums, with instructions for flashing it without rebuilding.
The [complete release archive](artifacts/miso-pin-1.0.0.zip) includes source,
firmware, previews and verification records.

## Reproduce the tests

Host prerequisites: GCC with AddressSanitizer and UndefinedBehaviorSanitizer,
Python 3 with Pillow, and FFmpeg with the `libvpx-vp9` encoder. Serial tools use
the ESP-IDF Python environment, which includes pyserial.

```bash
bash tools/test.sh
python3 tools/preview.py
python3 tools/check_previews.py
. /home/juan/.local/share/esp-idf/export.sh
python tools/device.py --seconds 480 --exercise --log artifacts/hardware-final.log
python3 tools/check_run.py artifacts/hardware-final.log
```

`--exercise` temporarily selects each animation, captures the framebuffer,
checks interaction commands, color bars and brightness, and finishes after 65 seconds
with autonomous behavior restored at medium brightness.
Keep the serial port free for this command. The hardware run deliberately takes
eight minutes to include a complete ambient day/night cycle after the exercise.
Captures show the transmitted framebuffer; they are not photographs of the panel.

On Fedora, the host compiler needs the `libasan` and `libubsan` packages. This
workstation has extracted copies in `build/host/sanitizers/usr/lib64`; the test
script uses those if present. `TEST_LDFLAGS` can select a different local copy.

## Code and performance

| File | Responsibility |
| --- | --- |
| `main/pet.c` | Activity scheduler, physics, locomotion, behavior and interactions |
| `main/paint.c` | Pixel drawing, palette, world cycle, facial expressions, RGB565 expansion |
| `main/input.c` | Touch coordinate mapping, gesture recognition, filtered tilt and shake detection |
| `main/board.c` | Exact board pins, QSPI panel, DMA ownership, I²C sensors and interrupt task |
| `main/main.c` | 30 fps scheduling, BOOT brightness, metrics and diagnostic console |
| `tests/test_pet.c` | Host tests for behavior, input, rendering bounds and long operation |

The renderer redraws a **134 × 60** logical canvas (16,080 bytes). Two internal
DMA strips of 40 physical rows use **85,760 bytes** combined. A strip is reused
only after the panel completion callback. Every frame covers the entire panel,
including the first, preventing trails and stale pixels. The running firmware
does no per-frame heap allocation and does not initialize PSRAM, Wi-Fi or BLE.
The accelerometer runs at 125 Hz and is sampled at about 50 Hz; the unused gyro
stays disabled. Rendering continues if an input device is unavailable at boot.
Runtime logging is suppressed when no USB host is connected, avoiding the USB
console's transmit timeout during battery/power-bank operation.

To add a behavior, extend `pet_state_t`, its duration/scheduler entry, and the
corresponding drawing in `mascot()`. Distances are logical pixels and time is
milliseconds. Tests and previews require no ESP-IDF headers. Update the host
preview state list when adding a state.

Power use depends on brightness and displayed pixels. The firmware supports
the board's normal USB/battery power path; battery runtime has not been measured.

## Diagnostic console

Send newline-terminated commands over USB Serial/JTAG:

```text
status
state idle
state walk
state sniff
state eat
state sleep
state love
state play
state surprise
state wave
tap 67 30
swipe 110 30
hold
shake
brightness 0
capture
bars
auto
```

Coordinates are logical 134 × 60 pixels. `state` holds an activity for up to ten
seconds, although walking can finish upon arrival. `bars` displays red, green,
blue, white and black for panel diagnosis; `auto` restores the pet. `capture`
dumps the exact last displayed logical frame as RGB565 hex; it intentionally
pauses animation briefly, and its transfer is excluded from normal timing stats.

`PERF` reports measured fps, average/maximum frame work, deadline overruns,
frame period range and free heap every ten seconds. `STATUS` includes raw sensor
samples, contact counts, input drops, errors and visited states. Heap integrity
is checked at each report.

Status positions and tilt use thousandths (`x_milli`, `tilt_milli`), and
acceleration uses milligravity (`accel_mg`). Integer logging avoids retained
allocations inside floating-point number formatting. An optional diagnostic
build enables heap tracing and a `heap` command:

```bash
idf.py -B build-trace -D SDKCONFIG=build-trace/sdkconfig \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.trace' build
```

After a CPU/USB reset the touch controller may still be sleeping. Its wake
interrupt remains armed, and a touch restores normal reporting. `touch=0` in
status means it has not answered yet this boot; it does not disable the input.
`errors` counts failed sensor transactions. `ignored_reports` counts rejected
touch data such as multiple fingers or coordinates outside the glass; those
reports generate no gesture and do not interrupt animation.

## Hardware references

Start with the two supplied board guides in this folder. The panel uses the
board-specific SH8601 initialization with `0x36=0xF0`, RGB565 byte swapping and
the exact QSPI pins from Waveshare's working example.

- [Official board examples](https://github.com/waveshareteam/ESP32-S3-AMOLED-1.91)
- [FT3168 datasheet, §2.3](https://files.waveshare.com/wiki/common/FT3168.pdf):
  monitor/sleep mode has an I²C sharing limitation. The input task waits for its
  wake interrupt and polls only while contact is active, allowing it to coexist
  with the QMI8658 on GPIO39/40.
- [ESP-IDF 5.5.1 LCD interface](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/lcd/index.html)
