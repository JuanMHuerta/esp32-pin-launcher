# Dungeon artwork

The renderer uses a 268 × 120 canvas scaled 2× to the display. Sprite sources use
64 × 64 cells, a shared 64-color palette and binary alpha. Transparent pixels use
palette index zero. Keep source sheets on that palette before compiling them.

## Sources

`source/` contains the sheets consumed by `tools/compile_assets.py`:

- `actors.png`, `actors-extra.png`: ordinary enemies, four poses each.
- `bosses.png`, `bosses-extra.png`: bosses, four poses each.
- `gear.png`, `gear-extra.png`: earlier weapon sheets retained as editable sources.
- `gear-right.png`, `gear-extra-right.png`, `axe-right.png`: right-handed weapons.
- `tiles-and-props.png`, `biomes-and-props.png`, `weapons-and-effects.png`:
  architecture, props, effects and weapon details.

`masters/` retains the transparent, higher-resolution character and weapon
images. These were generated with OpenAI's image generation tool. Native-size
architecture, props and effects are drawn by `tools/make_source_assets.py`.

The four biomes use cold stone, mossy ruins, ember basalt and blue ice. Torchlight
is warm amber; steel and bone stay light enough to read against the walls.
Weapons enter from the lower-right. Ready and attack poses should preserve the
same grip, proportions and material colors.

## Rebuild

Run these commands from `firmwares/dungeon-pin`:

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
./tools/test.sh
```

The importer extracts separate alpha-connected silhouettes and uses one scale
per animation row. It rejects touching or missing sprites. Keep padding between
master cells so an import cannot merge neighboring poses. Defeated creatures
should have a distinct collapsed silhouette. The asset tests check palette,
pose bounds, material colors, weapon direction and reproducible generated C.
