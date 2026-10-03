# SPDX-License-Identifier: GPL-3.0-only
"""Check fill area and gravity response in the host water preview."""

from pathlib import Path


def center(frame: int) -> tuple[float, float, int]:
    with Path(f"preview-{frame:03}.ppm").open("rb") as file:
        assert file.readline() == b"P6\n"
        width, height = map(int, file.readline().split())
        assert file.readline() == b"255\n"
        pixels = file.read()
    assert len(pixels) == width * height * 3
    total_x = total_y = count = 0
    for y in range(2, height - 2, 2):
        for x in range(2, width - 2, 2):
            i = (y * width + x) * 3
            red, green, blue = pixels[i : i + 3]
            if blue > 80 and green > 20:
                total_x += x
                total_y += y
                count += 1
    assert count > 5000
    return total_x / count, total_y / count, count


def edge_span(frame: int, from_right: bool = False) -> int:
    with Path(f"preview-{frame:03}.ppm").open("rb") as file:
        assert file.readline() == b"P6\n"
        width, height = map(int, file.readline().split())
        assert file.readline() == b"255\n"
        pixels = file.read()
    edges = []
    for y in range(10, height - 10, 8):
        columns = range(width - 1, -1, -1) if from_right else range(width)
        for x in columns:
            i = (y * width + x) * 3
            if pixels[i + 2] > 80 and pixels[i + 1] > 20:
                edges.append(x)
                break
    assert len(edges) > 20
    return max(edges) - min(edges)


upright = center(30)
early = center(65)
one_second = center(90)
right = center(160)
inverted = center(310)
recovered = center(600)
left = center(700)
flat = center(850)
assert upright[0] + 12 < early[0] < upright[0] + 55, (upright, early)
assert one_second[0] > upright[0] + 80, (upright, one_second)
assert right[0] > upright[0] + 90, (upright, right)
assert inverted[1] < upright[1] - 80, (upright, inverted)
assert abs(recovered[0] - upright[0]) < 20, (upright, recovered)
assert abs(recovered[1] - upright[1]) < 20, (upright, recovered)
assert left[0] < upright[0] - 100, (upright, left)
assert abs(flat[0] - upright[0]) < 25, (upright, flat)
assert abs(flat[1] - upright[1]) < 25, (upright, flat)
counts = [
    sample[2] for sample in (upright, early, one_second, right, inverted, recovered, left, flat)
]
assert min(counts) > 5000, counts
assert max(counts) < min(counts) * 1.02, counts
print("Pixel liquid responds, reaches every wall, and settles when flat: PASS")
