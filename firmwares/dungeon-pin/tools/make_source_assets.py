#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Draw native pixel materials, architecture, props, hands and combat effects.

Creature atlases are separately imported from the preserved generated masters;
running this script never replaces character artwork.
"""

from pathlib import Path
from random import Random
from math import cos, sin, pi
from PIL import Image, ImageDraw
from art_palette import CELL, PALETTE as P

OUT = Path(__file__).resolve().parents[1] / "assets" / "source"


def tile():
    im = Image.new("RGBA", (CELL, CELL))
    return im, ImageDraw.Draw(im)


def rect(d, xy, c):
    d.rectangle(xy, fill=P[c])


def poly(d, xy, c, edge=1):
    d.polygon(xy, fill=P[c], outline=P[edge])


def stroke(d, xy, c, w=1):
    d.line(xy, fill=P[c], width=w)


def dot(d, x, y, c):
    d.point((x, y), fill=P[c])


def ellipse(d, xy, c, edge=1):
    d.ellipse(xy, fill=P[c], outline=P[edge])


def gem(d, x, y, size=7, c=29):
    poly(d, [(x, y - size), (x + size, y), (x, y + size), (x - size, y)], c)
    poly(d, [(x, y - size + 1), (x, y), (x - size + 1, y)], 30 if c == 29 else 37)
    stroke(d, [(x, y - size + 2), (x + size - 2, y)], 61)


def masonry(depth, floor=False):
    im, d = tile()
    rng = Random(91 + depth + floor * 19)
    rect(d, (0, 0, 63, 63), 2)
    height = 16 if not floor else 21
    for row, y in enumerate(range(-3, 64, height)):
        width = 27 if not floor else 33
        for x in range(-width + (row % 2) * 13, 64, width):
            base = (4, 3, 3)[depth] + rng.randrange(2)
            rect(d, (x + 1, y + 1, x + width - 2, y + height - 2), base)
            stroke(d, [(x + 2, y + 1), (x + width - 3, y + 1)], base + 1)
            stroke(d, [(x + 1, y + 2), (x + 1, y + height - 3)], base + 1)
            stroke(d, [(x + 2, y + height - 2), (x + width - 2, y + height - 2)], 2)
            for _ in range(16 - depth * 4):
                xx = x + rng.randrange(3, width - 3)
                yy = y + rng.randrange(3, height - 3)
                dot(d, xx, yy, base + (1 if rng.randrange(3) == 0 else -1))
            if rng.randrange(3) == 0:
                xx = x + rng.randrange(8, 20)
                stroke(d, [(xx, y + 2), (xx - 2, y + 6), (xx + 1, y + 9)], 2)
                dot(d, xx + 1, y + 3, base + 1)
    return im


def biome_material(biome, floor=False):
    """Palette-native tiles: mossy ruin, cracked basalt, frozen carved stone."""
    im = masonry(0, floor)
    d = ImageDraw.Draw(im)
    rng = Random(720 + biome + floor * 99)
    maps = {
        1: {2: 50, 3: 50, 4: 51, 5: 52, 6: 53, 7: 54},
        2: {2: 1, 3: 2, 4: 3, 5: 55, 6: 56, 7: 57},
        3: {2: 21, 3: 22, 4: 62, 5: 63, 6: 25, 7: 61},
    }
    mapping = maps[biome]
    pixels = im.load()
    for y in range(CELL):
        for x in range(CELL):
            color = P.index(pixels[x, y])
            pixels[x, y] = P[mapping.get(color, color)]
    if biome == 1:
        for _ in range(13):
            x, y = rng.randrange(64), rng.randrange(64)
            stroke(d, [(x, y), (x + 3, y - 2), (x + 6, y)], 53)
            dot(d, x + 1, y - 1, 54)
    elif biome == 2:
        for _ in range(6 if floor else 3):
            x, y = rng.randrange(64), rng.randrange(64)
            stroke(d, [(x, y), (x + 4, y + 4), (x + 2, y + 9), (x + 7, y + 15)], 39)
            stroke(d, [(x + 1, y + 1), (x + 4, y + 4), (x + 3, y + 9)], 41)
            dot(d, x + 3, y + 7, 43)
    else:
        for _ in range(6):
            x, y = rng.randrange(64), rng.randrange(64)
            stroke(d, [(x, y), (x + 2, y + 6), (x - 2, y + 9), (x, y + 14)], 25)
            dot(d, x + 1, y + 1, 61)
    return im


def dungeon_prop(kind):
    im, d = tile()
    if kind in ("HP_POTION", "MP_POTION"):
        color = 11 if kind == "HP_POTION" else 21
        rect(d, (26, 4, 38, 13), 32)
        stroke(d, [(27, 4), (37, 4)], 34)
        rect(d, (24, 13, 40, 23), 7)
        stroke(d, [(26, 14), (26, 23)], 61)
        ellipse(d, (14, 20, 50, 59), 7)
        ellipse(d, (17, 24, 47, 57), color + 1)
        rect(d, (18, 27, 46, 43), color + 1)
        ellipse(d, (19, 31, 45, 55), color + 2)
        stroke(d, [(20, 28), (19, 35), (20, 44)], 61, 2)
        stroke(d, [(23, 52), (36, 54), (41, 50)], color + 3, 2)
        rect(d, (23, 37, 42, 47), 34)
        rect(d, (25, 38, 40, 46), 9)
        stroke(d, [(28, 42), (36, 42)], color + 2, 2)
        stroke(d, [(32, 39), (32, 45)], color + 2, 2)
    elif kind == "COFFIN":
        poly(d, [(23, 6), (41, 6), (50, 20), (45, 60), (18, 60), (14, 20)], 3)
        poly(d, [(24, 10), (39, 10), (45, 22), (41, 55), (23, 55), (19, 22)], 5)
        stroke(d, [(24, 10), (39, 10), (44, 22)], 8)
        stroke(d, [(32, 17), (32, 42)], 8, 3)
        stroke(d, [(25, 25), (39, 25)], 8, 3)
        stroke(d, [(24, 47), (28, 51), (27, 55)], 2)
    elif kind == "BOOKS":
        rect(d, (7, 8, 57, 62), 31)
        rect(d, (10, 11, 54, 59), 2)
        for y in (13, 29, 45):
            for n, x in enumerate(range(12, 53, 7)):
                c = (16, 21, 11, 50, 32, 55)[n % 6]
                rect(d, (x, y, x + 4, y + 12), c)
                stroke(d, [(x + 1, y + 1), (x + 1, y + 11)], c + 2)
                dot(d, x + 2, y + 3, 36)
                dot(d, x + 2, y + 9, 36)
            rect(d, (9, y + 13, 55, y + 15), 33)
            stroke(d, [(10, y + 13), (54, y + 13)], 34)
        stroke(d, [(8, 9), (8, 61)], 34)
        stroke(d, [(56, 9), (56, 61)], 33)
    elif kind == "RACK":
        for x in (15, 49):
            rect(d, (x, 13, x + 3, 61), 32)
        rect(d, (10, 24, 56, 28), 33)
        stroke(d, [(11, 24), (55, 24)], 34)
        for x in (21, 40):
            poly(d, [(x, 8), (x + 3, 3), (x + 6, 9), (x + 5, 40), (x + 1, 40)], 7)
            stroke(d, [(x + 2, 9), (x + 2, 39)], 61)
            rect(d, (x - 2, 40, x + 8, 42), 35)
            rect(d, (x + 2, 43, x + 4, 52), 32)
        ellipse(d, (25, 30, 40, 52), 4)
        ellipse(d, (28, 33, 37, 49), 35)
    elif kind == "MUSHROOMS":
        for x, y, s in ((16, 43, 11), (37, 30, 15), (51, 49, 9)):
            rect(d, (x - 2, y, x + 2, 60), 51)
            stroke(d, [(x - 1, y + 2), (x - 1, 58)], 54)
            ellipse(d, (x - s, y - s, x + s, y + 4), 17)
            rect(d, (x - s, y + 1, x + s, y + 4), 19)
            for dx, dy in ((-4, -4), (3, -7), (6, 0)):
                dot(d, x + dx, y + dy, 30)
    elif kind == "CRYSTALS":
        for x, y, s in ((17, 42, 12), (34, 23, 19), (48, 44, 10)):
            poly(d, [(x, y - s), (x + 6, y), (x + 3, 60), (x - 5, 58), (x - 7, y)], 23)
            poly(d, [(x, y - s + 1), (x, y + 2), (x + 3, 59), (x + 5, y)], 25, 25)
            stroke(d, [(x, y - s + 2), (x + 4, y)], 61)
        poly(d, [(9, 61), (17, 56), (48, 55), (56, 62)], 62)
    elif kind == "STAIRS":
        im = arch()
        d = ImageDraw.Draw(im)
        for n in range(7):
            y = 30 + n * 5
            half = 9 + n * 2
            rect(d, (32 - half, y, 32 + half, y + 4), 3 + n % 2)
            stroke(d, [(32 - half, y), (32 + half, y)], 7)
            stroke(d, [(33 - half, y + 1), (33 + half, y + 1)], 5)
    elif kind == "PORTAL":
        im = arch(True)
        d = ImageDraw.Draw(im)
        for y in (23, 32, 41, 50):
            stroke(d, [(19, y), (24, y - 3), (32, y + 2), (41, y - 2), (47, y)], 29)
    elif kind == "VINES":
        im = decor("ROOTS")
        d = ImageDraw.Draw(im)
        for x, y in ((12, 15), (28, 34), (47, 21), (8, 48)):
            poly(d, [(x, y), (x + 9, y - 5), (x + 5, y + 3)], 53, 50)
            dot(d, x + 5, y - 1, 54)
    return im


def arch(gate=False):
    im, d = tile()
    ellipse(d, (8, 2, 55, 50), 3)
    rect(d, (8, 25, 55, 63), 3)
    ellipse(d, (15, 9, 48, 48), 1)
    rect(d, (15, 28, 48, 63), 1)
    for side in (8, 49):
        for y in range(26, 64, 9):
            rect(d, (side, y, side + 6, y + 7), 5)
            stroke(d, [(side, y), (side + 5, y)], 7)
            stroke(d, [(side + 6, y + 1), (side + 6, y + 7)], 2)
    for n in range(9):
        a = pi + n * pi / 8
        x = 32 + int(cos(a) * 20)
        y = 26 + int(sin(a) * 20)
        b = a + pi / 18
        x2 = 32 + int(cos(b) * 14)
        y2 = 26 + int(sin(b) * 14)
        stroke(d, [(x, y), (x2, y2)], 2, 2)
        dot(d, x + 1, y + 1, 7)
    rect(d, (29, 3, 35, 10), 7)
    rect(d, (30, 4, 33, 8), 8)
    if gate:
        for x in range(19, 49, 7):
            rect(d, (x, 21, x + 2, 61), 4)
            stroke(d, [(x, 23), (x, 61)], 7)
            poly(d, [(x - 1, 24), (x + 1, 19), (x + 3, 24)], 6)
        for y in (35, 52):
            rect(d, (16, y, 47, y + 2), 3)
            stroke(d, [(17, y), (46, y)], 6)
            for x in range(20, 47, 7):
                dot(d, x, y + 1, 8)
    return im


def door():
    im = arch()
    d = ImageDraw.Draw(im)
    poly(d, [(17, 62), (17, 27), (22, 17), (31, 13), (41, 17), (46, 27), (46, 62)], 32)
    for x in range(18, 47, 5):
        stroke(d, [(x, 27 if x < 23 or x > 40 else 19), (x, 61)], 31)
        stroke(d, [(x + 1, 29), (x + 1, 59)], 33)
    for y in (30, 49):
        rect(d, (17, y, 46, y + 3), 3)
        stroke(d, [(18, y), (45, y)], 6)
        for x in (20, 28, 36, 43):
            dot(d, x, y + 1, 8)
    ellipse(d, (35, 38, 41, 44), 35)
    ellipse(d, (37, 39, 40, 43), 1)
    return im


def torch(brazier=False):
    im, d = tile()
    if brazier:
        poly(d, [(18, 37), (46, 37), (42, 47), (22, 47)], 4)
        stroke(d, [(18, 37), (46, 37)], 7, 2)
        for x in (23, 40):
            stroke(d, [(x, 46), (x - 4 if x < 32 else x + 4, 61)], 5, 3)
    else:
        rect(d, (28, 33, 35, 59), 31)
        stroke(d, [(29, 35), (29, 57)], 34)
        poly(d, [(23, 34), (40, 34), (37, 40), (26, 40)], 4)
        stroke(d, [(24, 34), (39, 34)], 7)
        rect(d, (22, 48, 28, 51), 3)
        dot(d, 23, 48, 7)
    poly(
        d,
        [(23, 34), (20, 25), (26, 29), (25, 16), (31, 6), (32, 20), (38, 13), (44, 29), (40, 36)],
        40,
    )
    poly(d, [(26, 34), (24, 26), (29, 28), (31, 15), (35, 25), (38, 21), (40, 32), (36, 37)], 42)
    poly(d, [(29, 33), (29, 27), (32, 22), (36, 31), (34, 36)], 44)
    dot(d, 27, 10, 42)
    dot(d, 41, 18, 41)
    dot(d, 35, 4, 44)
    return im


def chest(opened=False):
    im, d = tile()
    poly(d, [(7, 40), (49, 36), (57, 42), (54, 58), (11, 60)], 31)
    rect(d, (11, 40, 50, 57), 32)
    for y in (43, 49, 54):
        stroke(d, [(13, y), (48, y - 1)], 33)
    for x in (22, 35):
        stroke(d, [(x, 42), (x - 1, 55)], 31)
        stroke(d, [(x + 1, 42), (x, 54)], 33)
    for x, y in ((18, 46), (38, 53), (28, 51), (46, 45)):
        stroke(d, [(x, y), (x + 4, y - 1)], 34)
        dot(d, x + 1, y + 1, 31)
    if opened:
        poly(d, [(8, 30), (10, 8), (48, 5), (56, 24), (49, 34)], 32)
        poly(d, [(13, 28), (14, 12), (45, 10), (49, 25)], 1)
        for x in (13, 42):
            stroke(d, [(x, 9), (x + 1, 29)], 35, 3)
        poly(d, [(10, 38), (22, 33), (43, 33), (53, 39), (48, 45), (14, 46)], 2)
        for x, y in ((19, 38), (27, 36), (36, 39), (43, 37), (31, 41), (22, 42)):
            ellipse(d, (x - 3, y - 1, x + 3, y + 2), 36, 34)
            dot(d, x, y - 1, 38)
        gem(d, 38, 34, 4)
    else:
        poly(d, [(7, 39), (10, 26), (17, 21), (47, 21), (55, 28), (57, 40)], 33)
        stroke(d, [(13, 26), (48, 25)], 34, 2)
        for x in (18, 27, 37, 47):
            stroke(d, [(x, 23), (x - 1, 36)], 32)
            stroke(d, [(x + 1, 24), (x, 35)], 34)
        stroke(d, [(9, 39), (55, 39)], 35, 2)
    for x in (14, 43):
        rect(d, (x, 39, x + 3, 58), 34)
        stroke(d, [(x, 40), (x, 57)], 36)
        for y in (42, 53):
            dot(d, x + 1, y, 38)
    rect(d, (27, 39, 34, 47), 35)
    rect(d, (30, 42, 31, 45), 1)
    for x in (10, 48):
        poly(d, [(x, 48), (x + 4, 48), (x + 4, 57), (x, 58)], 3)
        stroke(d, [(x + 1, 49), (x + 1, 56)], 6)
        dot(d, x + 2, 51, 8)
    stroke(d, [(12, 58), (51, 56)], 34)
    return im


def shrine():
    im, d = tile()
    poly(d, [(7, 54), (16, 46), (49, 46), (57, 54), (54, 60), (9, 60)], 3)
    stroke(d, [(8, 54), (56, 54)], 6)
    poly(d, [(19, 48), (21, 28), (43, 28), (46, 48)], 4)
    for x in (23, 39):
        stroke(d, [(x, 31), (x, 46)], 7, 2)
    poly(d, [(16, 27), (23, 22), (42, 22), (49, 27), (47, 32), (18, 32)], 5)
    stroke(d, [(18, 27), (47, 27)], 8)
    gem(d, 32, 14, 11)
    stroke(d, [(32, 3), (32, 8)], 61)
    for x, y in ((19, 13), (45, 15), (25, 4), (40, 5)):
        dot(d, x, y, 29)
    return im


def decor(kind):
    im, d = tile()
    rng = Random(51)
    if kind == "BANNER":
        stroke(d, [(9, 6), (55, 6)], 6, 2)
        poly(d, [(14, 8), (48, 8), (48, 46), (39, 58), (31, 51), (23, 59), (14, 46)], 16)
        for x in (17, 22, 42, 46):
            stroke(d, [(x, 10), (x, 46)], 17)
        poly(d, [(32, 15), (42, 25), (32, 36), (22, 25)], 34)
        poly(d, [(32, 19), (38, 25), (32, 31), (26, 25)], 36)
        stroke(d, [(32, 32), (32, 43)], 34, 2)
        for x in (15, 47):
            dot(d, x, 7, 36)
    elif kind == "CHAIN":
        for n in range(9):
            y = n * 7
            d.ellipse((28, y - 2, 35, y + 8), outline=P[6], width=2)
            stroke(d, [(29, y), (29, y + 4)], 8)
            stroke(d, [(34, y + 3), (34, y + 7)], 3)
    elif kind == "SKULLS":
        for x, y in ((10, 45), (27, 38), (47, 43)):
            ellipse(d, (x - 7, y - 7, x + 7, y + 6), 8)
            rect(d, (x - 4, y + 5, x + 4, y + 8), 9)
            ellipse(d, (x - 5, y - 1, x - 1, y + 3), 1)
            ellipse(d, (x + 1, y - 1, x + 5, y + 3), 1)
            poly(d, [(x, y + 2), (x - 2, y + 5), (x + 2, y + 5)], 3)
            for z in (-3, 0, 3):
                stroke(d, [(x + z, y + 6), (x + z, y + 8)], 3)
            stroke(d, [(x - 4, y - 5), (x, y - 6)], 10)
        stroke(d, [(2, 56), (55, 60)], 8, 2)
        stroke(d, [(8, 60), (49, 54)], 7, 2)
    elif kind == "RUBBLE":
        for _ in range(15):
            x = rng.randrange(3, 55)
            y = rng.randrange(45, 61)
            w = rng.randrange(3, 9)
            poly(
                d,
                [(x, y), (x + 2, y - 4), (x + w, y - 3), (x + w + 1, y + 2), (x, y + 2)],
                rng.randrange(3, 7),
            )
            stroke(d, [(x + 2, y - 4), (x + w, y - 3)], 7)
    elif kind == "ROOTS":
        for x in (7, 28, 51):
            stroke(d, [(x, 0), (x + 3, 17), (x - 4, 32), (x + 2, 49), (x - 5, 61)], 50, 3)
            stroke(d, [(x, 0), (x + 2, 17), (x - 5, 32)], 52)
            stroke(d, [(x - 4, 32), (x + 7, 23), (x + 9, 14)], 51)
            for y in (12, 28, 43):
                dot(d, x + 1, y, 53)
    elif kind == "COLUMN":
        rect(d, (21, 5, 43, 59), 3)
        rect(d, (25, 9, 39, 55), 5)
        for x in (26, 32, 38):
            stroke(d, [(x, 10), (x, 54)], 7)
        rect(d, (16, 3, 48, 8), 5)
        stroke(d, [(16, 3), (47, 3)], 8)
        rect(d, (15, 57, 49, 62), 4)
        stroke(d, [(16, 57), (48, 57)], 7)
        stroke(d, [(25, 23), (29, 27), (27, 35)], 2)
    elif kind == "TITLE":
        poly(d, [(32, 1), (54, 13), (50, 42), (32, 61), (13, 43), (9, 13)], 3)
        poly(d, [(32, 5), (49, 15), (46, 40), (32, 54), (18, 40), (14, 15)], 35)
        poly(d, [(32, 9), (44, 18), (41, 37), (32, 48), (23, 37), (19, 18)], 2)
        gem(d, 32, 26, 10, 36)
        stroke(d, [(32, 35), (32, 45)], 37, 2)
    return im


def effect(kind):
    im, d = tile()
    rng = Random(21)
    if kind == "SLASH":
        poly(d, [(2, 53), (16, 30), (45, 10), (61, 7), (40, 22), (21, 39)], 37, 36)
        poly(d, [(5, 51), (22, 30), (50, 12), (30, 30)], 61, 61)
        stroke(d, [(14, 57), (36, 36), (59, 27)], 35)
    elif kind == "BOLT":
        stroke(d, [(5, 57), (20, 35), (28, 36), (37, 15), (45, 18), (59, 3)], 27, 7)
        stroke(d, [(5, 57), (20, 35), (28, 36), (37, 15), (45, 18), (59, 3)], 29, 3)
        stroke(d, [(5, 57), (20, 35), (28, 36), (37, 15), (45, 18), (59, 3)], 61)
        stroke(d, [(28, 36), (42, 43), (52, 32)], 28)
    elif kind == "CLAW":
        for n in range(3):
            x = 8 + n * 13
            poly(d, [(x, 7), (x + 12, 21), (x + 20, 49), (x + 10, 38)], 14, 13)
            stroke(d, [(x + 1, 8), (x + 11, 22), (x + 18, 46)], 9)
    elif kind == "FIRE":
        poly(
            d,
            [
                (16, 58),
                (9, 42),
                (15, 28),
                (15, 42),
                (24, 32),
                (22, 17),
                (28, 6),
                (30, 26),
                (40, 15),
                (38, 34),
                (48, 27),
                (47, 45),
                (54, 38),
                (50, 56),
                (35, 62),
            ],
            39,
            39,
        )
        poly(
            d,
            [
                (19, 55),
                (15, 42),
                (25, 47),
                (27, 29),
                (32, 20),
                (34, 43),
                (43, 36),
                (41, 49),
                (47, 47),
                (43, 59),
                (30, 60),
            ],
            41,
            41,
        )
        poly(d, [(24, 54), (26, 42), (30, 47), (33, 34), (38, 50), (40, 57), (29, 59)], 43, 43)
        poly(d, [(29, 54), (32, 45), (37, 56), (32, 59)], 44, 44)
        for n in range(12):
            x = rng.randrange(7, 58)
            y = rng.randrange(4, 60)
            stroke(d, [(x, y), (x - 4, y + 6)], 41, 2)
    elif kind == "RUNE":
        for radius, c in ((27, 26), (22, 28), (16, 27)):
            d.ellipse((32 - radius, 32 - radius, 32 + radius, 32 + radius), outline=P[c])
        for n in range(8):
            a = n * pi / 4
            x = 32 + int(cos(a) * 24)
            y = 32 + int(sin(a) * 24)
            stroke(d, [(x - 2, y - 2), (x + 2, y + 2)], 30)
        poly(d, [(32, 9), (51, 43), (13, 43)], 0, 29)
        gem(d, 32, 32, 5)
    else:
        colors = (37, 43, 61) if kind == "BURST" else (29, 30, 61)
        for n in range(12):
            a = n * pi / 6
            r = 27 if n % 2 else 18
            x = 32 + int(cos(a) * r)
            y = 32 + int(sin(a) * r)
            stroke(
                d,
                [(32 + int(cos(a) * 7), 32 + int(sin(a) * 7)), (x, y)],
                colors[n % 3],
                2 if n % 3 == 0 else 1,
            )
        gem(d, 32, 32, 7, 36 if kind == "BURST" else 29)
    return im


def hands():
    im, d = tile()
    for shift in (0, 34):
        poly(
            d,
            [
                (shift + 4, 63),
                (shift + 5, 43),
                (shift + 8, 38),
                (shift + 8, 33),
                (shift + 13, 30),
                (shift + 16, 33),
                (shift + 20, 32),
                (shift + 24, 35),
                (shift + 26, 44),
                (shift + 22, 53),
                (shift + 25, 63),
            ],
            45,
        )
        poly(
            d,
            [
                (shift + 7, 42),
                (shift + 11, 38),
                (shift + 21, 37),
                (shift + 24, 44),
                (shift + 20, 50),
                (shift + 11, 50),
            ],
            46,
            46,
        )
        for x in (10, 15, 20):
            stroke(d, [(shift + x, 34), (shift + x + 1, 42)], 47, 2)
            stroke(d, [(shift + x + 2, 35), (shift + x + 2, 42)], 44)
            dot(d, shift + x, 34, 48)
        poly(
            d,
            [(shift + 5, 45), (shift + 9, 42), (shift + 13, 47), (shift + 12, 52), (shift + 6, 51)],
            47,
            45,
        )
        poly(d, [(shift + 4, 53), (shift + 21, 51), (shift + 27, 63), (shift + 3, 63)], 3)
        stroke(d, [(shift + 5, 54), (shift + 22, 53)], 35, 2)
        stroke(d, [(shift + 6, 55), (shift + 22, 54)], 37)
        stroke(d, [(shift + 8, 58), (shift + 11, 63)], 6, 2)
        dot(d, shift + 18, 59, 36)
        dot(d, shift + 19, 59, 38)
    return im


def atlas(images, columns, name):
    im = Image.new("RGBA", (columns * CELL, ((len(images) + columns - 1) // columns) * CELL))
    for n, source in enumerate(images):
        im.paste(source, ((n % columns) * CELL, (n // columns) * CELL))
    im.save(OUT / name)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    images = [masonry(i) for i in range(3)]
    images += [masonry(i) for i in range(3)]
    images += [masonry(i, True) for i in range(3)]
    images += [
        arch(),
        door(),
        torch(),
        decor("BANNER"),
        decor("CHAIN"),
        decor("SKULLS"),
        decor("RUBBLE"),
        decor("ROOTS"),
        chest(),
        shrine(),
        arch(True),
        chest(True),
        decor("COLUMN"),
        torch(True),
        decor("TITLE"),
    ]
    atlas(images, 8, "tiles-and-props.png")
    atlas(
        [hands()]
        + [effect(kind) for kind in ("SLASH", "BOLT", "IMPACT", "BURST", "RUNE", "FIRE", "CLAW")],
        4,
        "weapons-and-effects.png",
    )
    atlas(
        [biome_material(b, f) for b in (1, 2, 3) for f in (False, True)]
        + [
            dungeon_prop(k)
            for k in (
                "COFFIN",
                "BOOKS",
                "RACK",
                "MUSHROOMS",
                "CRYSTALS",
                "STAIRS",
                "PORTAL",
                "VINES",
                "HP_POTION",
                "MP_POTION",
            )
        ],
        4,
        "biomes-and-props.png",
    )
    print("wrote native 64px architecture, props, weapons and effects")


if __name__ == "__main__":
    main()
