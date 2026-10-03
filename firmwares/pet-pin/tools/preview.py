# SPDX-License-Identifier: GPL-3.0-only
"""Render the actual C firmware at 30 fps, with a contact sheet for each state."""

from pathlib import Path
import subprocess
import tempfile
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "artifacts"
STATES = ["idle", "walk", "sniff", "eat", "sleep", "love", "play", "surprise", "wave"]


def main():
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
        sheet = Image.new("RGB", (536 * 3, 264 * 9), "#090f1c")
        all_frames = []
        for state, name in enumerate(STATES):
            path = temp / "frames.rgb"
            subprocess.run([str(exe), str(state), "150", str(state * 36000), str(path)], check=True)
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
            if state == 0:
                frames[60].save(OUT / "preview.png")
        sheet.save(OUT / "contact-sheet.png")
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
    print(f"Rendered 1,350 frames across 9 animations into {OUT}")


if __name__ == "__main__":
    main()
