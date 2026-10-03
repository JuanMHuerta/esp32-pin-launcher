#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check publishable files for broken documentation links and missing previews."""

from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote, urlsplit

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
        if path.suffix in (".c", ".h", ".py", ".sh"):
            if "SPDX-License-Identifier: GPL-3.0-only" not in path.read_text():
                errors.append(f"{path.relative_to(ROOT)}: missing SPDX license identifier")
        if path.stat().st_size >= 50 * 1024 * 1024:
            errors.append(f"{path.relative_to(ROOT)}: file is too large for a normal Git checkout")
    readme = (ROOT / "README.md").read_text()
    menu = (ROOT / "main/main.c").read_text()
    subtypes = re.findall(r"ESP_PARTITION_SUBTYPE_APP_(OTA_\d+)", menu)
    expected = [image[1].upper() for image in app_layout.IMAGES[1:]]
    if subtypes != expected:
        errors.append("Launcher menu OTA order differs from the flash layout")
    switcher = (ROOT / "common/app_switcher.c").read_text()
    for source, name in [(menu, "APP_COUNT"), (switcher, "DEMO_APP_COUNT")]:
        match = re.search(r"\b" + name + r"\s*=\s*(\d+)", source)
        if not match or int(match[1]) != len(APPS):
            errors.append(f"{name} does not match the number of apps")
    for app in APPS:
        if not re.search(r"firmwares/" + re.escape(app) + r"/[^)]+\.gif", readme):
            errors.append(f"README.md: no GIF for {app}")
        if not (ROOT / "firmwares" / app / "README.md").is_file():
            errors.append(f"{app}: missing README")
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("Repository checks passed: local links, app GIFs, licenses and file sizes.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
