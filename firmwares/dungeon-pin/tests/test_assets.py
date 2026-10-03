#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Validate every consumed atlas, material role, pose and generated artifact."""

import hashlib
import subprocess
import sys
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import compile_assets as assets  # noqa: E402 — asset tools are local to this app.

STONE = set(range(1, 11))
ENVIRONMENT = {name for name in assets.PROPS if name.startswith(("WALL_", "FLOOR_", "CEILING_"))}
assert len(assets.PALETTE) == 64
assert assets.PALETTE[0][3] == 0
assert all(color[3] == 255 for color in assets.PALETTE[1:])
assert len(set(assets.PALETTE)) == len(assets.PALETTE)
sheets = {}
for name in {entry[1] for entry in assets.ENTRIES}:
    image = Image.open(ROOT / "assets" / "source" / name).convert("RGBA")
    assert image.width % assets.CELL == 0 and image.height % assets.CELL == 0, name
    pixels = tuple(
        image.get_flattened_data() if hasattr(image, "get_flattened_data") else image.getdata()
    )
    assert set(pixels) <= set(assets.PALETTE), name
    assert any(p[3] == 0 for p in pixels), f"{name}: transparent key unused"
    sheets[name] = image

tiles = {}
for name, sheet, col, row in assets.ENTRIES:
    image = sheets[sheet]
    size = assets.CELL
    assert (col + 1) * size <= image.width and (row + 1) * size <= image.height, name
    tile = image.crop((col * size, row * size, (col + 1) * size, (row + 1) * size))
    pixels = tuple(
        tile.get_flattened_data() if hasattr(tile, "get_flattened_data") else tile.getdata()
    )
    assert sum(p[3] != 0 for p in pixels) > 40, f"{name}: empty or trivial"
    if name in ENVIRONMENT:
        assert {assets.nearest(p) for p in pixels} <= STONE, f"{name}: non-stone wall color"
    tiles[name] = tile

for kind in (
    "SKELETON",
    "SLIME",
    "BRUTE",
    "EYE",
    "GOBLIN",
    "SPIDER",
    "WRAITH",
    "GOLEM",
    "WARDEN",
    "WYRM",
    "ELDRITCH",
    "LICH",
    "HYDRA",
    "MINOTAUR",
):
    frames = [tiles[f"{kind}_{n}"] for n in range(4)]
    assert len({f.tobytes() for f in frames}) == 4, f"{kind}: repeated pose"
    assert all(len(f.getcolors()) >= 9 for f in frames), f"{kind}: insufficient material detail"
    # Defeated poses really collapse; a flash drawn over the upright body cannot pass.
    standing, defeated = frames[0].getbbox(), frames[3].getbbox()
    assert defeated[1] > standing[1] + 7, f"{kind}: defeat silhouette is still standing"
    for f in frames:
        assert f.getbbox()[0] > 0 and f.getbbox()[2] < size, f"{kind}: horizontal clipping"

assert tiles["CHEST"].tobytes() != tiles["CHEST_OPEN"].tobytes()
assert tiles["SWORD"].tobytes() != tiles["SWORD_SWING"].tobytes()
assert tiles["STAFF"].tobytes() != tiles["STAFF_CAST"].tobytes()
for ready, attack in zip(assets.EXTRA_GEAR[::2], assets.EXTRA_GEAR[1::2]):
    assert tiles[ready].tobytes() != tiles[attack].tobytes()
for name in ("SWORD", "STAFF") + assets.EXTRA_GEAR:
    hand = tiles[name].crop((0, 40, 64, 64))
    pixels = tuple(
        hand.get_flattened_data() if hasattr(hand, "get_flattened_data") else hand.getdata()
    )
    assert sum(p in assets.PALETTE[45:50] for p in pixels) > 12, (
        f"{name}: no readable gripping hand"
    )
    tile = tiles[name]
    box = tile.getbbox()
    top = [
        (x, y)
        for y in range(box[1], box[1] + (box[3] - box[1]) // 3)
        for x in range(64)
        if tile.getpixel((x, y))[3]
    ]
    bottom = [(x, y) for y in range(44, 62) for x in range(64) if tile.getpixel((x, y))[3]]
    assert top and bottom
    assert sum(x for x, y in bottom) / len(bottom) > 33, (
        f"{name}: forearm is not entering from right"
    )
    assert sum(x for x, y in top) / len(top) < sum(x for x, y in bottom) / len(bottom) - 6, (
        f"{name}: weapon points away from center"
    )
roles = {
    "MOSS": STONE | set(range(50, 55)),
    "EMBER": STONE | set(range(39, 45)) | set(range(55, 58)),
    "FROST": STONE | set(range(21, 26)) | {61, 62, 63},
}
for biome, allowed in roles.items():
    for surface in ("WALL", "FLOOR"):
        tile = tiles[f"{biome}_{surface}"]
        pixels = (
            tile.get_flattened_data() if hasattr(tile, "get_flattened_data") else tile.getdata()
        )
        assert {assets.nearest(p) for p in pixels} <= allowed
        assert tile.tobytes() != tiles["WALL_NEAR" if surface == "WALL" else "FLOOR_NEAR"].tobytes()
files = (ROOT / "main" / "assets_generated.c", ROOT / "main" / "assets_generated.h")
before = [hashlib.sha256(p.read_bytes()).digest() for p in files]
subprocess.run([sys.executable, str(ROOT / "tools" / "compile_assets.py")], check=True)
assert before == [hashlib.sha256(p.read_bytes()).digest() for p in files], "stale generated assets"
assert len(assets.ENTRIES) == 116
assert len(assets.ENTRIES) * size * size == 475136
print(
    "all 116 assets, inward right-hand grips, biome materials, defeat silhouettes and reproducibility passed"
)
