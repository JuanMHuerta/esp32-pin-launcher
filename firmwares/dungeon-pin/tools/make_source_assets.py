#!/usr/bin/env python3
"""Create editable, palette-locked source sheets for DUNGEON//SEED.

The sheets are deliberately drawn at their native logical-pixel size.  They are
small enough to edit in any raster editor without losing the 1:1 pixel grid.
"""
from pathlib import Path
from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parents[1] / "assets" / "source"
PAL = [
    # 0 is the transparent key.  1..6 and 13 are the stone material ramp;
    # saturated entries are deliberately reserved for actors and feedback.
    (0, 0, 0, 0), (9, 11, 14, 255), (25, 30, 37, 255), (48, 57, 67, 255),
    (77, 89, 101, 255), (111, 124, 136, 255), (169, 180, 188, 255),
    (246, 165, 63, 255), (255, 211, 106, 255), (181, 74, 71, 255),
    (98, 80, 139, 255), (71, 93, 156, 255), (63, 200, 154, 255),
    (238, 241, 239, 255), (94, 113, 75, 255), (39, 45, 53, 255),
]

def sheet(w, h): return Image.new("RGBA", (w, h), PAL[0])
def cell(draw, col, row, w=32, h=32): return col*w, row*h, w, h
def px(d, x, y, c): d.point((x, y), fill=PAL[c])
def box(d, x, y, w, h, c): d.rectangle((x, y, x+w-1, y+h-1), fill=PAL[c])
def line(d, a, b, c, width=1): d.line((a, b), fill=PAL[c], width=width)
def outline_box(d, x, y, w, h, c=1):
    d.rectangle((x, y, x+w-1, y+h-1), outline=PAL[c])
def gem(d, x, y, c=12):
    line(d, (x, y-3), (x-3, y), 1); line(d, (x-3, y), (x, y+3), 1)
    line(d, (x, y+3), (x+3, y), 1); line(d, (x+3, y), (x, y-3), 1)
    d.polygon([(x,y-2),(x+2,y),(x,y+2),(x-2,y)], fill=PAL[c])

def tiles():
    im=sheet(256, 96); d=ImageDraw.Draw(im)

    def masonry(col, row, base, course, joint, chip, rows, offset):
        x,y,_,_=cell(d,col,row); box(d,x,y,32,32,base)
        for n in range(rows):
            yy=y + 2 + n * (30 // rows)
            line(d,(x,yy),(x+31,yy),joint)
            width=9 + ((n + offset) % 3) * 3
            for xx in range(x - (offset * 3 + n * 5) % width, x+32, width):
                line(d,(xx,yy-(30 // rows)+1),(xx,yy),joint)
                if (xx + n) % 2 == 0: px(d,min(x+30,max(x+1,xx+2)),max(y+1,yy-2),chip)
        box(d,x+1,y+1,30,1,course)

    # Depth-specific wall courses: near stones are large and scarred, far
    # stones are tighter and quieter.  The renderer repeats these as courses,
    # not as one stretched wall texture.
    masonry(0,0,3,5,2,6,4,0)
    masonry(1,0,3,4,2,5,5,1)
    masonry(2,0,2,3,1,4,7,2)

    # Ceiling rows compress toward the vanishing point.
    for col, spacing, base in ((3,7,2),(4,9,2),(5,12,1)):
        x,y,_,_=cell(d,col,0); box(d,x,y,32,32,base)
        for yy in range(y+2,y+32,spacing):
            line(d,(x,yy),(x+31,yy),3)
            for xx in range(x + ((yy-y)//spacing & 1)*5,x+32,11): line(d,(xx,yy-2),(xx+2,yy),4)
        line(d,(x,y+31),(x+31,y+31),1)

    # Floor rows open toward the viewer; angled joints suggest laid flagstone.
    for col, spacing, base in ((6,11,3),(7,8,2),(0,6,2)):
        row=0 if col != 0 else 1; x,y,_,_=cell(d,col,row); box(d,x,y,32,32,base)
        for yy in range(y+2,y+32,spacing):
            line(d,(x,yy),(x+31,yy),1)
            for xx in range(x+2+(yy-y)%7,x+32,10): line(d,(xx,yy-1),(xx-3,yy+min(spacing-1,5)),4)
        box(d,x,y+1,32,1,4)

    # Rear arch and a heavy stone door are deliberately material-only.
    x,y,_,_=cell(d,1,1); box(d,x+2,y+12,28,20,2); box(d,x+4,y+9,24,23,4); box(d,x+7,y+6,18,26,5); box(d,x+10,y+3,12,5,5); box(d,x+13,y+1,6,3,6)
    box(d,x+10,y+14,12,18,1); line(d,(x+8,y+12),(x+24,y+12),2)
    x,y,_,_=cell(d,2,1); box(d,x+3,y+6,26,26,2); box(d,x+5,y+4,22,28,4); box(d,x+8,y+7,16,25,1)
    for yy in range(y+9,y+31,6): line(d,(x+9,yy),(x+23,yy),3)
    line(d,(x+16,y+8),(x+16,y+31),5); box(d,x+9,y+7,14,1,6)

    # Torch keeps fire colour; its bracket remains iron/stone.  Everything
    # else in this prop set is grey stone, bone, or metal.
    x,y,_,_=cell(d,3,1); line(d,(x+16,y+14),(x+16,y+29),5,2); box(d,x+10,y+12,12,3,2); box(d,x+13,y+8,6,6,7); box(d,x+14,y+4,4,6,8)
    x,y,_,_=cell(d,4,1); line(d,(x+16,y+1),(x+16,y+30),5,2); box(d,x+10,y+5,12,17,3); box(d,x+11,y+7,10,14,4); d.polygon([(x+10,y+22),(x+16,y+29),(x+22,y+22)],fill=PAL[2])
    x,y,_,_=cell(d,5,1); line(d,(x+16,y),(x+16,y+8),4); [d.ellipse((x+12,y+z,x+20,y+z+5),outline=PAL[5]) for z in (8,15,22)]
    x,y,_,_=cell(d,6,1); [d.ellipse((x+3+i*7,y+18-(i%2)*4,x+13+i*7,y+27-(i%2)*4),fill=PAL[6],outline=PAL[1]) for i in range(4)]; [px(d,x+6+i*7,y+22-(i%2)*4,1) for i in range(4)]
    x,y,_,_=cell(d,7,1); [box(d,x+2+i*6,y+25-(i%3)*4,7,5,4) for i in range(5)]; [line(d,(x+3+i*6,y+23),(x+8+i*6,y+19),2) for i in range(4)]

    x,y,_,_=cell(d,1,2); line(d,(x+5,y),(x+12,y+29),5,2); line(d,(x+14,y),(x+18,y+30),4,2); line(d,(x+27,y),(x+20,y+29),5,2)
    x,y,_,_=cell(d,2,2); box(d,x+3,y+17,26,12,1); box(d,x+5,y+16,22,12,5); box(d,x+4,y+12,24,6,4); box(d,x+6,y+10,20,5,5); box(d,x+14,y+17,4,7,6)
    x,y,_,_=cell(d,3,2); box(d,x+5,y+25,22,5,3); box(d,x+9,y+11,14,14,4); box(d,x+11,y+7,10,17,15); gem(d,x+16,y+9,12); line(d,(x+3,y+23),(x+10,y+18),5); line(d,(x+29,y+23),(x+22,y+18),5)
    x,y,_,_=cell(d,4,2); box(d,x+2,y+8,28,24,4); box(d,x+5,y+5,22,27,3); box(d,x+8,y+3,16,5,4); box(d,x+11,y+1,10,4,5)
    for xx in range(x+8,x+25,5): box(d,xx,y+10,2,19,1)
    box(d,x+6,y+28,20,3,1); box(d,x+4,y+17,3,3,6); box(d,x+25,y+17,3,3,6)
    im.save(OUT / "tiles-and-props.png")

def actor_frame(d, x, y, kind, frame):
    # actor silhouettes: standing knight, liquid slime, horned brute, floating eye
    bob = frame == 1; lunge = frame == 2
    if kind == 0:
        box(d,x+13,y+4+bob,7,7,6); box(d,x+11,y+11+bob,11,12,3); box(d,x+9,y+13+bob,3,10,6); box(d,x+12,y+23,3,7,6); box(d,x+18,y+23,3,7,6); line(d,(x+10,y+16),(x+4-lunge,y+25),6,2); line(d,(x+20,y+15),(x+26+lunge,y+25),6,2); gem(d,x+15,y+7+bob,9)
    elif kind == 1:
        d.ellipse((x+5,y+12+bob,x+27,y+28+bob),fill=PAL[11],outline=PAL[1]); d.ellipse((x+8,y+7+bob,x+24,y+24+bob),fill=PAL[10]); box(d,x+12,y+15+bob,3,3,13); box(d,x+19,y+15+bob,3,3,13); box(d,x+13,y+22+bob,7,2,9)
    elif kind == 2:
        d.polygon([(x+9,y+9),(x+4,y+3),(x+13,y+6),(x+19,y+6),(x+28,y+3),(x+23,y+10),(x+25,y+25),(x+7,y+25)],fill=PAL[9],outline=PAL[1]); box(d,x+10,y+10,12,7,5); gem(d,x+13,y+13,9); gem(d,x+20,y+13,9); box(d,x+8,y+24,5,6,5); box(d,x+20,y+24,5,6,5); line(d,(x+6,y+16),(x+(1 if lunge else 4),y+23),9,3); line(d,(x+25,y+16),(x+(30 if lunge else 27),y+23),9,3)
    else:
        d.ellipse((x+6,y+8+bob,x+27,y+24+bob),fill=PAL[10],outline=PAL[1]); d.ellipse((x+10,y+11+bob,x+23,y+21+bob),fill=PAL[6]); d.ellipse((x+14,y+12+bob,x+20,y+20+bob),fill=PAL[1]); gem(d,x+17,y+16+bob,9); line(d,(x+9,y+22),(x+4-lunge,y+29),10); line(d,(x+24,y+22),(x+29+lunge,y+29),10)
    if frame == 3:
        line(d,(x+5,y+28),(x+27,y+28),9,2); line(d,(x+11,y+4),(x+22,y+29),13)

def actors():
    im=sheet(128,128); d=ImageDraw.Draw(im)
    for kind in range(4):
        for frame in range(4): actor_frame(d, frame*32, kind*32, kind, frame)
    im.save(OUT / "actors.png")

def boss_frame(d,x,y,kind,frame):
    attack = frame == 2; defeat = frame == 3; glow = 7 if frame == 1 else 9
    if kind == 0: # warden
        box(d,x+10,y+8,13,17,3); box(d,x+12,y+3,9,7,5); box(d,x+8,y+16,17,12,10); box(d,x+5,y+13,4,16,15); line(d,(x+7,y+15),(x+(1 if attack else 4),y+6),6,2); line(d,(x+23,y+14),(x+(31 if attack else 27),y+8),6,2); gem(d,x+15,y+6,9); gem(d,x+20,y+6,9)
    elif kind == 1: # wyrm
        d.polygon([(x+3,y+23),(x+9,y+15),(x+14,y+18),(x+20,y+8),(x+28,y+9),(x+30,y+15),(x+23,y+21),(x+28,y+27),(x+20,y+25),(x+12,y+29)],fill=PAL[10],outline=PAL[1]); line(d,(x+19,y+13),(x+30+(2 if attack else 0),y+6),6,2); gem(d,x+25,y+13,glow)
    else: # eldritch eye
        d.ellipse((x+4,y+7,x+28,y+25),fill=PAL[10],outline=PAL[1]); d.ellipse((x+8,y+10,x+25,y+22),fill=PAL[6]); d.ellipse((x+14,y+10,x+20,y+22),fill=PAL[1]); gem(d,x+17,y+16,9)
        for k in range(4): line(d,(x+8+k*5,y+23),(x+3+k*8,y+30),10)
    if defeat:
        for k in range(5): line(d,(x+16,y+16),(x+2+k*7,y+2+(k&1)*20),13)

def effects():
    im=sheet(160,128); d=ImageDraw.Draw(im)
    for kind in range(3):
        for frame in range(4): boss_frame(d,(kind*4+frame)%5*32, (kind*4+frame)//5*32,kind,frame)
    # player sword/staff/hands and effect frames in slots 12 onward
    for slot in range(12,20):
        x,y=slot%5*32,slot//5*32
        if slot==12: line(d,(x+5,y+28),(x+26,y+5),13,2); line(d,(x+3,y+27),(x+10,y+30),6,2)
        elif slot==13: line(d,(x+4,y+26),(x+27,y+7),12,2); line(d,(x+7,y+29),(x+29,y+9),13)
        elif slot==14: box(d,x+11,y+18,10,10,6); gem(d,x+16,y+12,12); line(d,(x+6,y+28),(x+12,y+21),6,2); line(d,(x+26,y+28),(x+20,y+21),6,2)
        elif slot==15: line(d,(x+4,y+25),(x+27,y+7),13,2); line(d,(x+7,y+29),(x+30,y+11),7,2)
        elif slot==16: line(d,(x+4,y+25),(x+27,y+7),12,2); line(d,(x+8,y+29),(x+31,y+11),13)
        elif slot==17: gem(d,x+16,y+16,12); line(d,(x+5,y+16),(x+27,y+16),11); line(d,(x+16,y+5),(x+16,y+27),11)
        elif slot==18: [line(d,(x+16,y+16),(x+2+k*7,y+3+(k&1)*22),7) for k in range(5)]
        else: d.arc((x+4,y+4,x+28,y+28),200,350,fill=PAL[13],width=2); d.arc((x+1,y+1,x+31,y+31),200,340,fill=PAL[7])
    # title emblem in final grid cell
    x,y=4*32,3*32; box(d,x+4,y+8,24,17,1); box(d,x+7,y+10,18,13,4); gem(d,x+16,y+16,7); line(d,(x+7,y+25),(x+16,y+30),6); line(d,(x+25,y+25),(x+16,y+30),6)
    im.save(OUT / "bosses-and-effects.png")

if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    tiles(); actors(); effects()
