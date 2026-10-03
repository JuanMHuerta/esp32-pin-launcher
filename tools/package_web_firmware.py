#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Package a freshly built launcher/app collection for the static web flasher."""

import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile

import app_layout

APP_DETAILS = (
    ("Conway", "Game of Life with touch-placed patterns.", "preview.gif"),
    ("Fluid", "Water that moves as you tilt your pin.", "preview.gif"),
    ("Miso", "A woodland pet that reacts to touch and motion.", "preview.gif"),
    ("Lumen", "Fly through a constellation of stars.", "preview.gif"),
    ("Dungeon", "An autonomous dungeon crawl and combat.", "preview-combat.gif"),
    ("3D Maze", "Explore a freshly generated first-person maze.", "preview.gif"),
    ("Wayfarer", "A cozy cockpit with passing worlds and traffic.", "preview.gif"),
    ("Three Body", "Three bodies, gravity, and glowing orbit trails.", "preview-0.gif"),
    ("CRT", "A fictional station console in phosphor green.", "preview-0.gif"),
)


def source_files(root):
    if (root / ".git").exists():
        result = subprocess.run(
            ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
            cwd=root,
            capture_output=True,
            check=True,
        )
        return sorted(set(name for name in result.stdout.decode().split("\0") if name))
    # Source archives have no Git metadata. Exclude the outputs of documented build commands.
    ignored_dirs = {
        ".git",
        ".venv",
        ".ruff_cache",
        "__pycache__",
        "build",
        "managed_components",
        "artifacts",
        ".source-git-metadata",
        "node_modules",
        "dist",
        "test-results",
        "playwright-report",
    }
    names = []
    for directory, children, files in os.walk(root):
        children[:] = [
            name for name in children if name not in ignored_dirs and not name.startswith("build-")
        ]
        for name in files:
            path = Path(directory) / name
            relative = path.relative_to(root)
            if name in {"sdkconfig", "sdkconfig.old", "core"} or path.suffix in {
                ".pyc",
                ".ppm",
                ".tmp",
            }:
                continue
            if relative.parts[:2] == ("web", "public"):
                continue
            names.append(str(relative))
    return sorted(names)


def package_source(root, output):
    """Archive publication files, including working changes, without build outputs."""
    names = source_files(root)
    path = output / "source.tar.gz"
    with path.open("wb") as target:
        with gzip.GzipFile(fileobj=target, mode="wb", filename="", mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w|") as archive:
                for name in names:
                    source = root / name
                    if not source.is_file() or source.resolve().is_relative_to(output.resolve()):
                        continue
                    info = tarfile.TarInfo(f"multi-pin-launcher/{name}")
                    info.size = source.stat().st_size
                    info.mode = 0o755 if source.stat().st_mode & 0o111 else 0o644
                    with source.open("rb") as content:
                        archive.addfile(info, content)
    data = path.read_bytes()
    return {"file": path.name, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def package_licenses(root, output):
    description = json.loads((root / "build/project_description.json").read_text())
    idf = Path(description["idf_path"])
    sources = [("ESP-IDF 5.5.1", idf / "LICENSE")]
    directories = [("ESP-IDF components", idf / "components")]
    for path in description["build_component_paths"]:
        directory = Path(path)
        if "managed_components" in directory.parts:
            directories.append((directory.name, directory))
    for label, directory in directories:
        for path in sorted(directory.rglob("*")):
            if path.is_file() and path.name.upper().startswith(("LICENSE", "COPYING", "NOTICE")):
                sources.append((f"{label}/{path.relative_to(directory)}", path))
    texts = [f"{label}\n{'=' * len(label)}\n{path.read_text()}" for label, path in sources]
    (output / "FIRMWARE_LICENSES.txt").write_text("\n\n".join(texts))
    (output / "SOURCE.txt").write_text(
        "The matching project source is in source.tar.gz. Extract it and follow README.md\n"
        "or README.es.md to build. Build outputs and downloaded dependencies are excluded.\n\n"
        "Firmware dependencies and their sources:\n"
        "ESP-IDF v5.5.1: https://github.com/espressif/esp-idf/tree/v5.5.1\n"
        "Clone with git clone --recursive --branch v5.5.1 https://github.com/espressif/esp-idf.git\n"
        "Display driver and CMake utilities: manifests and dependencies.lock pin the versions;\n"
        "ESP-IDF's component manager downloads their source from https://components.espressif.com\n"
        "The web dependencies are pinned in web/package-lock.json and installed with npm ci.\n"
        "Firmware license texts are in FIRMWARE_LICENSES.txt; web licenses are in\n"
        "THIRD_PARTY_LICENSES.txt. Upstream source headers retain additional notices.\n"
    )


def package(root, output):
    entries = app_layout.allocate(root)
    app_layout.check_layout(root, entries, compiled=True)
    settings = json.loads((root / "build/flasher_args.json").read_text())
    if settings["extra_esptool_args"]["chip"] != "esp32s3":
        raise ValueError("Web flasher supports ESP32-S3 only")
    if settings["flash_settings"]["flash_size"] != "16MB":
        raise ValueError("Web flasher requires the board's 16 MB flash configuration")
    if int(settings["bootloader"]["offset"], 0) != 0:
        raise ValueError("Unexpected ESP32-S3 bootloader offset")
    output.mkdir(parents=True, exist_ok=True)
    firmware = output / "firmware"
    previews = output / "previews"
    firmware.mkdir(exist_ok=True)
    previews.mkdir(exist_ok=True)

    def image(path, name):
        data = path.read_bytes()
        if len(data) < 24 or data[0] != 0xE9 or int.from_bytes(data[12:14], "little") != 9:
            raise ValueError(f"Not an ESP32-S3 firmware image: {path}")
        digest = hashlib.sha256(data).hexdigest()
        filename = f"{name}-{digest[:16]}.bin"
        (firmware / filename).write_bytes(data)
        return {"file": f"firmware/{filename}", "size": len(data), "sha256": digest}

    manifest = {
        "schema": 1,
        "chip": "ESP32-S3",
        "flashSize": app_layout.FLASH_SIZE,
        "bootloader": image(root / "build" / settings["bootloader"]["file"], "bootloader"),
        "launcher": {**image(entries[0][2], "launcher"), "capacity": entries[0][4]},
        "apps": [],
    }
    if manifest["bootloader"]["size"] > 0x8000:
        raise ValueError("Bootloader overlaps the partition table")
    for entry, (name, description, preview) in zip(entries[1:], APP_DETAILS, strict=True):
        label, _, path, _, capacity, _ = entry
        source = path.parent.parent / preview
        target = previews / f"{label}.gif"
        shutil.copyfile(source, target)
        manifest["apps"].append(
            {
                "id": label,
                "name": name,
                "description": description,
                "preview": f"previews/{label}.gif",
                "capacity": capacity,
                **image(path, label),
            }
        )
    manifest["source"] = package_source(root, output)
    package_licenses(root, output)
    manifest["build"] = hashlib.sha256(json.dumps(manifest, sort_keys=True).encode()).hexdigest()[
        :12
    ]
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=app_layout.ROOT)
    parser.add_argument("--output", type=Path, default=app_layout.ROOT / "web/public")
    args = parser.parse_args()
    try:
        manifest = package(args.root.resolve(), args.output.resolve())
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Packaging error: {error}\n")
    print(f"Packaged launcher and {len(manifest['apps'])} apps: build {manifest['build']}")


if __name__ == "__main__":
    main()
