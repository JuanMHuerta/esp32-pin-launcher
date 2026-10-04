#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check publishable files for broken documentation links and missing previews."""

from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote, urlsplit

from PIL import Image

import app_layout

ROOT = Path(__file__).resolve().parents[1]
APPS = tuple(Path(image[2]).parts[1] for image in app_layout.IMAGES[1:])


def main():
    result = subprocess.run(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        cwd=ROOT,
        check=True,
        capture_output=True,
    )
    files = {ROOT / name for name in result.stdout.decode().split("\0") if name}
    errors = []
    for path in sorted(files):
        if not path.is_file():
            continue
        if path.suffix == ".md":
            content = re.sub(r"```.*?```", "", path.read_text(), flags=re.S)
            for target in re.findall(r"\]\(([^)]+)\)", content):
                target = target.strip("<>")
                if urlsplit(target).scheme or target.startswith("#"):
                    continue
                local = unquote(target.split("#", 1)[0])
                if local and not (path.parent / local).exists():
                    errors.append(f"{path.relative_to(ROOT)}: missing link target {target}")
        if path.suffix in (".c", ".h", ".py", ".sh", ".js", ".mjs", ".css", ".html"):
            if "SPDX-License-Identifier: GPL-3.0-only" not in path.read_text():
                errors.append(f"{path.relative_to(ROOT)}: missing SPDX license identifier")
        if path.stat().st_size >= 50 * 1024 * 1024:
            errors.append(f"{path.relative_to(ROOT)}: file is too large for a normal Git checkout")
    for guide in (
        "README",
        "CONTRIBUTING",
        "NOTICE",
        "documentation/README",
        "documentation/SD_CARD_FILE_TOOL",
        "documentation/WEB_FLASHER",
        "documentation/AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91",
        "documentation/WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91_VALIDATION",
        "documentation/ESP32_DEVICE_LESSONS",
        "documentation/REPOSITORY_VALIDATION",
        "documentation/FLASH_LAYOUT_VALIDATION",
        "documentation/SCENE_VALIDATION",
        "documentation/APP_VALIDATION",
        "documentation/APP_IDEAS",
        "firmwares/dungeon-pin/assets/ART_DIRECTION",
        "firmwares/wayfarer-pin/assets/ART_DIRECTION",
    ):
        for suffix in (".md", ".es.md"):
            path = guide + suffix
            if suffix == ".es.md" and guide in ("README", "CONTRIBUTING", "NOTICE"):
                path = f"readmes/{path}"
            if not (ROOT / path).is_file():
                errors.append(f"{path}: missing language version")
    menu = (ROOT / "main/main.c").read_text()
    catalog = re.findall(r"\{\"[^\"\n]+\", \"[^\"\n]+\", \"([^\"\n]+)\", '([1-9])',", menu)
    expected = [(image[0], str(index)) for index, image in enumerate(app_layout.IMAGES[1:], 1)]
    if catalog != expected:
        errors.append("Launcher catalog order or shortcuts differ from the flash layout")
    match = re.search(r"\bAPP_COUNT\s*=\s*(\d+)", menu)
    if not match or int(match[1]) != len(APPS):
        errors.append("APP_COUNT does not match the number of apps")
    previews = set()
    for suffix, relative_path in (
        (".md", "README.md"),
        (".es.md", "readmes/README.es.md"),
    ):
        readme_path = ROOT / relative_path
        if not readme_path.is_file():
            continue
        readme = readme_path.read_text()
        for app in APPS:
            matches = re.findall(r"firmwares/" + re.escape(app) + r"/[^)]+\.gif", readme)
            if not matches:
                errors.append(f"{readme_path.name}: no GIF for {app}")
            previews.update(matches)
            if not (ROOT / "firmwares" / app / ("README" + suffix)).is_file():
                errors.append(f"{app}: missing README{suffix}")
    for preview in sorted(previews):
        try:
            with Image.open(ROOT / preview) as gif:
                if gif.size != (536, 240) or gif.n_frames < 2:
                    errors.append(f"{preview}: expected animated 536 × 240 preview")
                for frame in range(gif.n_frames):
                    gif.seek(frame)
                    if gif.info.get("duration", 0) <= 0:
                        errors.append(f"{preview}: frame {frame} has no playback duration")
                        break
        except (OSError, ValueError) as error:
            errors.append(f"{preview}: {error}")
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print(
        "Repository checks passed: bilingual guides, links, animated app GIFs, licenses and sizes."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
