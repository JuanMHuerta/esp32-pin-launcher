#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render and label every DUNGEON//SEED set piece and encounter phase."""

from pathlib import Path
import subprocess
import tempfile

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SCALE, WIDTH, HEIGHT, COLUMNS = 2, 268, 120, 5
ROOMS = (
    "ARRIVAL",
    "PASSAGE",
    "TREASURE",
    "AMBUSH",
    "ELITE",
    "SHRINE",
    "STAIRS",
    "JUNCTION",
    "CRYPT",
    "LIBRARY",
    "ARMORY",
    "GROVE",
)
ENEMIES = ("SKELETON", "SLIME", "BRUTE", "EYE", "GOBLIN", "SPIDER", "WRAITH", "GOLEM")
BOSSES = ("WARDEN", "WYRM", "ELDRITCH", "LICH", "HYDRA", "MINOTAUR")
BIOMES = ("CATACOMBS", "OVERGROWN RUINS", "EMBER KEEP", "FROZEN VAULT")
LAYOUTS = ("ENTRY", "CROSSROADS", "SIDE ROOM", "LOOK BACK", "PILLARS", "BOSS VAULT")
WEAPONS = ("SWORD", "STAFF", "AXE", "MACE", "DAGGER", "CROSSBOW")
PHASES = ("APPROACH", "TELEGRAPH", "LUNGE", "COUNTER", "DEFEAT")


def labels():
    yield from (f"{room}  {PHASES[phase]}" for room in ROOMS for phase in range(5))
    yield from (f"{enemy}  {PHASES[phase]}" for enemy in ENEMIES for phase in range(5))
    yield from (f"{boss}  {PHASES[phase]}" for boss in BOSSES for phase in range(5))
    yield from (f"{biome}  {layout}" for biome in BIOMES for layout in LAYOUTS)
    yield from (
        f"{weapon} {phase}"
        for weapon in WEAPONS
        for phase in ("WINDUP", "STRIKE", "RECOVER", "READY")
    )
    yield "TITLE"
    yield from (f"DOOR OPEN {phase}" for phase in ("CLOSED", "HALF", "CLEAR"))
    yield from (f"DOOR CLOSE {phase}" for phase in ("CLEAR", "HALF", "CLOSED"))


with tempfile.TemporaryDirectory(prefix="dungeon-preview-") as temp:
    binary = Path(temp) / "preview"
    raw = Path(temp) / "preview.ppm"
    subprocess.run(
        [
            "cc",
            "-std=c11",
            "-O2",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I",
            str(ROOT / "main"),
            str(ROOT / "tools" / "preview.c"),
            str(ROOT / "main" / "dungeon.c"),
            str(ROOT / "main" / "paint.c"),
            str(ROOT / "main" / "assets_generated.c"),
            "-lm",
            "-o",
            str(binary),
        ],
        check=True,
    )
    with raw.open("wb") as output:
        subprocess.run([str(binary)], check=True, stdout=output)
    source = Image.open(raw).convert("RGB")
    rows = source.height // (HEIGHT * SCALE)
    caption_h = 14
    board = Image.new(
        "RGB", (COLUMNS * WIDTH * SCALE, rows * (HEIGHT * SCALE + caption_h)), (9, 11, 14)
    )
    draw = ImageDraw.Draw(board)
    for card, label in enumerate(labels()):
        col, row = card % COLUMNS, card // COLUMNS
        left, top = col * WIDTH * SCALE, row * (HEIGHT * SCALE + caption_h)
        crop = source.crop(
            (
                col * WIDTH * SCALE,
                row * HEIGHT * SCALE,
                (col + 1) * WIDTH * SCALE,
                (row + 1) * HEIGHT * SCALE,
            )
        )
        board.paste(crop, (left, top))
        draw.text((left + 3, top + HEIGHT * SCALE + 2), label, fill=(238, 241, 239))
    board.save(ROOT / "preview.png")
    # A smaller selection remains readable at native display size.
    featured = (130, 137, 144, 149, 61, 76, 81, 86, 91, 96, 101, 111, 116, 121, 126, 175)
    showcase = Image.new("RGB", (4 * WIDTH * SCALE, 4 * (HEIGHT * SCALE + caption_h)), (8, 10, 16))
    for n, card in enumerate(featured):
        left, top = (n % 4) * WIDTH * SCALE, (n // 4) * (HEIGHT * SCALE + caption_h)
        x, y = card % COLUMNS, card // COLUMNS
        showcase.paste(
            source.crop(
                (
                    x * WIDTH * SCALE,
                    y * HEIGHT * SCALE,
                    (x + 1) * WIDTH * SCALE,
                    (y + 1) * HEIGHT * SCALE,
                )
            ),
            (left, top),
        )
        ImageDraw.Draw(showcase).text(
            (left + 3, top + HEIGHT * SCALE + 2), tuple(labels())[card], fill=(207, 212, 202)
        )
    showcase.save(ROOT / "preview-showcase.png")
    for name, cards, columns in (
        ("preview-biomes.png", range(130, 154), 6),
        ("preview-weapons.png", range(154, 178), 4),
        ("preview-axe.png", range(162, 166), 4),
    ):
        cards = tuple(cards)
        gallery = Image.new(
            "RGB",
            (columns * WIDTH * SCALE, (len(cards) // columns) * (HEIGHT * SCALE + caption_h)),
            (8, 10, 16),
        )
        for n, card in enumerate(cards):
            left, top = (n % columns) * WIDTH * SCALE, (n // columns) * (HEIGHT * SCALE + caption_h)
            x, y = card % COLUMNS, card // COLUMNS
            gallery.paste(
                source.crop(
                    (
                        x * WIDTH * SCALE,
                        y * HEIGHT * SCALE,
                        (x + 1) * WIDTH * SCALE,
                        (y + 1) * HEIGHT * SCALE,
                    )
                ),
                (left, top),
            )
            ImageDraw.Draw(gallery).text(
                (left + 3, top + HEIGHT * SCALE + 2), tuple(labels())[card], fill=(207, 212, 202)
            )
        gallery.save(ROOT / name)
    with raw.open("wb") as output:
        animated = subprocess.run(
            [str(binary), "--animate"], check=True, stdout=output, stderr=subprocess.PIPE, text=True
        )
    animation = Image.open(raw).convert("RGB")
    frames = [
        animation.crop((0, n * HEIGHT, WIDTH, (n + 1) * HEIGHT)).resize(
            (WIDTH * SCALE, HEIGHT * SCALE), Image.Resampling.NEAREST
        )
        for n in range(animation.height // HEIGHT)
    ]
    frames[0].save(
        ROOT / "preview-motion.gif",
        save_all=True,
        append_images=frames[1:],
        duration=[120 if n % 2 == 0 else 130 for n in range(len(frames))],
        loop=0,
        optimize=True,
    )
    trace = [line.split(",") for line in animated.stderr.splitlines()]
    timeline = Image.new("RGB", (4 * WIDTH * SCALE, 4 * (HEIGHT * SCALE + caption_h)), (8, 10, 16))
    assert any(row[1] == "WALK" for row in trace) and any(row[1] == "TURN" for row in trace)
    assert any(int(row[10]) >= 3 for row in trace), "autoplay did not show multi-hit combat"
    for n, frame in enumerate(range(0, len(frames), len(frames) // 16)):
        left, top = (n % 4) * WIDTH * SCALE, (n // 4) * (HEIGHT * SCALE + caption_h)
        timeline.paste(frames[frame], (left, top))
        _, state, room, boss, phase, biome, is_boss, hp, mp, enemy_hp, hits = trace[frame]
        label = f"{frame * 0.125:04.1f}s {BIOMES[int(biome)]} {boss if is_boss == '1' else room} {PHASES[int(phase)]}"
        ImageDraw.Draw(timeline).text(
            (left + 3, top + HEIGHT * SCALE + 2), label, fill=(207, 212, 202)
        )
    timeline.save(ROOT / "preview-run.png")
    with raw.open("wb") as output:
        combat = subprocess.run(
            [str(binary), "--combat"], check=True, stdout=output, stderr=subprocess.PIPE, text=True
        )
    combat_source = Image.open(raw).convert("RGB")
    combat_frames = [
        combat_source.crop((0, n * HEIGHT, WIDTH, (n + 1) * HEIGHT)).resize(
            (WIDTH * SCALE, HEIGHT * SCALE), Image.Resampling.NEAREST
        )
        for n in range(combat_source.height // HEIGHT)
    ]
    combat_frames[0].save(
        ROOT / "preview-combat.gif",
        save_all=True,
        append_images=combat_frames[1:],
        duration=[120 if n % 2 == 0 else 130 for n in range(len(combat_frames))],
        loop=0,
    )
    contact = Image.new("RGB", (4 * WIDTH * SCALE, 2 * (HEIGHT * SCALE + caption_h)), (8, 10, 16))
    combat_trace = [line.split(",") for line in combat.stderr.splitlines()]
    for n, index in enumerate((0, 10, 19, 29, 38, 47, 55, 62)):
        left, top = (n % 4) * WIDTH * SCALE, (n // 4) * (HEIGHT * SCALE + caption_h)
        contact.paste(combat_frames[index], (left, top))
        row = combat_trace[index]
        label = f"{index * 0.125:.2f}s HP {row[7]} MP {row[8]} ENEMY HP {row[9]} HITS {row[10]}"
        ImageDraw.Draw(contact).text(
            (left + 3, top + HEIGHT * SCALE + 2), label, fill=(207, 212, 202)
        )
    contact.save(ROOT / "preview-combat.png")
    print(f"wrote {ROOT / 'preview.png'} with {len(tuple(labels()))} labelled frames")
