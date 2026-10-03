#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Extract a regular transparent master grid, preserve pose scale, lock palette.

The original masters live in assets/masters. Alpha is retained until the final
binary transparent key needed by the device. Each row shares a scale so attack
poses and collapsed bodies do not mysteriously grow during animation.
"""

from pathlib import Path
from collections import deque
import argparse
from PIL import Image
from art_palette import CELL, PALETTE
from compile_assets import nearest

ROOT = Path(__file__).resolve().parents[1]


def silhouettes(master, rows, columns):
    """Locate full alpha-connected sprites before applying the logical grid.

    A sword can extend beyond an approximate generated cell boundary. Cropping
    the full silhouette first avoids cutting it off or importing its neighbor.
    Ambiguous masters with touching creatures are rejected for art correction.
    """
    pixels = master.load()
    width, height = master.size
    seen = bytearray(width * height)
    components = []
    for y in range(height):
        for x in range(width):
            if seen[y * width + x] or pixels[x, y][3] < 128:
                continue
            pending = deque([(x, y)])
            seen[y * width + x] = 1
            points = []
            while pending:
                xx, yy = pending.popleft()
                points.append((xx, yy))
                for nx, ny in ((xx + 1, yy), (xx - 1, yy), (xx, yy + 1), (xx, yy - 1)):
                    if (
                        0 <= nx < width
                        and 0 <= ny < height
                        and not seen[ny * width + nx]
                        and pixels[nx, ny][3] >= 128
                    ):
                        seen[ny * width + nx] = 1
                        pending.append((nx, ny))
            if len(points) > width * height // (rows * columns * 15):
                components.append(points)
    if len(components) != rows * columns:
        raise ValueError(f"expected {rows * columns} separate silhouettes, found {len(components)}")
    cells = {}
    for points in components:
        left = min(x for x, _ in points)
        right = max(x for x, _ in points) + 1
        top = min(y for _, y in points)
        bottom = max(y for _, y in points) + 1
        col = (left + right) * columns // (2 * width)
        row = (top + bottom) * rows // (2 * height)
        if (row, col) in cells:
            raise ValueError(f"multiple silhouettes in cell {row},{col}")
        crop = Image.new("RGBA", (right - left, bottom - top))
        crop_pixels = crop.load()
        for x, y in points:
            crop_pixels[x - left, y - top] = pixels[x, y]
        cells[row, col] = crop
    return cells


def extract(path, rows, output, columns=4):
    master = Image.open(path).convert("RGBA")
    atlas = Image.new("RGBA", (columns * CELL, rows * CELL))
    cache = {}
    cells = silhouettes(master, rows, columns)
    for row in range(rows):
        crops = [cells[row, col] for col in range(columns)]
        scale = min(
            (CELL - 4) / max(c.width for c in crops), (CELL - 4) / max(c.height for c in crops)
        )
        for col, crop in enumerate(crops):
            small = crop.resize(
                (max(1, round(crop.width * scale)), max(1, round(crop.height * scale))),
                Image.Resampling.LANCZOS,
            )
            indexed = Image.new("RGBA", small.size)
            colors = []
            pixels = (
                small.get_flattened_data()
                if hasattr(small, "get_flattened_data")
                else small.getdata()
            )
            for p in pixels:
                if p not in cache:
                    cache[p] = PALETTE[nearest(p)]
                colors.append(cache[p])
            indexed.putdata(colors)
            atlas.paste(
                indexed,
                (col * CELL + (CELL - indexed.width) // 2, row * CELL + CELL - 2 - indexed.height),
            )
    atlas.save(output)
    print(f"wrote {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("master", type=Path)
    parser.add_argument("rows", type=int)
    parser.add_argument("output", type=Path)
    parser.add_argument("--columns", type=int, default=4)
    args = parser.parse_args()
    extract(args.master, args.rows, args.output, args.columns)
