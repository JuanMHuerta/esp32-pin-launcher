#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render a short deterministic preview using the same C scene as the device."""

from __future__ import annotations

import argparse
import os
import subprocess
import tempfile
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
WIDTH = 536
HEIGHT = 240
FRAME_BYTES = WIDTH * HEIGHT * 3


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=ROOT / "preview.gif")
    parser.add_argument("--still", type=Path, default=ROOT / "preview-scene.png")
    parser.add_argument("--seconds", type=float, default=16)
    parser.add_argument("--fps", type=int, default=15)
    parser.add_argument("--variant", type=int, default=0)
    parser.add_argument("--seed", type=lambda x: int(x, 0), default=0x81D2A93B)
    parser.add_argument(
        "--showcase", action="store_true", help="follow continuous arrivals, encounters and jumps"
    )
    parser.add_argument(
        "--event",
        type=int,
        default=1,
        help="-1=random, 0=cruise, 1=traffic, 2=alert, 3=planet, 4=jump, 5=nebula, 6=contact, 7=station, 8=convoy, 9=comet, 10=eclipse",
    )
    parser.add_argument("--movie", type=Path, help="also encode a full-color MP4")
    args = parser.parse_args()
    frame_count = round(args.seconds * args.fps)
    if frame_count < 1 or args.fps < 1:
        parser.error("duration and frame rate must be positive")

    source = ROOT / "main" / "scene.c"
    header = ROOT / "main" / "scene.h"
    harness = ROOT / "tools" / "preview.c"
    background = ROOT / "main" / "assets" / "cockpit.rgb565"
    compiler = os.environ.get("CC", "cc")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.still.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="wayfarer-preview-") as work:
        executable = Path(work) / "preview"
        raw_frames = Path(work) / "frames.rgb"
        subprocess.run(
            [
                compiler,
                "-std=c11",
                "-O2",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(header.parent),
                str(source),
                str(header.parent / "assets_generated.c"),
                str(header.parent / "exterior_generated.c"),
                str(harness),
                "-o",
                str(executable),
            ],
            check=True,
        )
        subprocess.run(
            [
                str(executable),
                str(background),
                str(raw_frames),
                str(frame_count),
                str(args.fps),
                str(-2 if args.showcase else args.event),
                str(args.variant),
                str(args.seed),
            ],
            check=True,
        )
        raw = raw_frames.read_bytes()
        expected = FRAME_BYTES * frame_count
        if len(raw) != expected:
            raise RuntimeError(f"renderer wrote {len(raw)} bytes; expected {expected}")
        frames = [
            Image.frombytes("RGB", (WIDTH, HEIGHT), raw[i * FRAME_BYTES : (i + 1) * FRAME_BYTES])
            for i in range(frame_count)
        ]
        if args.movie:
            args.movie.parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(
                [
                    "ffmpeg",
                    "-hide_banner",
                    "-loglevel",
                    "error",
                    "-y",
                    "-f",
                    "rawvideo",
                    "-pixel_format",
                    "rgb24",
                    "-video_size",
                    f"{WIDTH}x{HEIGHT}",
                    "-framerate",
                    str(args.fps),
                    "-i",
                    str(raw_frames),
                    "-c:v",
                    "libopenh264",
                    "-b:v",
                    "2M",
                    "-pix_fmt",
                    "yuv420p",
                    "-movflags",
                    "+faststart",
                    str(args.movie),
                ],
                check=True,
            )

    # Build one shared palette from the full animation so event colors survive.
    samples = frames[:: max(1, frame_count // 32)]
    sheet = Image.new("RGB", (WIDTH * 2, HEIGHT * ((len(samples) + 1) // 2)))
    for i, frame in enumerate(samples):
        sheet.paste(frame, ((i % 2) * WIDTH, (i // 2) * HEIGHT))
    palette_source = sheet.quantize(colors=224, method=Image.Quantize.MEDIANCUT)
    pal_frames = []
    for frame in frames:
        pal = frame.quantize(palette=palette_source, dither=Image.Dither.NONE)
        pal_frames.append(pal)
    args.still.parent.mkdir(parents=True, exist_ok=True)
    frames[frame_count // 2].save(args.still)
    # GIF durations use 10ms ticks; distribute rounding instead of speeding up
    # a 30fps animation by assigning every frame the same 30ms duration.
    durations = [
        round((i + 1) * 100 / args.fps) * 10 - round(i * 100 / args.fps) * 10
        for i in range(frame_count)
    ]
    pal_frames[0].save(
        args.output,
        save_all=True,
        append_images=pal_frames[1:],
        duration=durations,
        loop=0,
        optimize=True,
        disposal=1,
    )


if __name__ == "__main__":
    main()
