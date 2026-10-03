#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Preview CRT and THREE BODY using their actual portable firmware renderer."""

import argparse
import math
import os
from pathlib import Path
import subprocess
import tempfile
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
HARNESS = r"""
#include "scene.h"
#include <stdio.h>
#include <stdlib.h>
static uint16_t pixels[PIN_W * PIN_H];
#ifdef THREE_BODY
static orbit_t scene;
#else
static crt_t scene;
#endif
int main(int argc, char **argv)
{
    if (argc != 6) {
        return 2;
    }
    unsigned preset = (unsigned)atoi(argv[1]), fps = (unsigned)atoi(argv[2]);
    unsigned frames = (unsigned)atoi(argv[3]), start = (unsigned)atoi(argv[4]);
    FILE *out = fopen(argv[5], "wb");
    if (!out || !fps) {
        return 3;
    }
#ifdef THREE_BODY
    orbit_init(&scene, preset);
    for (unsigned t = 0; t < start;) {
        unsigned dt = start - t < 25 ? start - t : 25;
        orbit_step(&scene, dt);
        t += dt;
    }
#else
    crt_init(&scene, preset, preset & 1);
    crt_step(&scene, start);
#endif
    for (unsigned f = 0; f < frames; f++) {
#ifdef THREE_BODY
        orbit_paint(&scene, pixels);
#else
        crt_paint(&scene, pixels);
#endif
        for (unsigned i = 0; i < PIN_W * PIN_H; i++) {
            uint16_t c = pixels[i];
            unsigned char rgb[3] = {((c >> 11) & 31) * 255 / 31, ((c >> 5) & 63) * 255 / 63,
                                    (c & 31) * 255 / 31};
            if (fwrite(rgb, 1, 3, out) != 3) {
                return 4;
            }
        }
        unsigned dt = ((f + 1) * 1000 / fps) - (f * 1000 / fps);
#ifdef THREE_BODY
        for (unsigned t = 0; t < dt;) {
            unsigned h = dt - t < 25 ? dt - t : 25;
            orbit_step(&scene, h);
            t += h;
        }
#else
        crt_step(&scene, dt);
#endif
    }
    return fclose(out) ? 5 : 0;
}
"""


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("app", choices=["three-body-pin", "crt-pin"])
    ap.add_argument(
        "--preset", type=int, default=0, help="scenario: 0..2 for THREE BODY, 0..5 for CRT"
    )
    ap.add_argument("--seconds", type=float, default=15)
    ap.add_argument("--fps", type=int, default=15)
    ap.add_argument("--start", type=float, default=0)
    args = ap.parse_args()
    if args.preset not in range(3 if args.app == "three-body-pin" else 6):
        ap.error("preset is outside the selected app scenario range")
    if (
        not math.isfinite(args.seconds)
        or not math.isfinite(args.start)
        or not 0 < args.seconds <= 60
        or not 1 <= args.fps <= 40
        or not 0 <= args.start <= 3600
    ):
        ap.error("duration must be 0..60 seconds, fps 1..40 and start 0..3600 seconds")
    app = ROOT / "firmwares" / args.app
    count = max(1, round(args.seconds * args.fps))
    with tempfile.TemporaryDirectory(prefix=args.app + "-preview-") as temp:
        work = Path(temp)
        harness = work / "preview.c"
        harness.write_text(HARNESS)
        exe = work / "preview"
        raw = work / "frames.rgb"
        cmd = [
            os.environ.get("CC", "cc"),
            "-std=c11",
            "-O2",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I" + str(app / "main"),
            "-I" + str(ROOT / "common"),
            str(harness),
            str(app / "main/scene.c"),
            str(ROOT / "common/pin_gfx.c"),
            "-lm",
            "-o",
            str(exe),
        ]
        if args.app == "three-body-pin":
            cmd.insert(1, "-DTHREE_BODY")
        subprocess.run(cmd, check=True)
        subprocess.run(
            [
                str(exe),
                str(args.preset),
                str(args.fps),
                str(count),
                str(round(args.start * 1000)),
                str(raw),
            ],
            check=True,
        )
        data = raw.read_bytes()
        frame_bytes = 268 * 120 * 3
        if len(data) != frame_bytes * count:
            raise RuntimeError("renderer returned an incomplete frame sequence")
        frames = [
            Image.frombytes(
                "RGB", (268, 120), data[i * frame_bytes : (i + 1) * frame_bytes]
            ).resize((536, 240), Image.Resampling.NEAREST)
            for i in range(count)
        ]
    frames[count // 2].save(app / f"preview-{args.preset}.png")
    sample = Image.new("RGB", (536, 240 * min(32, len(frames))))
    for i, frame in enumerate(frames[:: max(1, len(frames) // 32)][:32]):
        sample.paste(frame, (0, i * 240))
    palette = sample.quantize(colors=224, method=Image.Quantize.MEDIANCUT)
    quantized = [f.quantize(palette=palette, dither=Image.Dither.NONE) for f in frames]
    durations = [
        round((i + 1) * 100 / args.fps) * 10 - round(i * 100 / args.fps) * 10 for i in range(count)
    ]
    quantized[0].save(
        app / f"preview-{args.preset}.gif",
        save_all=True,
        append_images=quantized[1:],
        loop=0,
        duration=durations,
        disposal=1,
    )
    print(f"Wrote {app}/preview-{args.preset}.png and .gif")


if __name__ == "__main__":
    main()
