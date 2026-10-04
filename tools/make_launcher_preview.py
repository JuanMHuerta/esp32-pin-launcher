#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render a GIF of the firmware launcher menu with its selection moving."""

from pathlib import Path
import os
import subprocess
import tempfile

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
WIDTH = 536
HEIGHT = 240
FRAME_COUNT = 10
FRAME_DURATION_MS = 600


def render():
    with tempfile.TemporaryDirectory(prefix="launcher-menu-preview-") as directory:
        binary = Path(directory) / "preview"
        raw = Path(directory) / "frames.rgb"
        subprocess.run(
            [
                os.environ.get("CC", "cc"),
                "-std=c11",
                "-O2",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I" + str(ROOT / "main"),
                str(ROOT / "tools/previews/launcher-menu.c"),
                str(ROOT / "main/menu_render.c"),
                "-o",
                str(binary),
            ],
            check=True,
        )
        with raw.open("wb") as output:
            subprocess.run([str(binary)], stdout=output, check=True)

        frame_bytes = WIDTH * HEIGHT * 3
        if raw.stat().st_size != frame_bytes * FRAME_COUNT:
            raise RuntimeError("launcher renderer returned an incomplete frame sequence")
        frames = []
        with raw.open("rb") as data:
            for _ in range(FRAME_COUNT):
                frames.append(Image.frombytes("RGB", (WIDTH, HEIGHT), data.read(frame_bytes)))

    palette_source = Image.new("RGB", (WIDTH, HEIGHT * FRAME_COUNT))
    for index, frame in enumerate(frames):
        palette_source.paste(frame, (0, index * HEIGHT))
    palette = palette_source.quantize(colors=64, method=Image.Quantize.MEDIANCUT)
    indexed = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
    target = ROOT / "main/menu-preview.gif"
    indexed[0].save(
        target,
        save_all=True,
        append_images=indexed[1:],
        duration=FRAME_DURATION_MS,
        loop=0,
        disposal=1,
        optimize=True,
    )
    print(f"{target.relative_to(ROOT)}: {FRAME_COUNT} frames, {target.stat().st_size:,} bytes")


if __name__ == "__main__":
    render()
