"""Generate previews from the same renderer used by the pin (Pillow + ffmpeg)."""
import math
from pathlib import Path
import subprocess
import tempfile
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="pin-preview-") as temp:
    temp = Path(temp)
    binary = temp / "preview"
    subprocess.run(["gcc", "-std=c11", "-O2", "-I", str(ROOT / "main"),
                    str(ROOT / "tools/preview.c"), str(ROOT / "main/renderer.c"),
                    "-lm", "-o", str(binary)], check=True)

    def frame(path, mood, t, x=0, y=0, roll=0, energy=0, pulse=-1):
        subprocess.run([str(binary), str(mood), str(t), str(path),
                        str(x), str(y), str(roll), str(energy), str(pulse),
                        "268", "120"], check=True)

    board = Image.new("RGB", (1072, 1512))
    for mood, name in enumerate(["ICE", "WARM WHITE", "SILVER"]):
        path = temp / f"palette{mood}.ppm"
        frame(path, mood, 2200)
        im = Image.open(path).resize((1072, 480))
        if mood == 0:
            im.save(ROOT / "preview.png")
        board.paste(im, (0, mood * 504 + 24))
        ImageDraw.Draw(board).text((14, mood * 504 + 5), name, fill="white")
    board.save(ROOT / "preview-palettes.png")
    for moving, output in [(False, "preview.gif"), (True, "preview-motion.gif")]:
        folder = temp / output
        folder.mkdir()
        for i in range(150):
            t = i * 67
            if moving:
                frame(folder / f"frame{i:03}.ppm", 0, t,
                      .65 * math.sin(t / 1600), .4 * math.sin(t / 2100),
                      36 * math.sin(t / 2200), .4,
                      t - 5500 if 5500 <= t <= 7400 else -1)
            else:
                frame(folder / f"frame{i:03}.ppm", 0, t)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", "15",
                        "-i", str(folder / "frame%03d.ppm"), "-filter_complex",
                        "[0:v]split[a][b];[a]palettegen=max_colors=128[p];"
                        "[b][p]paletteuse=dither=bayer:bayer_scale=3",
                        "-loop", "0", str(ROOT / output)], check=True)
