# SPDX-License-Identifier: GPL-3.0-only
"""Render the actual C firmware at 30 fps, with a contact sheet for each state."""

from pathlib import Path
import argparse
import subprocess
import tempfile
from PIL import Image, ImageDraw

from states import STATES

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "artifacts"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--publish", action="store_true", help="update checked-in PNG previews")
    args = parser.parse_args()
    OUT.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="miso-preview-") as temp:
        temp = Path(temp)
        exe = temp / "preview"
        subprocess.run(
            [
                "gcc",
                "-std=c11",
                "-O2",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "main"),
                str(ROOT / "tools/preview.c"),
                str(ROOT / "main/pet.c"),
                str(ROOT / "main/paint.c"),
                "-lm",
                "-o",
                str(exe),
            ],
            check=True,
        )
        sheet = Image.new("RGB", (536 * 3, 264 * len(STATES)), "#090f1c")
        poses = Image.new("RGB", (268 * 4, 140 * 4), "#090f1c")
        all_frames = []
        for state, name in enumerate(STATES):
            path = temp / "frames.rgb"
            subprocess.run(
                [str(exe), str(state), "150", str((state % 4) * 90000), str(path)], check=True
            )
            raw = path.read_bytes()
            frames = []
            for i in range(150):
                im = Image.frombytes("RGB", (134, 60), raw[i * 24120 : (i + 1) * 24120])
                frames.append(im.resize((536, 240), Image.Resampling.NEAREST))
            for j, i in enumerate([15, 60, 120]):
                sheet.paste(frames[i], (j * 536, state * 264 + 24))
                ImageDraw.Draw(sheet).text(
                    (j * 536 + 10, state * 264 + 6),
                    f"{name.upper()} / {i / 30:.1f}s",
                    fill="#c5ddcf",
                )
            frames[0].save(
                OUT / f"{name}.gif",
                save_all=True,
                append_images=frames[1:],
                duration=[30, 30, 40] * 50,
                loop=0,
                optimize=False,
            )
            all_frames.extend(frames)
            # Half-size review sheet approximates a more distant viewing size.
            px, py = state % 4 * 268, state // 4 * 140
            poses.paste(frames[60].resize((268, 120), Image.Resampling.NEAREST), (px, py + 20))
            ImageDraw.Draw(poses).text((px + 8, py + 4), name.upper(), fill="#c5ddcf")
            if state == 0:
                frames[60].save(OUT / "preview.png")
        sheet.save(OUT / "contact-sheet.png")
        poses.save(OUT / "preview-actions.png")
        cycle = Image.new("RGB", (1072, 480))
        for i in range(4):
            path = temp / "cycle.rgb"
            subprocess.run([str(exe), "0", "60", str(i * 90000), str(path)], check=True)
            im = Image.frombytes("RGB", (134, 60), path.read_bytes()[-24120:])
            cycle.paste(
                im.resize((536, 240), Image.Resampling.NEAREST), (i % 2 * 536, i // 2 * 240)
            )
        cycle.save(OUT / "preview-day-night.png")
        if args.publish:
            for name in ["preview.png", "preview-day-night.png", "preview-actions.png"]:
                (ROOT / name).write_bytes((OUT / name).read_bytes())
        process = subprocess.Popen(
            [
                "ffmpeg",
                "-y",
                "-loglevel",
                "error",
                "-f",
                "rawvideo",
                "-pixel_format",
                "rgb24",
                "-video_size",
                "536x240",
                "-framerate",
                "30",
                "-i",
                "-",
                "-an",
                "-c:v",
                "libvpx-vp9",
                "-pix_fmt",
                "yuv420p",
                "-crf",
                "20",
                "-b:v",
                "0",
                str(OUT / "all-animations.webm"),
            ],
            stdin=subprocess.PIPE,
        )
        for im in all_frames:
            process.stdin.write(im.tobytes())
        process.stdin.close()
        if process.wait():
            raise RuntimeError("ffmpeg failed")
    print(f"Rendered {len(all_frames):,} frames across {len(STATES)} animations into {OUT}")


if __name__ == "__main__":
    main()
