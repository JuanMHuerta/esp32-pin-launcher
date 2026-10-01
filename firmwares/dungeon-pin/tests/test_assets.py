#!/usr/bin/env python3
"""Asset-converter checks: atlas bounds, palette roles, alpha key, and stability."""
import hashlib
import subprocess
import sys
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import compile_assets as assets

STONE = {1, 2, 3, 4, 5, 6, 13, 15}
ENVIRONMENT = {
    "WALL_NEAR", "WALL_MID", "WALL_FAR", "CEILING_NEAR", "CEILING_MID", "CEILING_FAR",
    "FLOOR_NEAR", "FLOOR_MID", "FLOOR_FAR", "ARCH", "DOOR", "BANNER", "CHAIN",
    "SKULLS", "RUBBLE", "ROOTS", "CHEST", "GATE",
}
ACCENTS = {"TORCH": {7, 8}, "SHRINE": {12}}

assert assets.PALETTE[0][3] == 0
assert all(color[3] == 255 for color in assets.PALETTE[1:])
# Environment lightness is an ordered neutral stone ramp, never a brown wall
# palette.  Colour remains isolated to feedback and magical/gameplay marks.
stone_ramp = [assets.PALETTE[i] for i in (1, 2, 3, 4, 5, 6, 13)]
assert all(max(rgb[:3]) - min(rgb[:3]) <= 28 for rgb in stone_ramp)
assert [sum(rgb[:3]) for rgb in stone_ramp] == sorted(sum(rgb[:3]) for rgb in stone_ramp)

for name in {entry[1] for entry in assets.ENTRIES}:
    image = Image.open(ROOT / "assets" / "source" / name).convert("RGBA")
    assert image.width % 32 == 0 and image.height % 32 == 0, name
    pixels = image.get_flattened_data() if hasattr(image, "get_flattened_data") else image.getdata()
    for pixel in pixels:
        assert pixel in assets.PALETTE, (name, pixel)
    assert any(pixel[3] == 0 for pixel in pixels), f"{name}: transparent key unused"

for name, sheet, col, row in assets.ENTRIES:
    image = Image.open(ROOT / "assets" / "source" / sheet).convert("RGBA")
    assert (col + 1) * 32 <= image.width and (row + 1) * 32 <= image.height, name
    assert any(image.getpixel((col * 32 + x, row * 32 + y))[3] for y in range(32) for x in range(32)), name
    indices = {assets.nearest(image.getpixel((col * 32 + x, row * 32 + y))) for y in range(32) for x in range(32)}
    if name in ENVIRONMENT:
        assert indices <= STONE | {0}, f"{name}: non-stone environment colour {indices - STONE}"
    if name in ACCENTS:
        assert indices <= STONE | ACCENTS[name] | {0}, f"{name}: unexpected accent colour"

before = hashlib.sha256((ROOT / "main" / "assets_generated.c").read_bytes()).digest()
subprocess.run([sys.executable, str(ROOT / "tools" / "compile_assets.py")], check=True)
after = hashlib.sha256((ROOT / "main" / "assets_generated.c").read_bytes()).digest()
assert before == after, "asset generation must be deterministic"
assert len(assets.ENTRIES) == 56
assert len(assets.ENTRIES) * 512 == 28672
assert (ROOT / "main" / "assets_generated.c").stat().st_size < 230000
print("asset atlas and converter checks passed")
