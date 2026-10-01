# DUNGEON//SEED

An autonomous, original first-person pixel dungeon crawler for the 536×240
Waveshare ESP32-S3 display. It renders a 134×60 RGB565 logical canvas at 4×
nearest-neighbour scale through double-buffered 40-row DMA strips at 30 fps.

The viewport uses two explicit stone-built camera families: a deep centred
corridor for corridors, encounters, and exits; and a broad chamber for
arrivals, treasure, shrines, and bosses. Both are composed from separate near,
middle, and far wall courses plus compressed ceiling and floor stones around a
dark rear arch. The environment stays in a charcoal/slate/off-white material
ramp; colour is reserved for actors, fire, magic, blood, and HUD feedback.
Editable 16-colour PNG source sheets live in `assets/source`;
`tools/compile_assets.py` validates and packs them as transparent 4-bit sprites
in `main/assets_generated.c`. Enemy
cards use large, distinct silhouettes (skeleton knight, blue-violet slime,
horned brute, floating eye), while bosses use telegraph, attack, counter, and
defeat frames in the forward tile.

Every run starts with a title card, crosses four to six randomized room cards,
then resolves a telegraphed boss encounter before starting again. No input is
assigned to short BOOT presses; the shared 1.5-second long press returns to the
launcher.

## Build and test

After sourcing ESP-IDF:

```sh
idf.py build
cc -std=c11 -Wall -Wextra -Werror -I main tests/test_dungeon.c main/dungeon.c main/paint.c main/assets_generated.c -o /tmp/test_dungeon
/tmp/test_dungeon
python3 tests/test_assets.py
```

Run `python3 tools/make_preview.py` to regenerate `preview.png`, a labelled
70-card montage covering every corridor/chamber set piece and every regular-
enemy and boss phase, without requiring a device.

The collection-level `./build-and-flash.sh` places this image in `ota_5` at
`0x5a0000`.
