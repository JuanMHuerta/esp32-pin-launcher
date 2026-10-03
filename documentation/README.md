# Documentation

Start with the [project README](../README.md) for installation and controls.

| Document | Purpose |
| --- | --- |
| [Board contract](AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.md) | Pins, display/DMA rules and hardware workflow |
| [Hardware validation ledger](WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91_VALIDATION.md) | Primary sources and revision-specific findings |
| [Device lessons](ESP32_DEVICE_LESSONS.md) | Reusable hardware observations |
| [SD file guide](SD_CARD_FILE_TOOL.md) | USB file client, framing and operations |
| [Repository validation](REPOSITORY_VALIDATION.md) | Current host, build and publication checks |
| [Flash layout validation](FLASH_LAYOUT_VALIDATION.md) | Historical allocation and boot checks |
| [Scene validation](SCENE_VALIDATION.md) | Historical Three Body and CRT checks |
| [App validation](APP_VALIDATION.md) | Historical Dungeon, Miso, Maze and Wayfarer checks |
| [App ideas](APP_IDEAS.md) | Possible additions and implementation constraints |

Validation records describe the sources and firmware at their stated dates.
Older records use earlier menu assignments and flash offsets. Build the current
layout with `build-and-flash.sh`; do not use historical addresses to install an app.
