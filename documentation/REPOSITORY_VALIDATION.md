# Repository validation

[English](REPOSITORY_VALIDATION.md) · [Español](REPOSITORY_VALIDATION.es.md)

## Release preparation — 2026-10-03

This pass covers the nine-app collection, bilingual documentation and the static
USB installer. The local tools were ESP-IDF 5.5.1, GCC 16.2.1, Python 3.14.7,
Node.js 22.23.1, Ruff 0.16.10 and clang-format 23.1.2. Firmware component versions
remain pinned by the manifests and lockfiles.

### Code and previews

- The host suite passed for all nine apps and shared graphics, with strict C
  warnings, AddressSanitizer and UndefinedBehaviorSanitizer. The separate
  undefined-behavior trap run also passed.
- Sixteen Python unit tests passed, covering flash allocation, the SD host
  protocol and release packaging. Packaging tests check image hashes, source
  archives, license notices, reproducibility, stale tables and operation without
  Git metadata.
- Python lint/format, handwritten C format, shell syntax and `git diff --check`
  passed. Generated asset arrays are excluded from handwritten formatting.
- Repository checks passed for local links, English/Spanish guides and app
  READMEs, source license identifiers, catalog/shortcut alignment and file sizes.
- Both root READMEs include all nine animated app previews. Each GIF has
  536 × 240 frames and positive frame durations; playback ranges from six to
  32 seconds. Representative frames and the installer at desktop and phone
  sizes were visually reviewed. These are host renderings, not panel recordings.

The workstation's sanitizer runtimes were extracted into an ignored local
validation directory. Tests and release-table checks use unique temporary
directories and remove them on exit.

### Firmware and web release

The launcher and all nine apps built with `./build-and-flash.sh --build-only`.
The allocator matched the compiled full-collection partition table. The static
release includes SHA-256-addressed firmware, nine previews, upstream license
texts, dependency source locations and a matching project source archive.

The source archive was extracted into a unique temporary directory with no
Git metadata, build directories, downloaded components or generated
`sdkconfig` files. From that copy, the host suite, launcher and all nine firmware
builds, Git-free packaging, `npm ci`, web formatting/unit checks, release-table
checks and static-site build passed. The temporary copy was removed afterward.

Nine web unit tests and seven Chromium browser tests passed. They cover all
511 nonempty selections, integrity/capacity guards, write/reset failures, port
cleanup, canceled connections, numeric browser permission errors, translated
status/errors, saved language choices, repository subpaths and narrow layouts.
Browser tests use synthetic firmware and do not write to a board.

The real-release check independently parsed, verified and reproduced all 511
browser-generated partition tables with ESP-IDF's Python implementation. It
also verified every packaged image's size, SHA-256 and ESP32-S3 header and the
source archive's size and hash.

### Connected-board checks

The board was the project's Waveshare ESP32-S3-Touch-AMOLED-1.91, SKU 28596,
with ESP32-S3 silicon v0.2, 16 MB flash, embedded 8 MB PSRAM and native USB
Serial/JTAG. Silicon revision is not a PCB revision identifier; no pinout was
applied to a different board.

The production `web/src/install.js` and pinned esptool-js loader installed the
actual release images through a local serial adapter implementing Web Serial's
stream interface. Every write passed on-device MD5 verification. This exercises
the installer and ROM/stub protocol on real hardware; the native browser device
picker and browser USB permissions were covered separately by mocked tests.

| Installed selection | Observed result |
| --- | --- |
| Miso + CRT | Two apps discovered; omitted shortcut `1` ignored; Demo ran Miso → CRT → Miso at five-minute intervals, at `0x80000` and `0xe0000` |
| Fluid only | One app discovered; omitted shortcut `1` ignored; Demo restarted Fluid after five minutes at `0x80000` |
| All nine apps | Nine apps discovered; USB shortcut `9` booted CRT at the generated `0x3e0000` address |

No panic, abort or brownout appeared in those observations. The final shared
switcher also defers Demo rotation while BOOT is held; this follows the existing
GPIO0 reset rule. The Fluid-only and full-collection checks used that final
build. These checks did not exercise physical BOOT holds or every app's gestures.

The user confirmed Fluid's physical checks: no dark holes or V-shaped band in
settled water, prompt release when tilted, and visible motion when carrying the
board as if walking. The [physics review](../firmwares/fluid-pin/docs/physics-review.md)
records the result and closes its earlier pending checks.

Before installation, the complete 16,777,216-byte flash was backed up. After
validation, that image was restored, including NVS and OTA selection, and a
separate full-flash `verify_flash` matched its digest. Backup SHA-256:
`0bd3bd19546aa9275b110ab4728e6d0eff6435f64183eeebc8da56781905d37b`.
The restored Miso firmware then started successfully at its original brightness.
The backup and serial/build logs remain in the ignored
`artifacts/release-review-20261003/` directory. No SD files were changed and no
new reusable device lesson was learned.

The [manual Pages workflow](../.github/workflows/web-flasher-pages.yml) and CI
artifact build are prepared. Results here are local; no hosted GitHub Actions
run or public deployment is claimed.

## Earlier repository pass — 2026-10-02

This checks the nine-app collection after MECH removal, documentation cleanup
and GPL v3 licensing. Hardware was not flashed or exercised during this pass.
Older hardware observations remain in their dated validation records.

### Host checks

- `./tools/test.sh`: passed for all nine apps, shared graphics, flash allocation
  and the SD host client. C warnings are treated as errors.
- The same suite with AddressSanitizer and UndefinedBehaviorSanitizer: passed.
- UndefinedBehaviorSanitizer trap mode: passed.
- Twelve Python unit tests passed, including fragmented serial frames, device
  errors, path validation, atomic downloads and upload acknowledgement checks.
- Asset compilation reproduced the checked-in Dungeon tables.
- `ruff check .`, `ruff format --check .`, `./tools/format.sh --check`, shell
  syntax checks and `git diff --check`: passed.
- `python3 tools/check_repo.py`: passed. Local Markdown links, nine README GIFs,
  source license identifiers, app counts and menu/OTA order were checked.

The host compiler was GCC 16.2.1; Python was 3.14.7. Sanitizer runtime libraries
were extracted into a temporary directory for this workstation's tests.

### Firmware builds

ESP-IDF 5.5.1 and the pinned SH8601 component built the launcher and all nine
apps with `./build-and-flash.sh --build-only`. No compiler warnings or errors
appeared. The compiled partition table matched the layout generator.

The same command passed in a fresh copy containing only publication files,
without existing build directories, downloaded components or generated
`sdkconfig` files. The final launcher change that clears stale Demo selection
was also built against that fresh configuration.

### Previews

All nine README GIFs contain multiple 536 × 240 frames, with positive playback
lengths between six and fourteen seconds. Representative frames were visually
reviewed. The previews use the firmware's renderers; they do not establish
physical panel appearance or touch/IMU behavior.

The GitHub workflow has been added; these results are local checks of its build
and test commands, not a hosted Actions run.
