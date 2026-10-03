# Agent contract: Waveshare ESP32-S3-Touch-AMOLED-1.91 (SKU 28596)

Validated: 2026-10-01. Scope: **SKU 28596**, ESP-IDF. Load `WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91_VALIDATION.md` only when disputing a fact, changing versions, or touching SD/controller details.

## 1. Non-negotiable rules

- Target: `esp32s3`; MCU `ESP32-S3R8`; 16 MB flash; 8 MB PSRAM. PSRAM is optional, not a display requirement.
- Display hardware IC is **RM67162**. Use the **SH8601-compatible QSPI path** used by Waveshare; do not replace a working `esp_lcd_sh8601` setup because the IC label differs.
- Canonical working orientation: **536x240 landscape**, RGB565. Native/spec orientation is 240x536.
- Start from Waveshare's exact QSPI pins/init sequence. Change one hardware variable at a time.
- Treat display transfer buffers as owned by DMA until the panel-I/O completion callback.
- Touch is IRQ-driven. A boot/idle I2C NACK from FT3168 is not sufficient proof that touch is absent.
- Do not assume old SD mappings. Check the current board/repo revision before enabling SD.
- Do not upgrade ESP-IDF/component major versions while debugging hardware.

## 2. Canonical hardware map

| Function | Value |
|---|---|
| LCD host | `SPI2_HOST` |
| LCD CS | GPIO6 |
| LCD CLK/PCLK | GPIO47 |
| LCD D0/D1/D2/D3 | GPIO18 / 7 / 48 / 5 |
| LCD reset | GPIO17 |
| Logical display | `536x240` |
| Touch | FT3168, I2C `0x38` |
| I2C SCL/SDA | GPIO39 / GPIO40 |
| Touch INT | GPIO41 |
| Touch reset | tied to board reset; no independent GPIO in the standard example |
| IMU | QMI8658C; same I2C bus; probe `0x6B`, then `0x6A` |
| Battery ADC | GPIO1 = `ADC1_CH0`; 100k/100k divider => calibrated `Vbat ~= 2*Vadc` |
| BOOT | GPIO0 |
| USB native | D- GPIO19, D+ GPIO20 |
| microSD schematic nets | MISO GPIO8, CS GPIO9, MOSI GPIO42, CLK GPIO47 |

SD warning: GPIO47 is also LCD PCLK. The schematic also shares GPIO8/9 with panel-side SDO/TE nets. Do not let independent drivers claim shared pins without a revision-proven design/current Waveshare example.

## 3. Known-good display contract

Use these Waveshare init semantics before abstraction/refactoring:

```text
orientation: 0x36 <- 0xF0
pixel format: 0x3A <- 0x55        # RGB565
X window:     0x2A <- 0..535
Y window:     0x2B <- 0..239
brightness:   0x51
panel on:     0x29
```

Driver dependency for the proven project baseline:

```yaml
dependencies:
  idf: ">=5.5,<6.0"
  espressif/esp_lcd_sh8601: "2.0.1~1"
```

Policy:
- Reproduce existing firmware with ESP-IDF 5.5.x first.
- `esp_lcd_sh8601` 2.0.x supports IDF 6.0; treat IDF 6.x as a separate migration, not a bring-up change.
- Waveshare's current LVGL sample declares `idf >5.0.4, !=5.1.1`; do not infer that every version in that range is regression-tested for this project.

Rendering rules:
- Prefer internal `MALLOC_CAP_DMA` strip/double buffers; full framebuffer is optional.
- For LVGL/driver rectangles, round starts down to even and inclusive ends up to odd; the exclusive coordinates sent to `draw_bitmap` are then even.
- Never rewrite a pending DMA buffer. Mark/reuse it only after transfer-done callback.
- First rendered frame must cover the full display if later frames use dirty regions.
- Byte order: require **exactly one** RGB565 byte-order conversion. The proven custom renderer used `__builtin_bswap16`; Waveshare's LVGL path passes its configured color buffer directly. Do not cargo-cult both. Verify with black/red/green/blue/white bars.
- Blank screen triage order: pins -> reset/init commands -> SH8601-compatible driver setup -> orientation/window -> transfer completion -> renderer.

## 4. Touch contract

When using Waveshare's LVGL example, set `EXAMPLE_USE_TOUCH=1` for SKU 28596; the current sample defaults to `0`.

I2C:
```text
bus: I2C0 in Waveshare sample
SCL=39, SDA=40
sample speed=300 kHz; FT3168 supports <=400 kHz
addr=0x38
INT=41
```

Input rules:
- Install GPIO41 falling-edge ISR; wake a task and read the FT3168 report over I2C.
- Coalesce repeated reports into one contact; classify tap/drag/release at the application layer.
- FT3168 Monitor/Sleep can lose I2C responsiveness after traffic to another slave on the shared bus. Keep INT/wake handling active and retry after a touch event; do not permanently disable touch from one failed startup probe.
- Landscape report layout: first coordinate pair is panel Y; second is panel X. Invert Y.
- Proven application normalization: `x=clamp(raw_second,0,535)`, `y=239-clamp(raw_first,0,239)`. Waveshare's sample expresses the inversion with `240-y` and different edge handling. **Do not mix conventions**; test all four physical corners and require output ranges `x=0..535`, `y=0..239`.

Touch failure triage: bus/pullups -> `0x38`/wake state -> GPIO41 ISR -> report validity -> coordinate transform -> app debounce.

## 5. IMU contract

- Device: QMI8658C on GPIO39/40 I2C bus.
- Address is SA0-dependent (`0x6A`/`0x6B`); this board/project has worked at `0x6B`. Probe both rather than hard-failing.
- Expected WHO_AM_I for the proven path: `0x05`.
- Configure sensor before using samples; discard/use initial fresh samples for neutral calibration when gesture logic depends on gravity/tilt.
- Separate malformed/unsupported sensor data from I2C transaction errors.

## 6. SD contract

Canonical physical SPI nets from schematic: `MISO=8`, `CS=9`, `MOSI=42`, `CLK=47`.

Do **not** copy the old handoff's revision-dependent `VersionControl_V2` mapping as a universal pinout. Before SD work:
1. identify board/repo revision;
2. inspect the current Waveshare `SPI_SD` example for that revision;
3. account for GPIO47/display ownership;
4. validate display + SD together on hardware before merging.

## 7. Build/flash/monitor

```sh
idf.py set-target esp32s3
idf.py build
idf.py -p "$PORT" flash
idf.py -p "$PORT" monitor
```

State-sensitive logging:
```sh
idf.py -p "$PORT" monitor --no-reset
```

Operational rules:
- USB re-enumeration can change the device node and permissions; rediscover `$PORT` after reset/replug.
- Permission/open failure is a host issue until proven otherwise. Use durable serial-group/udev configuration; never hardcode a developer username.
- If flashing cannot enter ROM loader: hold BOOT, press/release RESET, then release BOOT.
- Normal IDF Monitor resets the target on connect. Use `--no-reset` when preserving runtime state matters.

## 8. Bring-up sequence

Stop at first failing stage; do not add higher layers to compensate.

1. Build/flash minimal app; verify stable boot/logging.
2. Display: exact Waveshare pins/init; full-screen solid colors.
3. Validate RGB565 byte order + 536x240 orientation + rectangle alignment.
4. I2C scan; initialize QMI8658C.
5. Touch: GPIO41 IRQ, wake behavior, four-corner test, contact debounce.
6. Battery ADC.
7. SD using revision-current example.
8. Wi-Fi/BLE/app framework.
9. Optimize buffers/dirty regions only after correctness.

## 9. Fast failure routing

| Symptom | Check first |
|---|---|
| Blank AMOLED | QSPI pins, reset/init, SH8601-compatible path, `0x36/0x3A`, window |
| Wrong colors | RGB565 configured; byte swap occurs zero-or-one time as intended; color bars |
| Corrupt/tearing strips | DMA buffer reused before completion; transfer rectangle alignment |
| 90°/mirrored output | `536x240` + `0x36=0xF0`; do not mix native 240x536 assumptions |
| Touch absent at boot | FT3168 may be Monitor/Sleep; keep INT ISR; retry after touch wake |
| Touch mirrored/off-by-one | raw pair order + Y inversion; four-corner test; one edge convention |
| IMU missing | shared bus, probe `0x6B` then `0x6A`, WHO_AM_I |
| SD mount failure | wrong revision path/pins; GPIO47 ownership; current Waveshare example |
| Flash/monitor fails | port changed/permissions; USB cable; BOOT+RESET loader sequence |
| Heap drifts | trace allocations; inspect logging/formatting before blaming display/I2C |

## 10. Agent change discipline

For every hardware-facing change:
1. state the invariant being changed;
2. cite/check `WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91_VALIDATION.md`;
3. make one hardware-variable change;
4. build with pinned toolchain;
5. run the smallest physical diagnostic;
6. record exact observed result;
7. only then integrate/refactor.

Never "fix" a discrepancy by choosing the most intuitive datasheet value. Prefer, in order: **current Waveshare schematic/current board repo -> Waveshare FAQ/product -> Espressif docs/component registry -> chip datasheet -> local empirical lesson**. Empirical results can refine behavior but must not silently redefine board-wide facts.
