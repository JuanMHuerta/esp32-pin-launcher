# Project agent guidance

## ESP32 board work

This project targets the Waveshare ESP32-S3-Touch-AMOLED-1.91, SKU 28596.
Before building, flashing, or changing hardware-facing code, read the
[board agent contract](documentation/AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.md).
Use the [validation ledger](documentation/WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91_VALIDATION.md)
when checking a disputed hardware fact, changing versions, or working on SD or
display-controller details. Confirm the board revision before applying a pinout
to different hardware.

## SD card file tool

For SD-card access or changes to the file protocol, read the
[SD card file tool guide](documentation/SD_CARD_FILE_TOOL.md) before acting.
The tool is exposed only by the root launcher over USB Serial/JTAG; app
applications do not provide this service. Keep the C firmware implementation,
Python host client, and protocol documentation synchronized when changing the
framing or operations. Use a unique temporary directory for validation and
remove it when testing is complete.

## Device lessons for future projects

After ESP32 hardware or sensor work, add any new, reusable finding to
[ESP32 device lessons](documentation/ESP32_DEVICE_LESSONS.md). Record the board
variant, the observed behavior, how it was verified, and the practical rule for
future firmware. Distinguish a source-verified fact from an observation on one
board, and update an existing entry when a lesson is already covered. If the
finding changes a board-wide rule, also update the board contract and its
validation ledger.

Keep these lessons about the ESP32 board and its peripherals: display, touch,
IMU, I2C/SPI, power, memory/DMA, USB, flashing, and similar hardware behavior.
Do not add application mechanics, project-specific thresholds or performance
figures, or transient project failures. If no new device lesson was learned,
leave the documentation unchanged.
