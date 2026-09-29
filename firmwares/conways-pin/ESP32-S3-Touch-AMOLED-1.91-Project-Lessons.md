# ESP32-S3 Touch AMOLED 1.91: project lessons

This is a practical handoff for the Waveshare **ESP32-S3-Touch-AMOLED-1.91**, SKU
28596, based on bringing up this Conway's Life firmware on an attached board. It
records corrections and constraints that cost time in this project. It is not a
replacement for the board guide or the official demo package.

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
brightness sequence in [`main/main.c`](main/main.c). A blank or oddly oriented
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

### A full display framebuffer was intentionally avoided

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
2. The display pin constants, `panel_init`, and `open_display()` from
   [`main/main.c`](main/main.c).
3. A small RGB565 full-screen or strip test that writes black, red, green, blue,
   and white before bringing in an application framework.

Then add touch, IMU, SD, Wi-Fi, or PSRAM one at a time. The board guide includes
their verified pin/address information and the known documentation conflicts.
