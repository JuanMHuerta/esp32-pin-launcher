# Miso

A small woodland pet that wanders, snacks, chases fireflies, waves and naps.
The scene cycles through day and night over six minutes. It plays by itself
and reacts to touch and motion.

![Miso reacting to touch and looking for a berry](preview.gif)

| Action | Reaction |
| --- | --- |
| Tap Miso | Hops and hearts |
| Tap empty space | Drop a berry for Miso to eat |
| Swipe | Chase a firefly |
| Hold for 0.8 seconds | Nap; hold again or tap to wake |
| Tilt | Eyes follow and the body leans |
| Gently shake | Surprised hop; six-second cooldown |
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
idf.py -C firmwares/pet-pin build
```

Use the root [build script](../../build-and-flash.sh) to install the collection.
Host tests cover 24 simulated hours, interactions, touch mapping, motion filtering,
rendering bounds and strip byte order. The [historical verification record](../../documentation/APP_VALIDATION.md)
contains the original device observations.

`python3 tools/preview.py`, run inside this app, exports individual animations
and a contact sheet into ignored `artifacts/`. It requires Pillow and FFmpeg
with VP9 support. The checked-in GIF uses the same drawing code.

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
