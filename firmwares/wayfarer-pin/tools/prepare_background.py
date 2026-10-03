#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Prepare the pixel-art cockpit plate for Wayfarer firmware."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

from PIL import Image

WIDTH = 536
HEIGHT = 240


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("rgb565", type=Path)
    parser.add_argument("preview", type=Path)
    args = parser.parse_args()

    image = Image.open(args.source).convert("RGB")
    mask_path = args.source.parent / "cockpit-window-mask.png"
    mask = Image.open(mask_path).convert("RGBA").getchannel("A")
    if mask.size != image.size:
        mask = mask.resize(image.size, Image.Resampling.NEAREST)
    target_ratio = WIDTH / HEIGHT
    ratio = image.width / image.height
    if ratio > target_ratio:
        crop_width = round(image.height * target_ratio)
        left = (image.width - crop_width) // 2
        image = image.crop((left, 0, left + crop_width, image.height))
        mask = mask.crop((left, 0, left + crop_width, mask.height))
    else:
        crop_height = round(image.width / target_ratio)
        top = (image.height - crop_height) // 2
        image = image.crop((0, top, image.width, top + crop_height))
        mask = mask.crop((0, top, image.width, top + crop_height))

    # Draw on a 268x120 logical grid, then enlarge with nearest-neighbor so every
    # authored pixel stays crisp on the 536x240 panel.
    logical = image.resize((WIDTH // 2, HEIGHT // 2), Image.Resampling.LANCZOS)
    logical = logical.quantize(colors=48, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    image = logical.resize((WIDTH, HEIGHT), Image.Resampling.NEAREST).convert("RGB")
    alpha = mask.resize((WIDTH // 2, HEIGHT // 2), Image.Resampling.NEAREST)
    alpha = alpha.resize((WIDTH, HEIGHT), Image.Resampling.NEAREST)
    args.preview.parent.mkdir(parents=True, exist_ok=True)
    args.rgb565.parent.mkdir(parents=True, exist_ok=True)
    preview = image.convert("RGBA")
    preview.putalpha(alpha)
    preview.save(args.preview, optimize=True)

    packed = bytearray(WIDTH * HEIGHT * 2)
    data = image.tobytes()
    coverage = alpha.tobytes()
    for pixel in range(WIDTH * HEIGHT):
        red, green, blue = data[pixel * 3 : pixel * 3 + 3]
        value = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)
        # Zero keys only the view through the canopy; opaque black stays opaque.
        value = max(1, value) if coverage[pixel] >= 128 else 0
        struct.pack_into("<H", packed, pixel * 2, value)
    args.rgb565.write_bytes(packed)


if __name__ == "__main__":
    main()
