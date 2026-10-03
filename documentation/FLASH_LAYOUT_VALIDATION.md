# Minimum app flash allocation — 2026-10-02

Historical record: this predates MECH removal and the current nine-app menu.
Offsets, subtypes and shortcut numbers below describe the tested layout only.

Target: Waveshare ESP32-S3-Touch-AMOLED-1.91, SKU 28596; attached ESP32-S3
silicon revision v0.2; 16 MiB flash; native USB Serial/JTAG. Builds used the
existing ESP-IDF v5.5.1 toolchain and component locks.

Each image occupies one contiguous app partition. Its capacity is exactly
`ceil(image_bytes / 65536) * 65536`; there are no extra growth blocks. NVS at
`0x9000` and OTA metadata at `0xf000` retain their previous addresses and sizes.
The factory launcher starts at `0x20000`; all subsequent apps are contiguous.

This is a snapshot of the installed layout. Future builds regenerate
`partitions.csv` from freshly built images; always use that table and the full
`build-and-flash.sh` workflow when installing a changed layout.

| Image | Image bytes | 64 KiB blocks | Capacity KiB | Offset |
|---|---:|---:|---:|---|
| Launcher | 358,752 | 6 | 384 | `0x20000` |
| CONWAY | 269,424 | 5 | 320 | `0x80000` |
| FLUID | 288,784 | 5 | 320 | `0xd0000` |
| MISO | 341,728 | 6 | 384 | `0x120000` |
| LUMEN | 279,120 | 5 | 320 | `0x180000` |
| MECH//BAY-07 | 254,048 | 4 | 256 | `0x1d0000` |
| DUNGEON//SEED | 743,648 | 12 | 768 | `0x210000` |
| 3D MAZE | 262,080 | 4 | 256 | `0x2d0000` |
| WAYFARER | 654,464 | 10 | 640 | `0x310000` |
| THREE BODY | 270,752 | 5 | 320 | `0x3b0000` |
| CRT | 301,232 | 5 | 320 | `0x400000` |

The last partition ends at `0x450000`. The region through `0x1000000` is
unallocated: **12,255,232 bytes (11.6875 MiB)**. This recovers 6.3125 MiB of
contiguous capacity compared with the previous ten 1 MiB app partitions
and 512 KiB launcher partition. There are also 366,880 unused bytes within the
rounded app allocations; these do not form a separate bootable partition.

## Checks

- `python3 -m unittest discover -s tools/tests -v`: five tests passed. Coverage
  includes exact block boundaries, minimum sizes, contiguous alignment, missing
  and empty images, the 16 MiB limit, and rejection of stale CSV/binary tables.
  Each test used a unique temporary directory which was automatically removed.
- `bash -n build-and-flash.sh` and `git diff --check`: passed.
- `./build-and-flash.sh --build-only`: all ten apps and the launcher
  built successfully; the generated binary partition entries matched the CSV
  and current image sizes.
- `./build-and-flash.sh`: installed the root bootloader, launcher, new partition
  table, and all ten app images. Every app passed explicit
  `verify_flash` digest comparison at its relocated address.
- Read back the device's complete 3,072-byte partition table from `0x8000` into
  a unique temporary directory, compared it byte for byte with the build output,
  and removed the temporary directory. SHA-256:
  `fec64f5454af801d2b9b5f04cef012cdaee7f520715e8ebe32f6f4acca769358`.
- Selected every app through the launcher's existing USB shortcuts
  (`1`–`9`, `0`). For each, confirmed the launcher's selected address, the
  bootloader's loaded address, and its app-specific startup log during a
  six-second observation. MAZE also emitted its runtime `PERF` sample. No
  firmware panic, abort, or reset loop was observed. This checks startup and
  addressing; it is not a long-duration gameplay or visual validation.
- Cleared only OTA boot-selection metadata between boot checks. After the last
  check, confirmed `pin_launcher: READY:` and left the board in the launcher.

The launcher and app return path select partitions by subtype/label,
so their existing selection code needed no address changes. The flashing
workflow derives app addresses from the same allocation data and checks
the compiled partition table before the first device write.
