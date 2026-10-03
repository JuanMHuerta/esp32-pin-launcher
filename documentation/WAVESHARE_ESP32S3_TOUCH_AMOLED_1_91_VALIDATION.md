# Validation ledger: Waveshare ESP32-S3-Touch-AMOLED-1.91 (SKU 28596)

Validated: 2026-10-01. Purpose: source lookup/conflict resolution. Normal agents should load the smaller `AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.md` only.

## Authority order

1. Current Waveshare schematic + current board repository.
2. Waveshare FAQ/product/docs.
3. Espressif ESP-IDF docs + Component Registry.
4. Device datasheets.
5. Project-local hardware observations.

## Verified source set

| Topic | Result | Source |
|---|---|---|
| SKU | 28596 = touch, no header | https://docs.waveshare.com/ESP32-S3-AMOLED-1.91 |
| MCU/memory/display/touch/IMU | ESP32-S3R8; 16 MB flash; 8 MB PSRAM; 240x536; RM67162; FT3168; QMI8658 | https://www.waveshare.com/esp32-s3-amoled-1.91.htm |
| RM67162 vs SH8601 | RM67162 is physical IC; SH8601 QSPI library is compatible with its init/data path | https://docs.waveshare.com/ESP32-S3-AMOLED-1.91/FAQ |
| Official board resources | repo, schematic, datasheets | https://docs.waveshare.com/ESP32-S3-AMOLED-1.91/Resources-And-Documents |
| Current board repo | official Waveshare examples, changelog 2026-03-10 | https://github.com/waveshareteam/ESP32-S3-AMOLED-1.91 |
| QSPI pins/orientation/RGB565 | SPI2; CS6 CLK47 D0=18 D1=7 D2=48 D3=5 RST17; 536x240; `0x36=F0`; `0x3A=55` | https://raw.githubusercontent.com/waveshareteam/ESP32-S3-AMOLED-1.91/main/02_Example/ESP-IDF/03_LVGL_V8_Test/LVGL_Test/main/example_qspi_with_ram.c |
| Current sample touch switch | `EXAMPLE_USE_TOUCH 0` by default | same source as above |
| Rectangle alignment | even starts/odd inclusive ends in Waveshare LVGL sample; SH8601 component requires even transfer boundaries | same source; https://components.espressif.com/components/espressif/esp_lcd_sh8601/versions/2.0.1~1/readme?language=en |
| Touch address/axis order | FT3168 `0x38`; first pair -> Y, second -> X; Y inverted | https://raw.githubusercontent.com/waveshareteam/ESP32-S3-AMOLED-1.91/main/02_Example/ESP-IDF/03_LVGL_V8_Test/LVGL_Test/components/esp_touch/touch_bsp.c |
| I2C pins/speed in sample | I2C0; SCL39 SDA40; 300 kHz | https://raw.githubusercontent.com/waveshareteam/ESP32-S3-AMOLED-1.91/main/02_Example/ESP-IDF/03_LVGL_V8_Test/LVGL_Test/components/i2c_bsp/i2c_bsp.c |
| Sample IDF dependency | `idf >5.0.4, !=5.1.1`; LVGL v8; SH8601 component | https://raw.githubusercontent.com/waveshareteam/ESP32-S3-AMOLED-1.91/main/02_Example/ESP-IDF/03_LVGL_V8_Test/LVGL_Test/main/idf_component.yml |
| SH8601 component | latest checked `espressif/esp_lcd_sh8601 2.0.1~1`; IDF >=5.3 | https://components.espressif.com/components/espressif/esp_lcd_sh8601/versions/2.0.1~1/readme?language=en |
| IDF 6 compatibility | SH8601 v2.0.0 added ESP-IDF 6.0 compatibility | https://components.espressif.com/components/espressif/esp_lcd_sh8601/versions/2.0.0/changelog?language=en |
| Schematic nets | BAT_ADC GPIO1; touch 39/40/41; SD 8/9/42/47; USB 19/20; W25Q128; QMI8658C | https://files.waveshare.com/wiki/ESP32-S3-AMOLED-1.91/ESP32-S3-AMOLED-1.91.pdf |
| ADC mapping | ESP32-S3 GPIO1 = ADC1_CH0 | https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/adc/index.html |
| USB/ROM loader | D-=19, D+=20; BOOT+RESET download sequence | https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/establish-serial-connection.html |
| Monitor reset | IDF Monitor resets on connect; `--no-reset` prevents it | https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-monitor.html |
| FT3168 sleep/shared-bus behavior | Monitor/Sleep can disrupt I2C after another slave; touch wake restores active state/I2C state | https://files.waveshare.com/wiki/common/DATA_SHEET_FT3168.pdf |
| QMI8658C addresses | address selected by SA0; 0x6A/0x6B valid | https://files.waveshare.com/wiki/common/QMI8658C.pdf |

## Resolved conflicts / corrections to source handoffs

### Display controller
- **Do not choose one label.** Hardware: RM67162. Software path: SH8601-compatible QSPI driver. Waveshare FAQ explicitly states compatibility.

### Color depth
- Product advertises 16.7M colors; Waveshare docs also contain 65K wording.
- Canonical ESP-IDF path here is RGB565 because the current sample sends `0x3A=0x55` and configures 16 bpp.
- Do not infer that RGB565 is the panel's only physical mode.

### Resolution/orientation
- Physical/native spec: 240x536.
- Canonical sample/project logical orientation: 536x240 with `0x36=0xF0`.

### RGB565 byte order
- Project-local custom renderer required `bswap16` before filling its DMA buffer.
- Current Waveshare LVGL sample passes configured LVGL color data directly.
- Therefore byte swap is **pipeline-dependent**, not a board invariant. Validate with color bars and perform exactly one conversion if required.

### Touch edge transform
- Both project and current Waveshare code agree on pair order and Y inversion.
- Project used `239-raw_y` to maintain `0..239`; Waveshare sample uses `240-y` with its own clamp/edge convention.
- Agent rule: choose one convention, clamp to valid application pixels, verify all four corners; never combine both.

### Touch startup NACK
- Project observed FT3168 NACKs after shared-bus IMU traffic while idle.
- FT3168 datasheet explicitly documents the Monitor/Sleep multi-slave I2C limitation and touch wake behavior.
- Treat single probe failure as non-conclusive when GPIO41 wake is available.

### ESP-IDF version
- Project-local known-good baseline: 5.5.x.
- Current Waveshare sample manifest accepts `>5.0.4, !=5.1.1`.
- Current Espressif SH8601 2.0.x supports IDF >=5.3 and added IDF 6.0 compatibility.
- Reproduce on 5.5.x; migrate to 6.x separately.

### microSD
- Old handoff described a revision-controlled path that can be misread as `CLK=GPIO9`.
- Current schematic's physical SPI nets are unambiguous: MISO8, CS9, MOSI42, CLK47.
- Since GPIO47 is also LCD PCLK and old examples contain revision switches, SD configuration is **revision/example-dependent**. Never promote an old branch mapping to a universal board fact.

## Empirical facts retained only as local guidance

These came from the supplied project lessons and are not universal performance guarantees:
- IDF 5.5.1 + `espressif/esp_lcd_sh8601 2.0.1~1` worked on the attached SKU 28596.
- QMI8658C answered at `0x6B` with WHO_AM_I `0x05` on that board.
- IRQ-driven touch avoided missed short contacts seen with slow INT-level polling.
- Strip/double-buffer rendering worked without PSRAM; full framebuffer is therefore not required.
- Reusing a DMA buffer before transfer completion is invalid regardless of measured frame rate.
- Runtime heap changes should be traced before attributing them to graphics/sensors.
- Attached-board observation (SKU 28596, ESP32-S3 revision v0.2, 2026-10-01):
  the current Waveshare `VersionControl_V2` SDMMC 1-bit mapping (CLK GPIO9,
  CMD GPIO42, D0 GPIO8) mounted a 31,116,288-sector card and coexisted with
  the project's display initialization. This is an observation for that board
  revision, not a universal replacement for the schematic's revision-dependent
  SD guidance.

Do not copy project-specific frame rates, tap-duration thresholds, heap counts, usernames, tty paths, or motion thresholds into general firmware defaults.
