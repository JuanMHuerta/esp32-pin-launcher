#!/usr/bin/env python3
"""Render and label every DUNGEON//SEED set piece and encounter phase."""
from pathlib import Path
import subprocess
import tempfile

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SCALE, WIDTH, HEIGHT, COLUMNS = 4, 134, 60, 5
ROOMS = ("ARRIVAL", "CORRIDOR", "TREASURE", "AMBUSH", "ELITE", "SHRINE", "EXIT")
ENEMIES = ("SKELETON", "SLIME", "BRUTE", "EYE")
BOSSES = ("WARDEN", "WYRM", "ELDRITCH")
PHASES = ("APPROACH", "TELEGRAPH", "LUNGE", "COUNTER", "DEFEAT")


def labels():
    yield from (f"CHAMBER  {room}  {PHASES[phase]}" if room in ("ARRIVAL", "TREASURE", "SHRINE")
                else f"CORRIDOR  {room}  {PHASES[phase]}"
                for room in ROOMS for phase in range(5))
    yield from (f"CORRIDOR  {enemy}  {PHASES[phase]}" for enemy in ENEMIES for phase in range(5))
    yield from (f"CHAMBER  {boss}  {PHASES[phase]}" for boss in BOSSES for phase in range(5))


with tempfile.TemporaryDirectory(prefix="dungeon-preview-") as temp:
    binary = Path(temp) / "preview"
    raw = Path(temp) / "preview.ppm"
    subprocess.run([
        "cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "main"),
        str(ROOT / "tools" / "preview.c"), str(ROOT / "main" / "dungeon.c"),
        str(ROOT / "main" / "paint.c"), str(ROOT / "main" / "assets_generated.c"), "-o", str(binary),
    ], check=True)
    with raw.open("wb") as output:
        subprocess.run([str(binary)], check=True, stdout=output)
    source = Image.open(raw).convert("RGB")
    rows = source.height // (HEIGHT * SCALE)
    caption_h = 14
    board = Image.new("RGB", (COLUMNS * WIDTH * SCALE, rows * (HEIGHT * SCALE + caption_h)), (9, 11, 14))
    draw = ImageDraw.Draw(board)
    for card, label in enumerate(labels()):
        col, row = card % COLUMNS, card // COLUMNS
        left, top = col * WIDTH * SCALE, row * (HEIGHT * SCALE + caption_h)
        crop = source.crop((col * WIDTH * SCALE, row * HEIGHT * SCALE,
                            (col + 1) * WIDTH * SCALE, (row + 1) * HEIGHT * SCALE))
        board.paste(crop, (left, top))
        draw.text((left + 3, top + HEIGHT * SCALE + 2), label, fill=(238, 241, 239))
    board.save(ROOT / "preview.png")
    print(f"wrote {ROOT / 'preview.png'} with {len(tuple(labels()))} labelled frames")
