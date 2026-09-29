# Waveshare ESP32-S3-Touch-AMOLED-1.91 (SKU 28596) Developer Guide (ESP-IDF)

Last verified: **2026-03-05**

## 1. Scope and Board Identification

This guide targets **Waveshare SKU 28596**:

- **SKU 28596** = `ESP32-S3-Touch-AMOLED-1.91` = **touch-enabled, without pre-soldered header**. [S1][S4]
- Related SKUs:
  - 28872: non-touch, without header
  - 28873: non-touch, with header (`-M`)
  - 28871: touch, with header (`-M`) [S1][S4]

## 2. Hardware Summary (Verified)

- MCU: **ESP32-S3R8** (Xtensa LX7 dual-core, up to 240 MHz). [S4]
- Wireless: 2.4 GHz Wi-Fi + BLE 5 (LE). [S1][S4]
- Memory on board:
  - **16 MB Flash** (schematic shows W25Q128).
  - **8 MB PSRAM** (from product specs). [S4][S5]
- Display:
  - 1.91" AMOLED, **240 x 536**, QSPI interface. [S4]
- Touch (touch SKU only): **FT3168** over I2C. [S4]
- IMU: **QMI8658** 6-axis (accel + gyro). [S4][S5]
- Storage: microSD/TF slot. [S4][S5]
- Power:
  - USB-C power/programming/debug
  - 3.7 V Li-ion connector (MX1.25), onboard charging circuit. [S4][S5]

## 3. Critical Documentation Inconsistencies (Important)

Waveshare materials are not fully consistent; use this policy:

1. **Display controller name conflict**
   - Product/spec page says **RM67162**. [S4]
   - Official ESP-IDF demo package for this board uses **`esp_lcd_sh8601`** and SH8601 init path in `FactoryProgram` / `LVGL` examples. [S6]
   - Arduino package includes `rm67162.*` files and pin configs for this board family. [S2]

   Practical guidance:
   - Start with the official ESP-IDF demo as ground truth for your board revision (SH8601 path in package S6).
   - If porting/rewriting drivers, keep a fallback path for RM67162-compatible init commands.

2. **Color depth conflict**
   - Docs landing page mentions 65K color. [S1]
   - Product page table mentions 16.7M color. [S4]
   - ESP-IDF demo initializes panel in **RGB565 (16-bit)** mode (`0x3A = 0x55`). [S6]

   Practical guidance:
   - For ESP-IDF/LVGL, default to RGB565 unless you explicitly validate 18-bit mode on your panel revision.

## 4. Verified GPIO / Bus Map for ESP-IDF

These pins are extracted from official Waveshare ESP-IDF and Arduino demo packages, then checked against schematic net names.

### 4.1 Display QSPI (factory/LVGL examples)

- `LCD_CS` -> GPIO6
- `LCD_PCLK` -> GPIO47
- `LCD_D0` -> GPIO18
- `LCD_D1` -> GPIO7
- `LCD_D2` -> GPIO48
- `LCD_D3` -> GPIO5
- `LCD_RST` -> GPIO17
- Resolution used by examples: 536 x 240 (landscape) [S6]

### 4.2 Touch (FT3168)

- `I2C_SCL` -> GPIO39
- `I2C_SDA` -> GPIO40
- `TP_INT` -> GPIO41
- `TP_RST` -> not GPIO-controlled in examples (`-1`, hardware-tied) [S2][S6]
- Touch I2C address used: **0x38** [S6]

### 4.3 IMU (QMI8658)

- Same I2C bus as touch in examples (GPIO39/40). [S6]
- Driver headers define 0x6A and 0x6B; low-level read/write path in package uses 0x6B. [S6]

### 4.4 Battery ADC

- Schematic net `BAT_ADC` is connected to GPIO1. [S5]
- ESP-IDF ADC example reads `ADC1_CHANNEL_0`; on ESP32-S3 this maps to GPIO1. [S6][S10]
- Example computes battery voltage as ADC voltage * 2 (divider compensation). [S6]

### 4.5 microSD / TF

ESP-IDF `SPI_SD` component provides two mappings:

- Default `VersionControl_V2` path (SDMMC 1-bit style in code):
  - CLK = GPIO9
  - CMD = GPIO42
  - D0 = GPIO8
- Fallback SPI path in same file:
  - MISO = GPIO8
  - MOSI = GPIO42
  - CLK = GPIO47
  - CS = GPIO9 [S6]

Because SD and display-related nets share several GPIOs in this board family, validate your selected SD mode with your exact firmware/hardware revision before production.

### 4.6 Buttons / USB

- Boot strap button path uses GPIO0 (`BOOT`). [S5]
- USB D+/D- are wired to ESP32-S3 native USB pins (GPIO20/GPIO19 in ESP32-S3 docs context). [S5][S9]

## 5. ESP-IDF Version and Toolchain

- Waveshare recommends **ESP-IDF v5.5.0 or higher** for this board. [S3]
- Official demo package includes ESP-IDF projects and `idf_component.yml` entries requiring IDF >5.0.4 and pulling `esp_lcd_sh8601` + LVGL v8 dependency chain. [S6]

Recommendation:
- Use **ESP-IDF 5.5.x** for new work unless your product has a pinned baseline.

## 6. Obtain Official Board Package

Primary official sources:

- Waveshare Resources page (schematic + demo links). [S2]
- ESP32-S3-AMOLED-1.91 Demo package (Google Drive link from Waveshare). [S2][S6]

Note: the current Waveshare ESP-IDF page includes a wrong GitHub example URL (points to an ESP32-C6 board). Use the board-specific package from Resources instead. [S3]

## 7. Project Layout Inside Official ESP-IDF Package

`ESP-IDF/` contains these board examples (names as shipped): [S6]

- `ADC`
- `I2C_QMI8658`
- `LVGL/LVGL_Test`
- `LVGL/LVGL_Test_90`
- `SPI_SD`
- `WIFI_STA`
- `WIFI_AP`
- `FactoryProgram`

Touch defaults:
- `FactoryProgram` is configured for touch by default (`EXAMPLE_USE_TOUCH 1`). [S6]

## 8. Build / Flash / Monitor Workflow (ESP-IDF)

### 8.1 Environment

Use standard ESP-IDF setup and environment export (`export.sh` on Linux/macOS). [S7][S8]

### 8.2 Build and flash one example

Example: `FactoryProgram`

```bash
cd <unzipped>/ESP32-S3-AMOLED-1.91-Demo/ESP-IDF/FactoryProgram
idf.py set-target esp32s3
idf.py -p <PORT> build flash monitor
```

References for this command pattern come from ESP-IDF docs and Waveshare examples. [S7][S6]

If serial flashing does not start:
- Enter ROM download mode using BOOT + reset sequencing per ESP32-S3/ESP-IDF guidance. [S7][S9]

## 9. Recommended Bring-up Order

1. `ADC` example: verify power path and battery readout.
2. `I2C_QMI8658`: verify I2C and IMU access.
3. `LVGL_Test` or `FactoryProgram`: verify display init and frame flush.
4. Touch validation (`FactoryProgram` touch enabled).
5. `SPI_SD` and Wi-Fi examples.

## 10. Minimal Integration Template (Custom Project)

For a new project, copy the proven pin macros first, then refactor:

```c
#define LCD_HOST                       SPI2_HOST
#define PIN_LCD_CS                     6
#define PIN_LCD_PCLK                   47
#define PIN_LCD_D0                     18
#define PIN_LCD_D1                     7
#define PIN_LCD_D2                     48
#define PIN_LCD_D3                     5
#define PIN_LCD_RST                    17

#define PIN_I2C_SCL                    39
#define PIN_I2C_SDA                    40
#define PIN_TOUCH_INT                  41
#define TOUCH_I2C_ADDR                 0x38

#define LCD_H_RES                      536
#define LCD_V_RES                      240
```

Then add an `idf_component.yml` similar to Waveshare's FactoryProgram for SH8601/LVGL dependency pull. [S6]

## 11. Troubleshooting Notes

- Blank display at boot:
  - Re-check QSPI pin map and panel init sequence.
  - Validate board revision against SH8601 vs RM67162 naming inconsistency.
- Touch not detected:
  - Confirm GPIO39/40 bus pullups and interrupt line on GPIO41.
  - Probe FT3168 at 0x38.
- IMU not detected:
  - Try address handling for 0x6A/0x6B; package implementation effectively uses 0x6B in low-level calls.
- SD card mount fails:
  - Confirm whether your firmware path is SDMMC-1bit style or SPI fallback in the provided code.

## 12. Sources

- **[S1]** Waveshare docs landing page (board + SKU table + touch notes): https://docs.waveshare.com/ESP32-S3-AMOLED-1.91
- **[S2]** Waveshare resources page (schematic, technical manuals, demos): https://docs.waveshare.com/ESP32-S3-AMOLED-1.91/Resources-And-Documents
- **[S3]** Waveshare ESP-IDF setup page (recommended IDF version + demo table): https://docs.waveshare.com/ESP32-S3-AMOLED-1.91/Development-Environment-Setup-ESP-IDF
- **[S4]** Waveshare product page (SKU attributes + detailed spec table): https://www.waveshare.com/product/esp32-s3-amoled-1.91.htm
- **[S5]** Official schematic PDF: https://files.waveshare.com/wiki/ESP32-S3-AMOLED-1.91/ESP32-S3-AMOLED-1.91.pdf
- **[S6]** Official demo package link (from Waveshare resources): https://drive.google.com/file/d/1k_63uyV5NWyEuRqmIeCVNHwCS1VzPhRA/view?usp=sharing
- **[S7]** ESP-IDF Start a Project / build-flash-monitor flow: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/start-project.html
- **[S8]** ESP-IDF Installation (official): https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/index.html
- **[S9]** ESP32-S3 USB Serial/JTAG guide: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/usb-serial-jtag-console.html
- **[S10]** ESP32-S3 datasheet (GPIO/ADC channel mapping reference): https://documentation.espressif.com/esp32-s3_datasheet_en.pdf
