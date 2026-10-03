# Repository validation — 2026-10-02

This checks the nine-app collection after MECH removal, documentation cleanup
and GPL v3 licensing. Hardware was not flashed or exercised during this pass.
Older hardware observations remain in their dated validation records.

## Host checks

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

## Firmware builds

ESP-IDF 5.5.1 and the pinned SH8601 component built the launcher and all nine
apps with `./build-and-flash.sh --build-only`. No compiler warnings or errors
appeared. The compiled partition table matched the layout generator.

The same command passed in a fresh copy containing only publication files,
without existing build directories, downloaded components or generated
`sdkconfig` files. The final launcher change that clears stale Demo selection
was also built against that fresh configuration.

## Previews

All nine README GIFs contain multiple 536 × 240 frames, with positive playback
lengths between six and fourteen seconds. Representative frames were visually
reviewed. The previews use the firmware's renderers; they do not establish
physical panel appearance or touch/IMU behavior.

The GitHub workflow has been added; these results are local checks of its build
and test commands, not a hosted Actions run.
