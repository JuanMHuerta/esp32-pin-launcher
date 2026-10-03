#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render README GIFs for Conway, Fluid, Miso and Maze with their firmware code."""

import argparse
import math
import os
from pathlib import Path
import subprocess
import tempfile

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
APPS = {
    "conways-pin": ((536, 240), ("life.c", "palette.c")),
    "fluid-pin": ((536, 240), ("fluid.c", "render.c")),
    "pet-pin": ((134, 60), ("pet.c", "paint.c")),
    "maze-pin": ((268, 120), ("maze.c", "paint.c")),
}


def render(app, seconds, fps):
    size, sources = APPS[app]
    app_dir = ROOT / "firmwares" / app
    count = round(seconds * fps)
    with tempfile.TemporaryDirectory(prefix=app + "-preview-") as directory:
        work = Path(directory)
        binary = work / "preview"
        subprocess.run(
            [
                os.environ.get("CC", "cc"),
                "-std=c11",
                "-O2",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I" + str(app_dir / "main"),
                str(ROOT / "tools/previews" / (app + ".c")),
                *(str(app_dir / "main" / source) for source in sources),
                "-lm",
                "-o",
                str(binary),
            ],
            check=True,
        )
        raw = work / "frames.rgb"
        with raw.open("wb") as output:
            subprocess.run([str(binary), str(count), str(fps)], stdout=output, check=True)
        frame_bytes = size[0] * size[1] * 3
        if raw.stat().st_size != frame_bytes * count:
            raise RuntimeError(f"{app}: renderer returned an incomplete frame sequence")
        frames = []
        with raw.open("rb") as data:
            for _ in range(count):
                frame = Image.frombytes("RGB", size, data.read(frame_bytes))
                frames.append(frame.resize((536, 240), Image.Resampling.NEAREST))
        sample = Image.new("RGB", (536, 240 * min(24, count)))
        for index in range(min(24, count)):
            sample.paste(frames[index * count // min(24, count)], (0, index * 240))
        palette = sample.quantize(colors=128, method=Image.Quantize.MEDIANCUT)
        indexed = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
        durations = [
            round((i + 1) * 100 / fps) * 10 - round(i * 100 / fps) * 10 for i in range(count)
        ]
        target = app_dir / "preview.gif"
        indexed[0].save(
            target,
            save_all=True,
            append_images=indexed[1:],
            duration=durations,
            loop=0,
            disposal=1,
            optimize=True,
        )
        print(f"{target.relative_to(ROOT)}: {count} frames, {target.stat().st_size:,} bytes")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", choices=["all", *APPS], nargs="?", default="all")
    parser.add_argument("--seconds", type=float, default=10)
    parser.add_argument("--fps", type=int, default=15)
    args = parser.parse_args()
    if not math.isfinite(args.seconds) or not 1 <= args.seconds <= 60 or not 1 <= args.fps <= 40:
        parser.error("duration must be 1..60 seconds and fps must be 1..40")
    for app in APPS if args.app == "all" else [args.app]:
        render(app, args.seconds, args.fps)


if __name__ == "__main__":
    main()
