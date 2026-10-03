# DUNGEON//SEED

[English](README.md) · [Español](README.es.md)

An autonomous old-school first-person dungeon animation for the 536×240
Waveshare ESP32-S3-Touch-AMOLED-1.91. Built to watch from a distance: monsters, held weapons and a full-height world.

One connected 19×19 raycast dungeon contains nine persistent rooms, branching
passages, side routes and loops. The camera walks between rooms, turns at
junctions, looks back and revisits cleared areas. Walls, floor, ceiling, props
and depth-clipped sprites all share the same moving camera and world coordinates.
Eight exploration destinations lead to a boss chamber. Biomes cycle
through cold catacombs, mossy ruins, ember basalt and a frozen blue vault.
Twelve room types add hinged doors, treasure, shrines, stairs, junctions,
crypts, libraries, armories and mushroom groves.

Eight enemies and six bosses have four distinct poses including
collapsed remains. Combat has approach, telegraph, quick lunge, timed player
counters and defeat. Six held weapons—sword, crystal staff, axe, mace, dagger
and crossbow—show gripping gloves/bracers, short wind-up, strike/projectile
and recovery. All grips are right-handed, enter from the lower-right and aim
inward; the crossbow uses an oblique inward-pointing rail. Shutters occur only at descent entry and stairs, not
between rooms. Opened loot and corpses remain in place on revisits. Progress
diamonds have a safe bottom inset. Small beveled corner plates sit over the
continuing floor instead of cutting the scene off with a black HUD strip.

Enemy HP is real simulation state: normal enemies need at least three hits,
bosses at least seven, with repeated wind-up, enemy attack, player counter and
recovery exchanges. The small enemy bar shows actual remaining health.
Player HP/MP persist through encounters, walking, turning and biome changes.
Landed attacks reduce HP; alternating guarded rounds do not. Staff casts cost
eight MP and an exhausted caster switches to steel. Potion inventory is consumed
in a visible drinking animation, and a shrine restores resources only once per
map. There is no pose-driven or room-driven resource regeneration. A player
defeat explicitly fades to a restart rather than silently refilling.

The allocation-free renderer uses a 268×120 RGB565 canvas, expanded exactly 2×.
The display uses double-buffered 40-row DMA strips and one RGB565 byte swap. The framebuffer is 64,320 bytes; 116 indexed 64×64 sprites occupy
475,136 flash bytes. PSRAM is not required. The frame target is 30 fps; a
10-second serial heartbeat reports frame rate, render time, movement state,
player resources and enemy damage/hit count.
Hold BOOT for 1.5 seconds to return to the launcher.

## Art and previews

- [Selected scenes](preview-showcase.png)
- [All 185 labelled room, encounter, gear and transition frames](preview.png)
- [Four biomes × six connected-world viewpoints](preview-biomes.png)
- [Six weapons × four attack timings](preview-weapons.png)
- [Corrected axe × four attack timings](preview-axe.png)
- [192 seconds of actual seeded exploration](preview-motion.gif)
- [Autoplay timeline](preview-run.png)
- [Five-hit combat animation](preview-combat.gif)
- [Persistent combat health timeline](preview-combat.png)
- [Artwork sources and rebuild instructions](assets/ART_DIRECTION.md)

Editable, palette-locked PNGs live in `assets/source`; transparent generated
character/weapon masters are preserved in `assets/masters`.
The silhouette importer rejects touching/missing sprites, preserves scale per
animation row and locks colors to the shared palette. Native architecture,
materials, props and effects remain editable in `tools/make_source_assets.py`.

```sh
python3 tools/make_source_assets.py
python3 tools/import_creatures.py assets/masters/monsters.png 4 assets/source/actors.png
python3 tools/import_creatures.py assets/masters/monsters-extra.png 4 assets/source/actors-extra.png
python3 tools/import_creatures.py assets/masters/bosses.png 3 assets/source/bosses.png
python3 tools/import_creatures.py assets/masters/bosses-extra.png 3 assets/source/bosses-extra.png
python3 tools/import_creatures.py assets/masters/gear-right.png 2 assets/source/gear-right.png --columns 2
python3 tools/import_creatures.py assets/masters/gear-extra-right.png 4 assets/source/gear-extra-right.png --columns 2
python3 tools/import_creatures.py assets/masters/axe-right.png 1 assets/source/axe-right.png --columns 2
python3 tools/compile_assets.py
tools/test.sh
python3 tools/make_preview.py
```

Tests cover 40 minutes of deterministic simulation, traversable camera paths,
turns/backtracks/revisits, every biome/enemy/boss/weapon, elapsed-time carryover,
persistent player/enemy health, multi-hit fights, potion consumption, one-use
shrines, stable world projection, safe progress-marker placement, 25,920
canary-guarded render cases, every pixel of all six DMA strips, biome palette
roles, inward right-hand grips, collapsed silhouettes and reproducible C assets.

## Build and install

Read the repository board contract and source the pinned ESP-IDF 5.5 environment:

```sh
idf.py build
idf.py size
```

The launcher's dungeon app is `ota_4`. Its current offset and minimum
64 KiB block allocation are generated in the root `partitions.csv` by
`build-and-flash.sh`. Use that script to reinstall all images after a layout change.
Do not use a standalone app `idf.py flash` to install into the launcher:
its standalone bootloader/partition table would replace the collection's layout.
Host previews verify composition/motion; physical display appearance still
requires viewing the board.

Historical device checks are in [app validation](../../documentation/APP_VALIDATION.md).

OTA subtypes above describe the full collection. Browser-selected installs assign
consecutive subtypes; USB shortcuts keep their app identities.
