# ESP32 device lessons

[English](ESP32_DEVICE_LESSONS.md) · [Español](ESP32_DEVICE_LESSONS.es.md)

Reusable hardware and sensor findings for the Waveshare
ESP32-S3-Touch-AMOLED-1.91 (SKU 28596). Keep project behavior and application
settings out of this file. The [board agent contract](AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.md)
holds the working rules; the [validation ledger](WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91_VALIDATION.md)
records the supporting sources and resolved conflicts.

When adding a finding, state its board revision or scope, observed symptom,
test or source, and the action that helped. Mark observations on one board as
such; do not turn them into universal pinouts or performance guarantees.

## Existing reusable findings

- **App partition alignment (source verified, ESP32-S3 including SKU 28596):**
  ESP-IDF requires app partition offsets aligned to 64 KiB. A bootable image
  occupies one contiguous app partition; different app partitions can have
  different sizes. Verified against the ESP-IDF 5.5.1 partition generator and
  [partition-table documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-guides/partition-tables.html).
  When packing images into flash, round each allocation to whole 64 KiB blocks
  and derive write addresses from the same table. If boundaries move, reinstall
  the table and affected images together; an old standalone write address can
  overwrite a different app even when the image itself is valid.
- **Display controller naming (source verified):** SKU 28596 uses an RM67162
  display IC, while Waveshare's working ESP-IDF path uses an SH8601-compatible
  QSPI driver. Check the initialization and transfer path before replacing a
  driver solely because its name differs from the IC label. See the validation
  ledger's display-controller resolution.
- **Touch wake behavior (datasheet backed; observed on one board):** An idle
  FT3168 can NACK I2C reads after traffic to another device on the shared bus.
  Keep the touch interrupt active and retry after a touch wake event before
  concluding the sensor is missing. See the validation ledger's touch-startup
  resolution.
- **Display DMA buffer ownership (device interface rule):** A display transfer
  can still be reading its source buffer after the draw call returns. Reuse that
  buffer only after the panel-I/O completion callback; otherwise strips can
  tear or corrupt. See the board contract's rendering rules.
- **SDMMC/display coexistence (observed on SKU 28596, ESP32-S3 revision v0.2):**
  The current Waveshare `VersionControl_V2` 1-bit SD mapping (CLK GPIO9, CMD
  GPIO42, D0 GPIO8) mounted a 31,116,288-sector card while the same boot also
  initialized the SH8601-compatible display. This was verified on 2026-10-01
  with the launcher, including recursive USB file-tool read/write/delete tests.
  Use this mapping only for the validated v0.2 board/repo path; do not generalize
  it to another board revision without repeating the display-plus-SD check.
