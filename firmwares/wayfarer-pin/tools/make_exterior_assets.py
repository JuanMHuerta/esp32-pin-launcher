#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Create editable, deterministic pixel sprites and compact indexed firmware art."""
from pathlib import Path
import argparse
import math
import random
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'assets/source/exterior'
DEST = ROOT / 'main'


def field(x, y, seed):
    """Layered, coherent terrain noise; all detail is sampled on integer pixels."""
    def lattice(ix, iy):
        n = (ix * 374761393 + iy * 668265263 + seed * 1274126177) & 0xffffffff
        n = ((n ^ (n >> 13)) * 1274126177) & 0xffffffff
        return ((n ^ (n >> 16)) & 65535) / 32767.5 - 1
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a = lattice(ix, iy) * (1-fx) + lattice(ix+1, iy) * fx
    b = lattice(ix, iy+1) * (1-fx) + lattice(ix+1, iy+1) * fx
    return a * (1-fy) + b * fy


def terrain(x, y, seed):
    return sum(field(x * scale, y * scale, seed + n * 17) * weight
               for n, (scale, weight) in enumerate(((.075,.56),(.16,.28),(.34,.12),(.7,.04))))


def world(kind):
    im = Image.new('RGBA', (96, 88)); p = im.load()
    cx, cy, radius = 48, 43, 33
    rng = random.Random(791 + kind)
    craters = [(rng.randrange(18,78),rng.randrange(13,73),rng.randrange(1,7)) for _ in range(37)]
    plates = [(rng.randrange(12,85),rng.randrange(9,78)) for _ in range(25)]
    atmosphere = [(55,99,143),(122,91,66),(68,113,137),(116,94,75),(108,52,39),(64,75,91)][kind]
    for y in range(88):
        for x in range(96):
            dx, dy = (x-cx)/radius, (y-cy)/radius
            distance = dx*dx + dy*dy
            if distance > 1:
                if kind != 5 and 1 < distance < 1.065 and dx < -.2:
                    p[x,y] = (*tuple(int(v * .72) for v in atmosphere),255)
                continue
            z = math.sqrt(1-distance)
            incidence = -dx*.78 - dy*.30 + z*.42
            day = max(0,incidence)
            light = .13 + .66 * day ** .72
            # Curved longitude gives surface features the shape of a globe.
            sx = 48 + math.atan2(dx,z) * 25
            sy = 43 + math.asin(dy) * 24
            rough = terrain(sx,sy,kind+11)
            fine = field(sx*.65,sy*.65,kind+81)
            grain = ((x*13+y*7)%5-2) * 1.1
            r,g,b = 105,115,132
            emissive = None
            if kind == 0:
                land = terrain(sx,sy,139)
                r,g,b = (40+rough*22,82+rough*26,117+rough*34)
                if land > -.035:
                    ridge = abs(field(sx*.28,sy*.28,35))
                    r,g,b = (77+ridge*48,98+ridge*34,76+ridge*18)
                    if land < .012: r,g,b = 133,128,94
                    if ridge > .75: r,g,b = 142,142,121
                clouds = terrain(sx+24,sy*1.7,77) + field(sx*.4,sy*.23,71)*.16
                if clouds > .18 and fine > -.55:
                    w = min(.85,(clouds-.18)*3.2)
                    r,g,b = tuple(c*(1-w)+v*w for c,v in zip((r,g,b),(164,177,183)))
                if incidence < .09 and land > .08 and clouds < .23:
                    city = field(sx*.58,sy*.58,342)
                    if city > .50 and (x*19+y*13)%9 < 2: emissive = (139,103,54)
            elif kind == 1:
                ridge = math.sin(sy*.72 + rough*12 + field(sx*.14,sy*.14,3)*2)
                r,g,b = 149+ridge*22+rough*35,113+ridge*18+rough*22,80+ridge*11+rough*15
                if abs(field(sx*.20,sy*.18,48)) < .08: r,g,b = 80,64,56
                for qx,qy,qr in craters[:12]:
                    d=math.hypot(x-qx,y-qy)
                    if d < qr*.8: r,g,b = r*.67,g*.68,b*.72
                    elif d < qr+1 and x < qx: r,g,b = r*1.13,g*1.12,b*1.10
                if dy < -.8 and rough > -.2: r,g,b = 160,158,137
            elif kind == 2:
                r,g,b = 111+rough*55,139+rough*48,152+rough*40
                nearest = sorted((x-qx)**2+(y-qy)**2 for qx,qy in plates)[:2]
                fracture = math.sqrt(nearest[1])-math.sqrt(nearest[0])
                if fracture < .6: r,g,b = 42,76,103
                elif fracture < 1.7: r,g,b = 76,112,132
                if fine > .45 and rough > .07: r,g,b = 164,176,178
                if rough < -.36: r,g,b = 57,88,113
            elif kind == 3:
                bend = rough*3 + field(sx*.11,sy*.11,97)*2
                band = math.sin(sy*.64 + bend)
                vein = math.sin(sy*1.8 + bend*1.7)
                r,g,b = 152+band*27+vein*7,118+band*22+vein*5,92+band*19+vein*4
                u,v=(sx-58)/9,(sy-51)/4
                storm=u*u+v*v
                if storm < 1.4:
                    swirl=math.sin(math.atan2(v,u)*2 + storm*9)
                    r,g,b=124+swirl*18,75+swirl*13,63+swirl*11
            elif kind == 4:
                r,g,b = 76+rough*32,73+rough*25,79+rough*31
                nearest = sorted((x-qx)**2+(y-qy)**2 for qx,qy in plates)[:2]
                crack=math.sqrt(nearest[1])-math.sqrt(nearest[0])
                if crack < .48:
                    r,g,b=100,48,36
                    if fine > -.05: emissive=(171+fine*36,69+fine*22,30)
                elif crack < 1.25: r,g,b=87,51,44
                if fine < -.6: r,g,b=38,40,48
            else:
                r,g,b=130+rough*47,135+rough*43,143+rough*45
                for qx,qy,qr in craters:
                    d=math.hypot(x-qx,y-qy)
                    if d < qr:
                        shadow=.50 + .26*(x-qx+qr)/(qr*2)
                        r,g,b=r*shadow,g*shadow,b*shadow
                    elif d < qr+1.1:
                        rim=1.20 if x+y < qx+qy else .72
                        r,g,b=r*rim,g*rim,b*rim
            # Deliberate, restrained pixel dithering keeps shaded texture readable.
            light = round((light + (.010 if (x+y)%2 else -.010))*24)/24
            result=tuple(max(4,min(205,int(c*light+grain+fine*3))) for c in (r,g,b))
            if emissive: result=tuple(max(a,int(b)) for a,b in zip(result,emissive))
            p[x,y]=(*result,255)
    if kind == 3:
        for y in range(88):
            for x in range(96):
                u,v=x-cx,y-cy+(x-cx)*.28
                ellipse=(u/47)**2+(v/12)**2
                if .67 < ellipse < 1 and (v > 0 or (x-cx)**2+(y-cy)**2 > 34**2):
                    ring=math.sin(ellipse*175)*9+math.sin(ellipse*61)*6
                    light=.78 if x < cx else .48
                    if v > 0 and 0 < u < 25: light*=.45
                    r,g,b=(144+ring)*light,(126+ring)*light,(102+ring)*light
                    if .815 < ellipse < .84 or .93 < ellipse < .943: r,g,b=28,29,35
                    p[x,y]=(int(r),int(g),int(b),255)
    return im


def finish_hull(im, seed):
    """Paint directional shade, recessed machinery and wear on the pixel canvas."""
    source=im.copy(); p=im.load(); rng=random.Random(seed)
    for y in range(im.height):
        for x in range(im.width):
            r,g,b,a=source.getpixel((x,y))
            if not a: continue
            luma=(r*3+g*6+b)/10
            shade=.80 - .29*y/max(1,im.height-1)
            if luma > 100:
                # Weathered grey paint; preserve subdued rust and teal accents.
                r,g,b=(r*.84+10,g*.87+12,b*.92+15)
            rim=y>0 and source.getpixel((x,y-1))[3]==0
            boost=15 if rim else 0
            p[x,y]=tuple(max(7,min(172,int(c*shade+boost))) for c in (r,g,b))+(a,)
    d=ImageDraw.Draw(im)
    def safe_rect(box,color):
        for y in range(max(0,box[1]),min(im.height,box[3]+1)):
            for x in range(max(0,box[0]),min(im.width,box[2]+1)):
                if source.getpixel((x,y))[3]: d.point((x,y),fill=color)
    for y in range(8,im.height-5,9):
        for x in range(17,im.width-13,15):
            if source.getpixel((x,y))[3] and sum(source.getpixel((x,y))[:3])>210:
                safe_rect((x,y,x+5,y+3),'#23313d')
                safe_rect((x+1,y,x+4,y),'#687b85')
                safe_rect((x+1,y+2,x+3,y+2),'#151f2c')
                safe_rect((x+6,y+4,x+6,y+4),'#96744f')
    for _ in range(im.width*im.height//38):
        x,y=rng.randrange(4,im.width-4),rng.randrange(3,im.height-3)
        if source.getpixel((x,y))[3]:
            c=rng.choice(['#28343e','#3c4b55','#71818a','#7e6651'])
            safe_rect((x,y,x+rng.choice([0,0,1,2]),y),c)
    return im


def atlas_ship(index):
    atlas=Image.open(ROOT/'assets/source/space-sprites-pixel.png').convert('RGBA')
    # The painted atlas has uneven spacing; uniform thirds cut off the freighter
    # nose and include neighboring hulls and the planet in the other cutouts.
    boxes=[(22,95,685,365),(708,70,1142,360),(1159,102,1767,335)]
    box=boxes[index]
    cutout=atlas.crop(tuple(round(v * (atlas.width/1774 if i%2==0 else atlas.height/887))
                            for i,v in enumerate(box)))
    cutout=cutout.crop(cutout.getchannel('A').point(lambda a:255 if a>=128 else 0).getbbox())
    width=[108,86,106][index]
    cutout=cutout.resize((width,round(cutout.height*width/cutout.width)),Image.Resampling.LANCZOS)
    cutout.putalpha(cutout.getchannel('A').point(lambda a:255 if a>=128 else 0))
    return finish_hull(cutout,1103+index)

HULL = ['#182735', '#34434d', '#596873', '#879398', '#bec4c1', '#e6e5d6',
        '#fff0c5', '#9e6149', '#d09160', '#529aab', '#97d9df']


def craft(tug=False):
    im = Image.new('RGBA', (72, 32))
    d = ImageDraw.Draw(im)
    if tug:
        d.rectangle((13, 10, 54, 22), fill=HULL[0])
        for x in (20, 33, 46):
            d.rounded_rectangle((x, 7, x + 9, 25), radius=2, fill=HULL[2], outline=HULL[0])
            d.rectangle((x + 1, 8, x + 8, 13), fill=HULL[4])
            d.line((x + 2, 9, x + 7, 9), fill=HULL[5])
            d.rectangle((x + 2, 17, x + 7, 18), fill=HULL[7])
            d.line((x + 1, 24, x + 8, 24), fill=HULL[1])
        d.polygon([(53, 11), (62, 11), (67, 15), (67, 19), (54, 22)], fill=HULL[4], outline=HULL[0])
        d.rectangle((58, 13, 63, 15), fill=HULL[9])
        d.rectangle((5, 9, 17, 14), fill=HULL[3], outline=HULL[0])
        d.rectangle((5, 19, 17, 24), fill=HULL[3], outline=HULL[0])
        d.rectangle((3, 11, 5, 12), fill=HULL[10])
        d.rectangle((3, 21, 5, 22), fill=HULL[10])
    else:
        d.polygon([(13, 9), (25, 2), (38, 4), (44, 12), (62, 13), (69, 17),
                   (61, 21), (42, 22), (32, 29), (22, 28), (13, 22)], fill=HULL[0])
        d.polygon([(15, 10), (25, 4), (36, 5), (42, 14), (60, 15), (65, 17),
                   (60, 19), (40, 20), (31, 26), (23, 26), (15, 21)], fill=HULL[3])
        d.polygon([(24, 7), (35, 7), (38, 13), (57, 14), (60, 17), (39, 19),
                   (29, 23), (20, 22), (22, 13)], fill=HULL[4])
        d.line((25, 7, 35, 7), fill=HULL[5])
        d.rectangle((42, 14, 52, 15), fill=HULL[9])
        d.rectangle((14, 9, 23, 12), fill=HULL[2], outline=HULL[0])
        d.rectangle((14, 22, 22, 25), fill=HULL[2], outline=HULL[0])
        d.rectangle((11, 10, 13, 11), fill=HULL[10])
        d.rectangle((11, 23, 13, 24), fill=HULL[10])
        d.rectangle((26, 14, 35, 17), fill=HULL[2])
        d.line((26, 14, 35, 14), fill=HULL[5])
        d.rectangle((28, 24, 33, 25), fill=HULL[7])
    rng = random.Random(304 + tug)
    for _ in range(29):
        x, y = rng.randrange(15, 55), rng.randrange(8, 25)
        if im.getpixel((x, y))[3]:
            d.point((x, y), fill=HULL[rng.choice([1, 2, 5, 8])])
    return finish_hull(im, 304 + tug)


def station():
    im = Image.new('RGBA', (120, 64)); d = ImageDraw.Draw(im)
    for side in (0, 1):
        left = 4 if side == 0 else 83
        for y in (13, 36):
            d.rectangle((left, y, left + 31, y + 14), fill='#142635', outline='#78929b')
            for x in range(left + 2, left + 29, 6):
                d.rectangle((x, y + 2, x + 3, y + 12), fill='#355567')
                d.line((x, y + 3, x + 3, y + 3), fill='#609097')
            d.line((left, y + 8, left + 31, y + 8), fill='#192f42')
    d.rectangle((30, 29, 87, 33), fill='#848b87', outline='#182735')
    d.ellipse((38, 8, 80, 54), fill='#aeb7b4', outline='#26353f', width=2)
    d.ellipse((44, 15, 73, 48), fill='#1a2b3a', outline='#e0dec9', width=2)
    d.arc((40, 10, 78, 53), 270, 95, fill='#e7e4cc', width=3)
    for i in range(12):
        a = i * math.tau / 12
        x, y = 59 + int(math.cos(a)*20), 31 + int(math.sin(a)*23)
        ix, iy = 59 + int(math.cos(a)*14), 31 + int(math.sin(a)*17)
        d.line((ix,iy,x,y),fill='#34434d')
        d.point((x,y),fill='#a5acaa')
    for y in (18,44):
        for x in (41,75):
            d.rectangle((x,y,x+2,y+5),fill='#293944')
            d.point((x+1,y+1),fill='#ae875e')
    d.rectangle((55, 5, 62, 56), fill='#939e9e', outline='#34434d')
    d.rectangle((48, 24, 69, 37), fill='#c9ccbe', outline='#34434d')
    d.rectangle((52, 27, 67, 30), fill='#375b6b')
    for x in (53, 57, 61, 65): d.point((x, 28), fill='#afdede')
    d.rectangle((52, 34, 67, 35), fill='#ac7350')
    d.line((59, 5, 59, 0), fill='#9daaa7')
    d.line((44, 12, 32, 5), fill='#727f80')
    d.arc((27, 0, 40, 10), 5, 175, fill='#cbd3cd', width=2)
    for y in (18, 42):
        d.rectangle((39, y, 42, y + 3), fill='#e7b674')
        d.rectangle((76, y, 79, y + 3), fill='#e7b674')
    im = finish_hull(im, 852)
    d = ImageDraw.Draw(im)
    # Small occupied windows, ribbed cooling equipment and docking clamps.
    for x in range(53,67,3):
        d.point((x,28),fill='#799ba5' if x%2 else '#ab8353')
    for y in (8,43):
        d.rectangle((56,y,61,y+9),fill='#263541')
        for yy in range(y+1,y+9,2): d.line((57,yy,60,yy),fill='#56666d')
    for x in (30,82):
        d.rectangle((x,27,x+6,35),fill='#1b2935')
        d.line((x,27,x+5,27),fill='#7d8a8c')
        d.point((x+3,30),fill='#ad764b')
    return im


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-only', action='store_true', help='compile edited PNG masters without regenerating art')
    args = parser.parse_args()
    ART.mkdir(parents=True, exist_ok=True)
    if args.compile_only:
        names = [f'world_{i}' for i in range(6)]
        names += ['ship_freighter','ship_courier','ship_tanker','shuttle','tug','station']
        sprites = [(name, Image.open(ART / f'{name}.png').convert('RGBA')) for name in names]
    else:
        sprites = [(f'world_{i}', world(i)) for i in range(6)]
        sprites += [('ship_freighter',atlas_ship(0)), ('ship_courier',atlas_ship(1)), ('ship_tanker',atlas_ship(2)),
                    ('shuttle', craft()), ('tug', craft(True)), ('station', station())]
    header = ['/* SPDX-License-Identifier: GPL-3.0-only */', '#pragma once', '#include <stdint.h>',
              'typedef struct { int width, height; const uint8_t *pixels; const uint16_t *palette; } wayfarer_indexed_t;',
              'extern const wayfarer_indexed_t wayfarer_worlds[6];',
              'extern const wayfarer_indexed_t wayfarer_shuttle, wayfarer_tug, wayfarer_station;',
              'extern const wayfarer_indexed_t wayfarer_ship_freighter, wayfarer_ship_courier, wayfarer_ship_tanker;']
    code = ['/* SPDX-License-Identifier: GPL-3.0-only */', '/* Generated by tools/make_exterior_assets.py. */', '#include "exterior_generated.h"']
    sheet = Image.new('RGB', (720, 720), '#0b1422'); sd = ImageDraw.Draw(sheet)
    compiled_sizes = []
    for n, (name, im) in enumerate(sprites):
        im.save(ART / f'{name}.png')
        packed = im.crop(im.getchannel('A').getbbox())
        compiled_sizes.append(packed.size)
        alpha = packed.getchannel('A').tobytes()
        indexed = packed.convert('RGB').quantize(colors=63, dither=Image.Dither.NONE)
        palette = indexed.getpalette()
        palette += [0] * (768 - len(palette))
        values = [0]
        for i in range(63):
            r, g, b = palette[i*3:i*3+3]
            values.append(max(1, (r >> 3) << 11 | (g >> 2) << 5 | b >> 3))
        pixels = [v + 1 if a else 0 for v, a in zip(indexed.tobytes(), alpha)]
        for symbol, data, bits in [(f'{name}_pixels', pixels, 8), (f'{name}_palette', values, 16)]:
            code.append(f'static const uint{bits}_t {symbol}[{len(data)}] = {{')
            code.extend('    ' + ','.join(str(v) for v in data[i:i+24]) + ',' for i in range(0,len(data),24))
            code.append('};')
        if not name.startswith('world_'):
            code.append(f'const wayfarer_indexed_t wayfarer_{name} = {{{packed.width},{packed.height},{name}_pixels,{name}_palette}};')
        x, y = n % 3 * 240, n // 3 * 180
        view = im.resize((im.width*2, im.height*2), Image.Resampling.NEAREST)
        sheet.paste(view, (x + (240-view.width)//2, y+15), view)
        sd.text((x+10,y+165),(['OCEAN','DESERT','ICE','GAS GIANT','VOLCANIC','CRATERED MOON'][n] if n < 6 else name.replace('ship_','').upper()),fill='#adbdbd')
    code.append('const wayfarer_indexed_t wayfarer_worlds[6] = {')
    code.extend(f'    {{{compiled_sizes[i][0]},{compiled_sizes[i][1]},world_{i}_pixels,world_{i}_palette}},' for i in range(6))
    code.append('};')
    (DEST / 'exterior_generated.h').write_text('\n'.join(header)+'\n')
    (DEST / 'exterior_generated.c').write_text('\n'.join(code)+'\n')
    sheet.save(ROOT / 'preview-exterior-assets.png')
    print('Generated six detailed worlds, five weathered ships and orbital station (indexed pixel art).')

if __name__ == '__main__': main()
