#!/usr/bin/env python3
"""Validate and compile DUNGEON//SEED source sheets into 4-bit C sprites."""
from pathlib import Path
import hashlib
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "source"
OUT_C, OUT_H = ROOT / "main" / "assets_generated.c", ROOT / "main" / "assets_generated.h"
PALETTE = [(0,0,0,0),(9,11,14,255),(25,30,37,255),(48,57,67,255),(77,89,101,255),(111,124,136,255),(169,180,188,255),(246,165,63,255),(255,211,106,255),(181,74,71,255),(98,80,139,255),(71,93,156,255),(63,200,154,255),(238,241,239,255),(94,113,75,255),(39,45,53,255)]

# name, sheet, cell column, cell row; all assets are 32x32 logical pixels
ENTRIES = [
    *( (f"WALL_{name}", "tiles-and-props.png", col, 0) for name,col in (("NEAR",0),("MID",1),("FAR",2)) ),
    *( (f"CEILING_{name}", "tiles-and-props.png", col, 0) for name,col in (("NEAR",3),("MID",4),("FAR",5)) ),
    ("FLOOR_NEAR","tiles-and-props.png",6,0),("FLOOR_MID","tiles-and-props.png",7,0),("FLOOR_FAR","tiles-and-props.png",0,1),
    ("ARCH","tiles-and-props.png",1,1),("DOOR","tiles-and-props.png",2,1),
    ("TORCH","tiles-and-props.png",3,1),("BANNER","tiles-and-props.png",4,1),("CHAIN","tiles-and-props.png",5,1),("SKULLS","tiles-and-props.png",6,1),("RUBBLE","tiles-and-props.png",7,1),("ROOTS","tiles-and-props.png",1,2),("CHEST","tiles-and-props.png",2,2),("SHRINE","tiles-and-props.png",3,2),("GATE","tiles-and-props.png",4,2),
    *( (f"{kind}_{frame}", "actors.png", frame, row) for row,kind in enumerate(("SKELETON","SLIME","BRUTE","EYE")) for frame in range(4) ),
    *( (f"{kind}_{frame}", "bosses-and-effects.png", (row*4+frame)%5, (row*4+frame)//5) for row,kind in enumerate(("WARDEN","WYRM","ELDRITCH")) for frame in range(4) ),
    *((name,"bosses-and-effects.png",slot%5,slot//5) for slot,name in enumerate(("SWORD","STAFF","HANDS","SLASH","BOLT","IMPACT","BURST","TITLE"),start=12)),
]

def nearest(rgba):
    if rgba[3] < 128: return 0
    distance = [sum((rgba[k]-p[k])**2 for k in range(3)) for p in PALETTE]
    return min(range(1,16), key=lambda n: distance[n])

def compile():
    sheets = {n:Image.open(SOURCE/n).convert("RGBA") for _,n,_,_ in ENTRIES}
    values=[]
    for name,sh,c,r in ENTRIES:
        image=sheets[sh]
        if image.width < (c+1)*32 or image.height < (r+1)*32: raise ValueError(f"{name}: outside {sh}")
        indices=[nearest(image.getpixel((c*32+x,r*32+y))) for y in range(32) for x in range(32)]
        if not any(indices): raise ValueError(f"{name}: fully transparent")
        values.append((name,indices))
    h = ['#pragma once', '#include <stdint.h>', '', 'typedef struct { uint8_t width, height; const uint8_t *pixels; } dungeon_sprite_t;', 'enum dungeon_sprite_id {']
    h += [f'    DSP_{name},' for name,_ in values] + ['    DSP_COUNT', '};', 'extern const uint16_t dungeon_palette[16];', 'extern const dungeon_sprite_t dungeon_sprites[DSP_COUNT];', '']
    c=['#include "assets_generated.h"','', 'const uint16_t dungeon_palette[16] = {']
    c += [f'    0x{((r&0xf8)<<8)|((g&0xfc)<<3)|(b>>3):04x},' for r,g,b,a in PALETTE] + ['};','']
    for name,indices in values:
        packed=[]
        for i in range(0,len(indices),2): packed.append((indices[i]<<4)|indices[i+1])
        c.append(f'static const uint8_t pixels_{name.lower()}[] = {{')
        c += ['    '+', '.join(f'0x{x:02x}' for x in packed[i:i+16])+',' for i in range(0,len(packed),16)] + ['};']
    c += ['', 'const dungeon_sprite_t dungeon_sprites[DSP_COUNT] = {']
    c += [f'    [DSP_{name}] = {{32, 32, pixels_{name.lower()}}},' for name,_ in values] + ['};','']
    OUT_H.write_text('\n'.join(h)); OUT_C.write_text('\n'.join(c))
    digest=hashlib.sha256(OUT_C.read_bytes()).hexdigest()
    print(f"compiled {len(values)} sprites / {len(values)*512} bytes, sha256 {digest}")

if __name__ == '__main__': compile()
