# Miso

[English](README.md) · [Español](README.es.md)

A woodland pet in a layered forest clearing, with warm mushrooms, swaying
ferns, butterflies and glowing fireflies. Large mint and cream shapes, dark
outlines, expressive eyes and an orange scarf keep Miso clear at a distance.
The scene has no on-screen text, including during naps.

The six-minute cycle passes through daylight, sunset, moonlit night and dawn.
The sun sets and the crescent moon rises; the forest, ground and character
change palette together. Clouds drift, stars twinkle, and butterflies give way
to fireflies at dusk. Miso stays bright enough to read at night.

Miso wanders, discovers and eats berries, sniffs flowers, watches insects,
grooms, stretches, dances, waves, chases and naps. A shuffled activity cycle
gives each autonomous behavior a turn. Night naps last longer, and waking is
followed by a stretch.

![Miso's woodland scene and expressive poses through day and night](preview.gif)

The GIF compresses the six-minute cycle and demonstrates poses. See the
[four lighting phases](preview-day-night.png) and
[action sheet at half display size](preview-actions.png).

| Action | Reaction |
| --- | --- |
| Tap Miso | Hops and hearts |
| Tap empty space | Drop a berry for Miso to eat |
| Swipe | Chase a butterfly by day or a firefly by night |
| Hold for 0.8 seconds | Nap; hold again or tap to wake |
| Tilt | Eyes follow, ears tilt, the body leans and scenery shifts |
| Sustain a stronger tilt | Balance with outstretched paws; wave on settling |
| Gently shake | Cycle surprised hops, dancing and chasing; six-second cooldown |
| Tap BOOT | Cycle medium, bright and dim brightness |
| Hold BOOT for 1.5 seconds, then release | Return to launcher |

Keep the board still for the first half-second after boot to establish neutral
tilt. Brightness starts at medium. A nap is an animation; the board stays awake.

## Development

The simulation and renderer use a 134 × 60 canvas scaled 4×. `main/pet.c` handles
behavior, `main/paint.c` draws the world and character, and `main/input.c`
recognizes gestures. `main/board.c` owns the display and sensor tasks.

From the repository root:

```sh
./tools/test.sh --app pet-pin
python3 tools/make_previews.py pet-pin
python3 firmwares/pet-pin/tools/preview.py --publish
idf.py -C firmwares/pet-pin build
```

Use the root [build script](../../build-and-flash.sh) to install the collection.
Host tests cover 24 simulated hours, unattended activity coverage, feeding,
tilt sustain/recovery, nap protection, shake variety, the six-minute cycle,
touch mapping, motion filtering, rendering bounds and strip byte order.
The [historical verification record](../../documentation/APP_VALIDATION.md)
contains the original device observations.

`python3 tools/preview.py`, run inside this app, exports all fourteen animations,
lighting and action sheets, and a video into ignored `artifacts/`.
Add `--publish` to update the checked-in PNGs. It requires Pillow and FFmpeg
with VP9 support. The checked-in GIF uses the same drawing code; regenerate
its longer showcase with `python3 tools/make_previews.py pet-pin --seconds 28`
from the repository root.

## Diagnostic console

Send newline-terminated commands over USB Serial/JTAG while Miso runs:

```text
status
state idle
state walk
state sniff
state eat
state sleep
state love
state play
state surprise
state wave
state groom
state stretch
state dance
state look
state balance
tap 67 30
swipe 110 30
hold
shake
brightness 0
capture
bars
auto
```

Coordinates are logical 134 × 60 pixels. `state` holds an activity for up to ten
seconds; walking can finish on arrival. `capture` outputs the last logical frame
as RGB565 hex and briefly pauses animation. `bars` displays diagnostic colors;
`auto` restores autonomous behavior. Telemetry uses integer-scaled coordinates,
acceleration and tilt. `PERF` reports frame timing and heap use every ten seconds.

After a reset, FT3168 touch may remain asleep until a contact wakes it. Its IRQ
stays armed; `touch=0` in status does not disable input. See the shared
[board contract](../../documentation/AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.md).
