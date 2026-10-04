# Contributing

[English](CONTRIBUTING.md) · [Español](readmes/CONTRIBUTING.es.md)

Use ESP-IDF 5.5.x and keep the display component pinned to the version in
`main/idf_component.yml`. Read the [board contract](documentation/AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.md)
before changing hardware-facing code. Hardware details and observations belong
in the shared documentation, rather than separate copies in each app.

## Source layout

| Directory | Contents |
| --- | --- |
| `main/` | Launcher, menu and SD file service |
| `common/` | Return/demo switching, shared graphics and QSPI adapter |
| `firmwares/` | Independently buildable apps, their assets and host tests |
| `tools/` | Build layout, previews, host tests and serial clients |
| `documentation/` | Board constraints, protocol and validation records |
| `web/` | Static USB flasher, release build and browser tests |

Most apps separate simulation and painting from their ESP-IDF entry point.
Keep that boundary so behavior and rendering can be checked without hardware.
Generated asset arrays should be rebuilt from the PNG sources rather than
edited by hand.

Keep the English and Spanish READMEs and user guides in sync. Use direct
descriptions of behavior and keep app names, commands and protocol fields intact.

## Checks

Install `requirements-dev.txt` in a virtual environment, then run:

```sh
./tools/test.sh
python3 tools/check_repo.py
ruff check .
ruff format --check .
./tools/format.sh --check
./build-and-flash.sh --build-only
```

For the web flasher, run `npm ci`, `npm test`, `npm run format:check` and
`npm run test:browser` inside `web/`. See the
[flasher guide](documentation/WEB_FLASHER.md) for packaging and previewing a release.

C checks use warnings as errors. `SANITIZERS=address,undefined ./tools/test.sh`
adds sanitizer checks when the compiler runtimes are installed.
`SANITIZERS=undefined SANITIZER_TRAP=1 ./tools/test.sh` uses trap mode when only
undefined-behavior instrumentation is available. Use `./tools/test.sh --soak`
for Fluid's longer runs. Tests compile in unique temporary directories and
remove them on exit.

Hardware changes also need checks on the actual supported board. Record the
board/revision, toolchain, commands and observed results. Host previews do not
verify display colors, touch orientation or sensor behavior. Add reusable
hardware findings to [device lessons](documentation/ESP32_DEVICE_LESSONS.md)
only when something new was learned.

## Formatting and comments

Use the checked-in clang-format and Ruff settings. `./tools/format.sh` formats
handwritten C; `ruff format .` formats Python. The formatter skips generated C
asset tables. Include `SPDX-License-Identifier: GPL-3.0-only` in new source files.

Comments should explain units, invariants or a reason the code cannot be
simpler. Remove comments that restate a statement or describe a completed edit.
Documentation should describe current behavior and reproducible commands.
Keep measurements and old flash addresses in dated validation records.

## Preview generation

From the repository root:

```sh
python3 tools/make_previews.py
python3 firmwares/render-pin/tools/render_previews.py
python3 firmwares/dungeon-pin/tools/make_preview.py
python3 firmwares/wayfarer-pin/tools/make_preview.py
python3 tools/preview_scene.py three-body-pin --preset 0 --seconds 14
python3 tools/preview_scene.py crt-pin --preset 0 --start 44 --seconds 14
```

The first command renders Conway, Fluid, Miso and Maze. The other commands
render the remaining apps. Pillow is required; Lumen also uses FFmpeg.
Preview tools compile the same C simulation and painting code used by the
firmware. Keep the README images small enough to load comfortably on GitHub.

## Adding an app

Add an ESP-IDF project under `firmwares/` with `sdkconfig.defaults`, a pinned
component manifest, a README and a host test. Link `common/app_switcher.c` so a
BOOT hold returns to the launcher. Add the app to the catalog in `main/main.c`
and register its image in `tools/app_layout.py` and its presentation in
`tools/package_web_firmware.py` and `web/src/i18n.js`. Demo discovers the installed
OTA partitions. Keep menu order, OTA subtypes and USB shortcuts aligned.
Add its test to `tools/test.sh` and a renderer-produced GIF to the README.

Rebuild every image before generating a new layout. If partition boundaries
change, install the partition table and all images together. Use the
[SD file guide](documentation/SD_CARD_FILE_TOOL.md) when changing the USB file
service; its C implementation, Python client and protocol documentation must
agree.

In a pull request, describe the behavior changed and the checks you ran. For a
hardware change, say which board revision was tested and what remains unverified.
Contributions are distributed under the repository's GPL v3 license.
