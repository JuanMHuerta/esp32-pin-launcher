# ESP32-S3 Touch AMOLED 1.91: project lessons

This is a practical handoff for the Waveshare **ESP32-S3-Touch-AMOLED-1.91**, SKU
28596, based on bringing up Conway's Life and Miso pixel-pet firmware on an
attached board. It records corrections and constraints that cost time in these
projects. It is not a replacement for the board guide or the official demo package.

## Corrections to make before writing new firmware

### Do not choose the display driver from the product-page controller name

The product material refers to an **RM67162**, while the board-specific ESP-IDF
demo initializes the panel through **`esp_lcd_sh8601`**. The working firmware
uses `espressif/esp_lcd_sh8601` 2.0.1~1 and the Waveshare SH8601 initialization
sequence. Treat that official ESP-IDF demo as the first source of truth for this
board revision. A generic RM67162 setup is not a safe starting point just because
of the product-page label.

The controller-name and colour-depth documentation conflict is described, with
sources, in [the board guide](Waveshare-ESP32-S3-Touch-AMOLED-1.91-SKU28596-ESP-IDF-Guide.md).

### The display is used as a 536 x 240 landscape panel

Its native specification is often written as 240 x 536. The working ESP-IDF path
uses a **536 x 240** framebuffer/window and the matching display-orientation
command from the Waveshare example. Do not allocate or flush a 240 x 536 buffer
without deliberately changing the orientation configuration as well.

### Copy the QSPI pin map and panel bring-up sequence exactly at first

The working mapping is:

| Signal | GPIO |
| --- | ---: |
| CS | 6 |
| CLK/PCLK | 47 |
| D0 | 18 |
| D1 | 7 |
| D2 | 48 |
| D3 | 5 |
| Reset | 17 |

It uses `SPI2_HOST`, RGB565 (`0x3A = 0x55`), and the reset/sleep-out/orientation/
brightness sequence in [`main/board.c`](main/board.c). A blank or oddly oriented
screen should first be investigated as an init-command, driver, or pin-map issue
rather than a rendering-algorithm issue.

### RGB565 bytes must be put on the QSPI wire in the expected order

The draw buffer is made from `uint16_t` RGB565 values, but the panel I/O sends
memory bytes in address order. The working code uses:

```c
uint16_t wire_color = __builtin_bswap16(rgb565_color);
```

immediately before filling the DMA buffer. Keep this conversion, or validate the
colour-byte order afresh if changing panel I/O configuration. It is easy to make
an otherwise working renderer display wrong colours by assuming the CPU's
little-endian word layout is automatically what the panel expects.

## Toolchain and flashing lessons

### Use an ESP-IDF 5.5.x project configuration

This project builds with ESP-IDF 5.5.1 and declares `idf >=5.5,<6.0`. It targets
`esp32s3`, runs at 240 MHz, and is configured for the board's **16 MB flash**.
The board has 8 MB PSRAM, but this application neither initializes nor needs it.
Do not assume PSRAM is necessary simply because this is an S3R8 board.

### USB serial permission is a host problem that returns after re-enumeration

The board appeared as `/dev/ttyACM0` through the ESP32-S3 USB Serial/JTAG
interface. A user that cannot open that device cannot flash or monitor it even
when the firmware and cable are fine. During this session a temporary ACL fixed
it:

```sh
sudo setfacl -m u:juan:rw /dev/ttyACM0
```

That ACL was lost after the board reconnected, because `/dev/ttyACM0` is a new
device node after USB re-enumeration. For regular development, prefer a durable
host setup such as membership in the distribution's serial-device group (commonly
`dialout`) or a narrowly scoped udev rule. Re-check the port name and permissions
after any reset/replug before debugging the firmware.

On this development machine, a udev rule matching the board's USB vendor,
product, and serial number now makes `juan` the owner of its tty node. That
survives re-enumeration without granting access to other serial devices.

## Touch input lessons

### Capture GPIO41's interrupt instead of sampling its level slowly

The FT3168 uses I2C address `0x38` on SDA GPIO40 and SCL GPIO39. Its INT line
is GPIO41. A 20 ms task that read only while INT was low missed short pulses;
an interrupt handler now wakes a task that reads the touch report over I2C.
Repeated contact reports should be treated as one press, or dragging a finger
can create many unintended seeds.

### Check the coordinate transform against this exact board's demo

The controller's first coordinate pair is the display's Y (0–239), and its
second pair is X (0–535). With the working landscape `0x36 = 0xF0` panel
orientation, the screen position is `x = raw_x`, `y = 239 - raw_y`. An earlier
assumption also reversed X, placing seeds on the opposite long side of the
screen. The [Waveshare touch driver](https://github.com/waveshareteam/ESP32-S3-AMOLED-1.91/blob/main/02_Example/ESP-IDF/03_LVGL_V8_Test/LVGL_Test/components/esp_touch/touch_bsp.c)
uses this same transform. Test all four corners before adding application input.

## Rendering constraints and decisions that were *not* hardware faults

### Conway's Life intentionally avoided a full display framebuffer

The app uses an 80-row RGB565 DMA strip instead of a 536 x 240 framebuffer.
That needs about 84 KiB rather than about 251 KiB for a full framebuffer, leaving
room in internal RAM for the two Life state arrays and program data. This was a
memory/performance design choice, not evidence that the display cannot accept a
full framebuffer. A different app may make a different trade-off, especially if
it deliberately uses PSRAM and accepts its performance characteristics.

### Initial content must force one complete display update

Later frames may skip unchanged 80-row strips. The first frame explicitly forces
a full refresh, so all pixels receive a known value. Preserve that rule whenever
adding dirty-region rendering; otherwise pixels not covered by an initial dirty
region can retain the panel's previous image.

### “Ghosts” need to be separated from intended death traces

The Life state machine represents recently dead cells as states 4 through 1,
then state 0. At 10 fps those programmed trails last four generations (roughly
0.4 seconds). In the current palette they are magenta; live cells are cyan after
their one white birth frame. State 0 maps to RGB565 value zero, and a changed
strip is sent when a cell reaches it.

If pixels stay visible after the corresponding logical cells have been state 0
and their strip has been transmitted, that would be a panel persistence or
hardware/rendering issue to test—not an expected Life trail. A useful diagnostic
firmware mode would fill the whole screen solid black, then alternate black and
white rectangles while logging the exact flushes.

## Minimal proven starting point

For a fresh project, start by copying only the following from this repository:

1. `main/idf_component.yml` for the SH8601 dependency and IDF range.
2. The display pin constants, `panel_init`, and `display_open()` from
   [`main/board.c`](main/board.c).
3. A small RGB565 full-screen or strip test that writes black, red, green, blue,
   and white before bringing in an application framework.

Then add touch, IMU, SD, Wi-Fi, or PSRAM one at a time. The board guide includes
their verified pin/address information and the known documentation conflicts.

## Further lessons from the Miso pin build (2026-09-25)

### Treat a sleeping FT3168 as a wake-up case, not an absent device

The FT3168 shares GPIO39/40 I²C with the QMI8658. Its datasheet §2.3 warns that
the controller may not respond to I²C while in monitor/sleep mode on a shared bus.
Polling touch while idle produced NACKs after IMU transactions. A CPU or USB
reset can leave the touch controller asleep, so even the startup probe at `0x38`
may NACK while the panel and IMU work normally. On this board, always install the
GPIO41 falling-edge interrupt handler. Read touch on its interrupt and continue
reading only while a contact is active to catch hold and release. A successful
read after the interrupt can mark touch ready for that boot. Avoid declaring
touch permanently missing solely because its boot probe failed.

This was observed in the [final hardware log](artifacts/hardware-final.log):
`FT3168 awaiting touch wake interrupt` at startup, then real taps at 273.2 and
279.0 seconds woke it and triggered Miso. The final eight-minute run had zero
sensor transaction errors. The [FT3168 datasheet, §2.3](https://files.waveshare.com/wiki/common/FT3168.pdf)
explains the shared-bus limitation.

### Validate short taps and reject malformed reports separately

Real contacts in this session lasted as little as 9 ms; a 25 ms minimum tap
duration lost valid taps. Miso uses a 5 ms minimum and classifies the gesture on
release. Test the chosen threshold on the physical glass and debounce repeated
reports as one contact. Reject unsupported multi-touch and out-of-range reports
before producing an input event. Count those reports separately from failed I²C
transactions, so the error counter still indicates a bus or sensor fault. For a
scaled canvas, apply the already verified landscape transform first, then divide
both screen coordinates by the scale factor; Miso uses 4× scaling in
[`main/input.c`](main/input.c).

### Calibrate motion from fresh samples after configuring the QMI8658

The QMI8658 responded at `0x6B` with ID `5` on this board. Miso enables only
its accelerometer at 8 g / 125 Hz and samples it at about 50 Hz. Initial readings
can reflect the previous sensor configuration or a changing pose. Its first ten
samples establish the neutral tilt before gesture detection. A filtered gravity
estimate drives tilt; shake requires sustained motion for at least 40 ms plus a
cooldown. This prevented stationary noise and slow tilts from acting like shakes
in host tests; the user also confirmed tilt and gentle-shake reactions on the
board. See [`main/board.c`](main/board.c) and [`main/input.c`](main/input.c).

### Keep DMA buffers owned until the panel reports completion

Miso draws at 134 × 60 and expands each logical pixel 4× to the 536 × 240 panel.
It uses two internal DMA buffers of 40 physical rows each. A buffer must not be
rewritten while its QSPI transfer is pending: wait for the panel I/O completion
callback before reuse. Render the first complete frame so no old pixels remain.
On this board a complete 30 fps redraw did not require PSRAM: the
[eight-minute hardware run](artifacts/hardware-final.json) measured
30.001–30.041 fps, at most 21 ms of frame work, zero late frames and constant
261,088-byte free heap. Those are measurements for this renderer and board, not
a general throughput guarantee for other applications.

### Measure memory after serial logging and with no host attached

Short runs showed small permanent heap drops even though rendering itself
allocated nothing per frame. ESP-IDF heap tracing identified retained newlib
`_dtoa_r` / `_Balloc` allocations from floating-point diagnostic formatting.
Integer-only runtime logs removed them; a fresh 15-second hardware trace had
zero allocations/frees, and the final eight-minute run held the same free-heap
value in every performance window. See the [trace result](artifacts/heap-trace-fixed-verified.log)
and [hardware report](artifacts/hardware-final.json). Trace allocations instead
of assuming that every heap change comes from graphics or an I²C driver.

Opening USB Serial/JTAG reset this attached board on this host. A monitor should
record the resulting boot rather than assume that opening the port is passive.
Also, ESP-IDF's USB console can wait for a disconnected host when logging;
Miso's runtime log hook checks `usb_serial_jtag_is_connected()` to skip output
without a host. This avoids a console wait inside the 33 ms frame budget when
running from a power bank. Battery endurance was not measured.

### Check panel brightness with a real button press

For this QSPI panel, `esp_lcd_panel_io_tx_param()` with command prefix
`(0x02 << 24) | (0x51 << 8)` writes the SH8601 brightness value. Miso assigns
BOOT (GPIO0) a brightness cycle on release after a 40 ms press; the user
confirmed the three levels on the actual panel. Diagnostic framebuffer captures
and RGB565 color bars can verify pixel data and byte order, while the physical
screen check establishes that the panel actually displays the intended colors.
