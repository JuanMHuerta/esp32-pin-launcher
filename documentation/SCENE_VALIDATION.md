# New app validation

[English](SCENE_VALIDATION.md) · [Español](SCENE_VALIDATION.es.md)

Historical record: this predates MECH removal and the current nine-app menu.
Offsets, subtypes and shortcut numbers below describe the tested layout only.

The hardware observations recorded here used the earlier fixed 1 MiB app
layout. Current addresses and sizes are generated from image sizes in the root
`partitions.csv`, in minimum 64 KiB allocations. Use `build-and-flash.sh` for
layout changes; historical offsets in this report must not be used to flash a
device with the new table.

Validated on 2026-10-01 using ESP-IDF **5.5.1**, target **esp32s3**,
and **esp_lcd_sh8601 2.0.1~1**. The connected SKU 28596 board reported
**ESP32-S3 revision v0.2**, 16 MB flash and embedded 8 MB PSRAM.
The new apps use internal RGB565 buffers; PSRAM is not enabled or required.

## Implementation and launcher integration

- THREE BODY integrates three mutually gravitating masses with RK4, adaptive
  encounter steps, three scenarios, orbit trails, camera zoom and energy telemetry.
- CRT plays normal boot, recovery and deep-space relay scripts. Each includes
  typed commands and varied output; the renderer supplies CRT curvature, green
  and amber phosphors, glow, scanlines, refresh band, cursor and reboot collapse.
- Launcher has ten entries on two pages. USB `9` selects THREE BODY at
  `ota_8` / `0x8a0000`; `0` selects CRT at `ota_9` / `0x9a0000`.
- Both apps initialize the shared BOOT return task and use short BOOT releases
  to switch their scenarios. The shared hardware mapping and display init
  sequence are reused from the existing maze app. DMA ownership, even
  transfer boundaries and exactly one byte swap are retained.

## Host and build checks

`tools/test_scenes.sh` passed with strict C11 compiler warnings.
The host lacks the AddressSanitizer runtime; this run used the default test
configuration with explicit buffer guards. Sanitizers remain selectable.
The tests cover one-period figure-eight return, conservation through each full
scenario, finite camera zoom, switching, RGB565 strip coverage/byte order,
every CRT event and glyph, typing order, progress, scrolling, time-step
independence, reboot boundaries and renderer buffer guards.

Full-scenario host relative energy drifts were approximately:
`-2.56e-11` (figure eight), `-1.59e-10` (chaotic suns), and `2.39e-7` (binary visitor).
PNG and GIF previews were generated and visually reviewed using the same C
renderers as the firmware. Every preset has both preview formats.

Both app builds and the root launcher build passed. Each app is
less than 271 KB and fits its 1 MB launcher partition. Python compilation,
shell syntax checks and `git diff --check` passed.

## Attached-board results

The launcher and both new app images were flashed. Dedicated `verify_flash`
checks matched both final app images. Existing app offsets stay the same.

| Playback check | Result |
| --- | --- |
| THREE BODY, 140 seconds | All three scenarios and return to figure eight; 27 samples, 25.0–34.4 fps, 212664–212736 bytes free heap |
| CRT, 180 seconds | All three sequences and reboot into normal boot; 35 samples, 20.9–21.7 fps, 158280–158500 bytes free heap |
| Final THREE BODY image, 16 seconds | 3 samples, 29.5–34.3 fps, 212736 bytes free heap |
| Final CRT image, 16 seconds | 3 runtime samples, fps=21.6..21.7, heap=158280..158500 |

The final smoke checks follow the addition of missing terminal punctuation
glyphs (`@`, `~`, `?`). The final images match these SHA-256 values:

| Image | Bytes | SHA-256 |
| --- | ---: | --- |
| `three_body_pin.bin` | 270752 | `bbfe2d441d10d96f34f1294628263b94e3a888b570c6e62a851c682ff4773953` |
| `crt_pin.bin` | 269472 | `e0d74a0c4fe6dd9ee9253ad451022f1beb3709a4bac6235435e83ba8b659c16f` |

No panic or transfer failure appeared in the runtime logs. GPIO button
behavior is inherited from the established shared return task; validation
used USB launch commands rather than physically pressing the button.

The launcher SD service mounted the existing 31116288-sector card. An 8458-byte
binary containing embedded `MPFS`, nulls, and launcher command bytes passed
an exact upload/download comparison. Validation used a unique temporary
directory and removed it afterward; the final root listing was empty.

The board was left running CRT. No new reusable hardware fact was learned,
so the board contract, board validation ledger and device lessons were unchanged.


## CRT continuous-console update — 2026-10-02

The CRT app now boots once and cycles six workflows indefinitely,
retaining scrollback and its randomly selected green or amber phosphor across
all automatic transitions. Manual BOOT restarts select a fresh color. The
playlist contains 610 events and lasts 233.6 seconds, with faster command typing
and burst output. The renderer increases bloom, scanline contrast, curvature,
and refresh effects while keeping phosphor monochrome.

The shared board pins, display initialization, RGB565 byte swap, and DMA
transfer ownership are unchanged. No new board or peripheral lesson was learned.

Host checks passed for every event and glyph, complete commands before output,
progress, scrollback continuity through every boundary, 100 complete playlist
loops, time-step agreement, the 32-bit timer boundary, manual restart/color
selection, monochrome rendering, and framebuffer guards. Both apps'
host suites passed. An additional UndefinedBehaviorSanitizer run could not link
because the host lacks libubsan.so.1.0.0; sanitizer checks were not run.
The CRT build passed using the existing ESP-IDF 5.5.1
installation and pinned component dependencies; its 291872-byte image fits the
1 MB app partition. These results supersede the earlier CRT playback
behavior, but the attached-board measurements above describe the old image.
The updated CRT image was flashed to the attached ESP32-S3 revision v0.2
board after reading and confirming its existing ota_9 partition at 0x9a0000.
A dedicated verify_flash check matched the image digest. USB command 0 launched
the app, and a 22-second serial smoke check passed with four runtime
samples: 21.2–21.6 fps and 157992–158212 bytes free heap. Startup completed,
the selected phosphor stayed green, and no crash was reported. The board was
left running CRT. This check did not cover a full playlist or physical BOOT taps.

Flashed CRT image SHA-256: ef7b0a7eb9d7fefe8becd48e635b5052602e8ea80a2134003a317a5cd9027cc0.


### CRT readability adjustment — 2026-10-02

Green phosphor now includes a small neutral tint (red at 1/7 and blue at 1/6
of green intensity). Text and cursor are enlarged uniformly by 6.25% in the
CRT renderer; the 39-column, ten-row terminal and shared display driver stay
unchanged. Green and amber previews were reviewed, and all six previews updated.
Host tests and the ESP-IDF 5.5.1 build passed. The 291984-byte
image was flashed at 0x9a0000 and matched by verify_flash. A 22-second serial
smoke check reported four samples at 20.0–20.4 fps, 157992–158212 bytes free heap,
and no crash. That launch randomly chose amber; a normal restart chose green,
with a 20.4 fps runtime sample. The board was left showing the softened green.
No new reusable device lesson was learned.

Current CRT image SHA-256: c38bcbaa4ccad7972a11827141d6e0d1569f622754525cc6cd82188f10377fb7.


### CRT curvature reduction — 2026-10-02

Both horizontal and vertical curvature coefficients were halved to reduce
edge distortion of the enlarged font. All six previews were regenerated,
and the green preview was visually reviewed. Host tests and the pinned
ESP-IDF 5.5.1 build passed. The 291920-byte image was flashed at
0x9a0000 and matched by verify_flash. A 17-second serial smoke check passed:
three samples at 21.4–21.7 fps, 157992–158212 bytes free heap, and no crash.
The board was left running CRT in its randomly selected amber theme.
No new reusable device lesson was learned.

Current CRT image SHA-256: 6464ad4baeb0911f8f17ebab6872e1076912c33b151974a3542e1e26d1f80ca8.


### CRT comparison test: original font and curvature — 2026-10-02

Restored the original font size by removing the 6.25% text/cursor enlargement,
and restored the stronger curvature coefficients from before the readability
adjustments (horizontal divisor 29, vertical divisor 28). The softened green
phosphor remains. All six previews were regenerated; the green preview was
visually reviewed. Host tests and the ESP-IDF 5.5.1 build passed. The
291920-byte image was flashed at 0x9a0000 and matched by verify_flash.
A 17-second serial smoke check passed: three samples at 20.9–21.3 fps,
157992–158212 bytes free heap, and no crash. The board was left running the
comparison image in its randomly selected amber theme.
No new reusable device lesson was learned.

Current CRT image SHA-256: 626561ee375059b9858a682f5287737d83a1effeedb4fedccc81d2d4775d266c.


### CRT final content and layout pass — 2026-10-02

Removed the bottom nameplate, indicator, and wide lower bezel. The glass now
holds twelve rows at the original font size, with its curvature normalized to
the taller area. The playlist grew from 610 to 817 events and lasts 317.9 seconds.
New sequences include survey jobs, priority mail, watchdog and clock failover,
buffered radio handoffs, route simulations, mirrored archive repair, and
interference suppression. Animated ASCII carrier traces update in place.
Subtle sci-fi references appear in routine station tags, job IDs, and records.

Host tests passed for all scripted events and glyphs, animation, completion,
scrolling through all six workflows, time-step agreement, 100 playlist loops,
monochrome output, framebuffer guards, and text in the recovered bottom rows.
All six previews were regenerated; green and amber were visually reviewed.
The pinned ESP-IDF 5.5.1 build passed. The 301232-byte image was
flashed at 0x9a0000 and matched by verify_flash.

An initial serial observation was interrupted by USB re-enumeration. On
reconnection, the boot log reported USB_UART_CHIP_RESET; a 35-second check
produced six healthy runtime samples at 20.6–21.2 fps. A subsequent 75-second
observation covered the new survey events and the continuous operations-to-
maintenance transition, with no automatic boot or phosphor change. An incomplete
line on monitor connection caused the collector's final parser to fail; six
complete late samples were separately validated at 20.5–20.6 fps with exactly
157904 bytes free heap throughout. No firmware panic was reported. The board
was left running CRT in green. This physical observation covered two workflows;
all six workflows and their boundaries were covered by host tests.
No new reusable device lesson was learned.

Current CRT image SHA-256: 6c804715996e807c743f1d2bbe8bb002fcf1230bd89273865b9614b752694de9.
