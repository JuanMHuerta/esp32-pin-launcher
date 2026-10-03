// SPDX-License-Identifier: GPL-3.0-only
#include "paint.h"
#include <math.h>
#include <string.h>

typedef struct {
    uint16_t *pixels;
} canvas_t;
/* Reduce before converting to float, keeping loops continuous even after months. */
static float phase(uint64_t ms, unsigned period)
{
    return (float)(ms % period) * (6.2831853f / period);
}
static float wave(uint64_t ms, unsigned period, float offset)
{
    return sinf(phase(ms, period) + offset);
}
uint16_t pet_rgb(unsigned c)
{
    return ((c >> 8) & 0xf800) | ((c >> 5) & 0x07e0) | ((c >> 3) & 0x001f);
}
static void rect(canvas_t *c, int x, int y, int w, int h, unsigned rgb)
{
    int right = x + w, bottom = y + h;
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (right > PET_W) {
        right = PET_W;
    }
    if (bottom > PET_H) {
        bottom = PET_H;
    }
    const uint16_t color = pet_rgb(rgb);
    for (int iy = y; iy < bottom; ++iy) {
        for (int ix = x; ix < right; ++ix) {
            c->pixels[iy * PET_W + ix] = color;
        }
    }
}
static void oval(canvas_t *c, int x, int y, int rx, int ry, unsigned rgb)
{
    if (rx < 1 || ry < 1) {
        return;
    }
    const int r = rx * rx * ry * ry;
    for (int dy = -ry; dy <= ry; ++dy) {
        int dx = rx;
        while (dx > 0 && dx * dx * ry * ry + dy * dy * rx * rx > r) {
            --dx;
        }
        rect(c, x - dx, y + dy, dx * 2 + 1, 1, rgb);
    }
}
static void line(canvas_t *c, int x0, int y0, int x1, int y1, unsigned rgb)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = -(y1 > y0 ? y1 - y0 : y0 - y1), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        rect(c, x0, y0, 1, 1, rgb);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e = err * 2;
        if (e >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}
static void heart(canvas_t *c, int x, int y, unsigned color)
{
    rect(c, x - 2, y, 2, 1, color);
    rect(c, x + 1, y, 2, 1, color);
    rect(c, x - 3, y + 1, 7, 2, color);
    rect(c, x - 2, y + 3, 5, 1, color);
    rect(c, x - 1, y + 4, 3, 1, color);
    rect(c, x, y + 5, 1, 1, color);
}
static void star(canvas_t *c, int x, int y, int r, unsigned color)
{
    rect(c, x - r, y, r * 2 + 1, 1, color);
    rect(c, x, y - r, 1, r * 2 + 1, color);
}
static unsigned mix(unsigned a, unsigned b, float t)
{
    unsigned out = 0;
    for (int k = 0; k < 3; ++k) {
        int av = (a >> (k * 8)) & 255, bv = (b >> (k * 8)) & 255;
        out |= (unsigned)(av + (bv - av) * t) << (k * 8);
    }
    return out;
}
static void berry(canvas_t *c, int x, int y, bool bitten)
{
    oval(c, x, y, 4, 4, 0x67324e);
    oval(c, x, y - 1, 3, 3, 0xf2818f);
    rect(c, x - 2, y - 3, 2, 2, 0xffd6b8);
    oval(c, x + 1, y - 5, 2, 1, 0x9cdb8e);
    if (bitten) {
        rect(c, x + 1, y - 1, 2, 2, 0xffe5b4);
    }
}
/* Localized light keeps the quiet background dark enough for the mascot. */
static void glow(canvas_t *c, int x, int y, int rx, int ry, unsigned color, float strength)
{
    for (int dy = -ry; dy <= ry; ++dy) {
        for (int dx = -rx; dx <= rx; ++dx) {
            int px = x + dx, py = y + dy;
            float d = (float)(dx * dx) / (rx * rx) + (float)(dy * dy) / (ry * ry);
            if (px < 0 || px >= PET_W || py < 0 || py >= PET_H || d >= 1) {
                continue;
            }
            uint16_t v = c->pixels[py * PET_W + px];
            unsigned rgb = (((v >> 11) * 255 / 31) << 16) | ((((v >> 5) & 63) * 255 / 63) << 8) |
                           ((v & 31) * 255 / 31);
            c->pixels[py * PET_W + px] = pet_rgb(mix(rgb, color, (1 - d) * (1 - d) * strength));
        }
    }
}
static void butterfly(canvas_t *c, int x, int y, uint64_t ms)
{
    int flap = wave(ms, 520, 0) > 0 ? 3 : 1;
    oval(c, x - flap, y - 1, flap, 3, 0xe99778);
    oval(c, x + flap, y - 1, flap, 3, 0xffd49b);
    oval(c, x - flap, y + 2, flap, 2, 0xce727d);
    oval(c, x + flap, y + 2, flap, 2, 0xf4b780);
    rect(c, x, y - 2, 1, 5, 0x39374a);
}
static void firefly(canvas_t *c, int x, int y, uint64_t ms)
{
    glow(c, x, y, 7, 6, 0xffd978, .28f + .09f * wave(ms, 1700, 0));
    star(c, x, y, 1, 0xffcc73);
    rect(c, x, y, 1, 1, 0xfff7c5);
    rect(c, x - 2, y - 2, 2, 1, 0x8ec9ba);
    rect(c, x + 1, y - 2, 2, 1, 0x8ec9ba);
}
static void fern(canvas_t *c, int x, int y, int facing, unsigned dark, unsigned light)
{
    line(c, x, y, x + facing * 2, y - 11, dark);
    for (int i = 0; i < 4; ++i) {
        int stem = x + facing * (i > 1);
        line(c, stem, y - 2 - i * 2, stem - 4 - i, y - 4 - i * 2, dark);
        line(c, stem, y - 2 - i * 2, stem + 4 + i, y - 5 - i * 2, light);
    }
}
static void mushroom(canvas_t *c, int x, int y, int size, float night)
{
    glow(c, x, y - 3, size * 2, size + 2, 0xffbb79, .07f + night * .22f);
    oval(c, x, y + 1, size + 2, 1, 0x192e35);
    rect(c, x - 1, y - size + 1, 3, size, 0xb9968c);
    rect(c, x, y - size + 1, 1, size - 1, 0xffdba8);
    oval(c, x, y - size, size, size / 2 + 1, 0x673e59);
    oval(c, x, y - size - 1, size - 1, size / 2, 0xd4787e);
    rect(c, x - size, y - size, size * 2 + 1, 1, 0xffbd98);
    rect(c, x - size + 2, y - size - 2, 2, 1, 0xffe7b8);
    rect(c, x + 1, y - size - 3, 2, 2, 0xffe7b8);
}
static void world(canvas_t *c, const pet_t *p)
{
    /* Four coordinated palettes: noon, sunset, midnight, dawn. */
    float t = (float)(p->uptime_ms % PET_DAY_MS) / (PET_DAY_MS / 4);
    int segment = (int)t, next = (segment + 1) % 4;
    float f = t - segment;
    f = f * f * (3 - 2 * f);
    static const unsigned tops[] = {0x285c68, 0x53364f, 0x101b34, 0x38485d};
    static const unsigned bottoms[] = {0x75ac98, 0xc28a78, 0x354969, 0xb0a18b};
    static const unsigned distant[] = {0x457f77, 0x79566b, 0x263653, 0x596e73};
    static const unsigned woods[] = {0x2f615c, 0x4a424f, 0x1c3047, 0x354e55};
    static const unsigned grounds[] = {0x34584c, 0x4b4644, 0x20353d, 0x3e514a};
    unsigned top = mix(tops[segment], tops[next], f);
    unsigned bottom = mix(bottoms[segment], bottoms[next], f);
    unsigned far = mix(distant[segment], distant[next], f);
    unsigned trees = mix(woods[segment], woods[next], f);
    unsigned ground = mix(grounds[segment], grounds[next], f);
    float night = 1 - pet_daylight(p);
    int drift = (int)lroundf(-p->tilt * 2);
    for (int y = 0; y < PET_H; ++y) {
        rect(c, 0, y, PET_W, 1, mix(top, bottom, fminf(1, y / 42.0f)));
    }
    if (night > .25f) {
        for (int i = 0; i < 13; ++i) {
            int x = (i * 31 + 39) % 110 + 12, y = 4 + (i * 11) % 25;
            star(c, x + drift, y, wave(p->uptime_ms, 4300, i * 1.7f) > .88f ? 1 : 0,
                 mix(mix(top, bottom, y / 42.0f), 0xd4e1dd, night * .7f));
        }
    }
    float horizon = night < .5f ? fminf(t, 4 - t) : fabsf(t - 2);
    int celestial_x = 108 + drift, celestial_y = 12 + (int)(30 * horizon * horizon);
    glow(c, celestial_x, celestial_y, 12, 11, 0xffd599, .17f);
    if (night < .5f) {
        unsigned sun = mix(0xffd596, 0xf7aa87, night * 2);
        oval(c, celestial_x, celestial_y, 5, 5, sun);
        for (int i = -1; i <= 1; i += 2) {
            rect(c, celestial_x + i * 8, celestial_y, 2, 1, sun);
            rect(c, celestial_x, celestial_y + i * 8, 1, 2, sun);
        }
        oval(c, celestial_x - 1, celestial_y - 1, 3, 3, 0xffe8b1);
    } else {
        /* Crescent cutout leaves the underlying gradient/stars intact. */
        for (int dy = -6; dy <= 6; ++dy) {
            for (int dx = -6; dx <= 6; ++dx) {
                if (dx * dx + dy * dy <= 36 && (dx - 3) * (dx - 3) + (dy + 2) * (dy + 2) > 27) {
                    rect(c, celestial_x + dx, celestial_y + dy, 1, 1, 0xffedbd);
                }
            }
        }
    }
    for (int i = 0; i < 2; ++i) {
        int x = (int)((p->uptime_ms / (850 + i * 240) + i * 73) % 174) - 23;
        unsigned cloud = mix(top, 0xc6d6bb, .25f * (1 - night));
        oval(c, x + drift, 15 + i * 7, 13, 2, cloud);
        oval(c, x + drift - 3, 13 + i * 7, 6, 3, cloud);
    }
    /* Distant ridges and staggered trunks establish depth without fine noise. */
    for (int x = 0; x < PET_W; ++x) {
        int h = (int)(3 * sinf((x + drift) * .065f) + 2 * sinf((x + drift) * .14f));
        rect(c, x, 33 + h, 1, PET_H, far);
    }
    for (int i = 0; i < 7; ++i) {
        int x = 7 + i * 20 + drift, y = 26 + i % 3 * 3;
        rect(c, x, y - 6, 2, 26, mix(far, trees, .55f));
        oval(c, x, y, 8 + i % 2 * 3, 10, mix(far, trees, .45f));
        oval(c, x - 4, y + 9, 10, 7, trees);
    }
    for (int x = 0; x < PET_W; ++x) {
        int y = 45 + (int)(2 * sinf(x * .085f));
        rect(c, x, y, 1, PET_H, ground);
    }
    /* Broad moss/path shapes read at a distance. */
    unsigned moss = mix(ground, 0x759567, .18f + .12f * (1 - night));
    oval(c, 67, 51, 45, 5, mix(ground, 0xa2a17b, .14f));
    oval(c, 24, 51, 21, 3, moss);
    oval(c, 108, 50, 17, 4, moss);
    rect(c, 0, 57, PET_W, 3, mix(ground, 0x101f2d, .45f));
    for (int i = 0; i < 15; ++i) {
        int x = (i * 37 + 8) % PET_W, y = 52 + i % 4;
        rect(c, x, y, 3 + i % 3, 1, i % 3 ? moss : mix(ground, 0xc0b286, .28f));
    }
    /* Close foliage frames an open center; its motion uses the same filtered tilt. */
    int sway = (int)lroundf(p->tilt * 2 + wave(p->uptime_ms, 9000, 0));
    for (int side = 0; side < 2; ++side) {
        int x = side ? 128 + sway : 3 + sway;
        unsigned bark = mix(0x183e3e, 0x152639, night);
        rect(c, x - 3, 0, 7, 48, bark);
        rect(c, x - 1, 8, 1, 34, mix(bark, 0x708570, .3f));
        line(c, x, 24, x + (side ? -10 : 10), 12, bark);
        oval(c, x, 2, 22, 8, trees);
        oval(c, x + (side ? -12 : 12), 0, 13, 6, mix(trees, 0x729078, .2f));
        oval(c, x + (side ? -6 : 6), 7, 12, 4, trees);
        oval(c, x + (side ? -14 : 14), 3, 5, 2, mix(trees, 0x84a478, .18f));
    }
    fern(c, 13 + sway, 51, 1, mix(0x467668, 0x335564, night), mix(0x8dab7d, 0x64888b, night));
    mushroom(c, 117, 50, 7, night);
    mushroom(c, 126, 52, 4, night);
    mushroom(c, 7, 54, 3, night);
    if (night > .4f) {
        for (int i = 0; i < 3; ++i) {
            int x = (int)(23 + i * 43 + 6 * wave(p->uptime_ms, 10000, i));
            int y = (int)(29 + 5 * wave(p->uptime_ms, 7000, i * 2.0f));
            firefly(c, x, y, p->uptime_ms + i * 800);
        }
    } else {
        butterfly(c, 24 + (int)(5 * wave(p->uptime_ms, 6000, 0)),
                  24 + (int)(3 * wave(p->uptime_ms, 4300, 0)), p->uptime_ms);
    }
}
static void foreground(canvas_t *c, const pet_t *p)
{
    unsigned dark = mix(0x1c3c39, 0x122736, 1 - pet_daylight(p));
    unsigned light = mix(0x5f8a62, 0x3c6469, 1 - pet_daylight(p));
    for (int i = 0; i < 10; ++i) {
        int x = i < 5 ? i * 6 : 107 + (i - 5) * 7;
        int sway = (int)lroundf(wave(p->uptime_ms, 5000, i) + p->tilt);
        line(c, x, 59, x - 2 + sway, 54 + i % 3, dark);
        line(c, x, 59, x + 3 + sway, 56 - i % 2, light);
    }
    for (int i = 0; i < 3; ++i) {
        int x = 20 + i * 47;
        line(c, x, 57, x, 54, light);
        oval(c, x, 54, 2, 1, 0xd99b88);
        rect(c, x, 54, 1, 1, 0xffdd9d);
    }
}
static void mascot(canvas_t *c, const pet_t *p)
{
    float night = 1 - pet_daylight(p);
    const unsigned ink = 0x16323b, shade = mix(0x529c8e, 0x559eac, night),
                   mint = mix(0xafe7b6, 0xa3e3cd, night), light = mix(0xedf6c5, 0xd9f2d8, night),
                   blush = 0xf2aa9b;
    float pose = fminf(1, p->state_ms / 300.0f);
    if (p->state_ms < p->duration_ms) {
        pose = fminf(pose, (p->duration_ms - p->state_ms) / 300.0f);
    }
    float rhythm = wave(p->uptime_ms, 1200, 0);
    float awake = 1 - p->sleep_blend;
    int step = (int)lroundf(sinf(p->gait) * fminf(fabsf(p->speed) / 4, 2));
    if (p->state == PET_DANCE) {
        step = (int)lroundf(rhythm * 3 * pose);
    } else if (p->state == PET_BALANCE) {
        step = (int)lroundf(p->tilt * 4 * pose);
    }
    int bob = (int)lroundf(wave(p->uptime_ms, 2400, 0) * .6f + fabsf((float)step) * .45f);
    int x = (int)lroundf(p->x), foot = 50 - (int)lroundf(p->lift);
    int y = foot - 21 + (int)lroundf(p->sleep_blend * 9) - bob;
    if (p->state == PET_STRETCH) {
        y -= (int)lroundf(3 * pose * (.75f + .25f * rhythm));
    } else if (p->state == PET_LOOK) {
        y -= (int)lroundf(2 * pose);
    }
    if (y < 22) {
        y = 22;
    }
    int lean =
        (int)lroundf(p->lean * awake +
                     (p->state == PET_SNIFF ? p->facing * (1 + wave(p->uptime_ms, 1500, 0)) : 0));
    if (p->state == PET_DANCE) {
        lean += (int)lroundf(rhythm * 3 * pose);
    } else if (p->state == PET_GROOM) {
        lean += (int)lroundf(wave(p->uptime_ms, 2100, 0) * 2 * pose);
    }
    int face_x = x + lean;
    int tail_sway = (int)lroundf(wave(p->uptime_ms, p->state == PET_DANCE ? 600 : 1800, 0) * 2);
    oval(c, x + 1, 51, 20 - (int)fminf(4, p->lift * .5f), 3, 0x243e3c);
    oval(c, x, 50, 17 - (int)fminf(4, p->lift * .5f), 2, 0x0b2029);
    // Tail, ears and limbs have a continuous one-pixel dark silhouette.
    oval(c, x - p->facing * 17, foot - 8 + tail_sway, 9, 6, ink);
    oval(c, x - p->facing * 18, foot - 9 + tail_sway, 7, 4, shade);
    oval(c, x - p->facing * 21, foot - 10 + tail_sway, 4, 3, mint);
    oval(c, x - p->facing * 22, foot - 11 + tail_sway, 2, 2, light);
    int ear_drop = (int)lroundf(p->sleep_blend * 4);
    for (int side = -1; side <= 1; side += 2) {
        int ex = face_x + side * 12;
        int ey = y - 11 + ear_drop + (int)lroundf(side * p->tilt * 2);
        oval(c, ex, ey, 5, 9 - ear_drop, ink);
        oval(c, ex, ey - 1, 3, 6 - ear_drop, mint);
        rect(c, ex - 1, ey - 5, 2, 6 - ear_drop, blush);
        rect(c, ex - 1, ey - 7, 2, 2, light);
    }
    oval(c, x, foot - 7, 12 + (int)(p->sleep_blend * 3), 9, ink);
    oval(c, x, foot - 8, 11 + (int)(p->sleep_blend * 3), 8, shade);
    oval(c, x - 1, foot - 9, 9, 7, mint);
    oval(c, x, foot - 6, 6, 5, light);
    oval(c, x - 7, foot - (step > 0 ? step : 0), 5, 2, ink);
    oval(c, x + 7, foot + (step < 0 ? step : 0), 5, 2, ink);
    rect(c, x - 10, foot - 1 - (step > 0 ? step : 0), 7, 2, mint);
    rect(c, x + 4, foot - 1 + (step < 0 ? step : 0), 7, 2, mint);
    // Rounded head with deliberately stepped edges and a cream muzzle.
    oval(c, face_x, y, 17, 12 - (int)(p->sleep_blend * 3), ink);
    oval(c, face_x, y - 1, 16, 11 - (int)(p->sleep_blend * 3), shade);
    oval(c, face_x - 1, y - 2, 15, 10 - (int)(p->sleep_blend * 3), mint);
    oval(c, face_x - 4, y - 6, 8, 4, light);
    oval(c, face_x, y + 5, 8, 4, light);
    line(c, face_x - 14, y - 4, face_x - 12, y - 7, light);
    line(c, face_x + 15, y - 2, face_x + 15, y + 3, 0x72bcad);
    int look = (int)lroundf(p->gaze * awake);
    bool happy = p->state == PET_LOVE || p->state == PET_DANCE || p->state == PET_STRETCH;
    bool shut = p->state == PET_SLEEP || happy || (p->blink_ms >= p->next_blink_ms);
    for (int side = -1; side <= 1; side += 2) {
        int ex = face_x + side * 7 + look;
        if (shut || (p->state == PET_GROOM && side == (rhythm > 0 ? 1 : -1))) {
            rect(c, ex - 2, y, 5, 1, ink);
            rect(c, ex - 3, y + (happy ? 1 : -1), 1, 1, ink);
            rect(c, ex + 3, y + (happy ? 1 : -1), 1, 1, ink);
        } else {
            oval(c, ex, y - 1, 3, p->state == PET_SURPRISE ? 5 : 4, 0x102b36);
            rect(c, ex - 1, y - 3, 2, 2, 0xf5ffe8);
            rect(c, ex + 1, y + 1, 1, 1, 0x4e8f88);
        }
        rect(c, face_x + side * 12 - 2, y + 4, 4, 2, blush);
    }
    rect(c, face_x - 1, y + 3, 3, 1, ink);
    if (p->state == PET_SURPRISE) {
        oval(c, face_x, y + 6, 2, 2, ink);
    } else if (happy) {
        oval(c, face_x, y + 6, 3, 2, ink);
        rect(c, face_x - 1, y + 7, 3, 1, blush);
    } else {
        rect(c, face_x, y + 4, 1, 2, ink);
        rect(c, face_x - 2, y + 6, 2, 1, ink);
        rect(c, face_x + 1, y + 6, 2, 1, ink);
    }
    // Tiny sprout, the mascot's identifying detail.
    int sprout_y = y + (int)lroundf(p->sleep_blend * 4);
    line(c, face_x, sprout_y - 11, face_x + 1, sprout_y - 16, ink);
    oval(c, face_x - 2, sprout_y - 16, 3, 1, 0x81c98c);
    oval(c, face_x + 3, sprout_y - 18, 3, 2, 0xc7e889);
    rect(c, face_x + 2, sprout_y - 19, 2, 1, 0xedf6b0);
    // Red-orange scarf is visible from across the room.
    int scarf_y = y + 11;
    rect(c, x - 12, scarf_y, 25, 4, ink);
    rect(c, x - 11, scarf_y + 1, 23, 3, 0xef927e);
    rect(c, x - p->facing * 12 - 2, scarf_y + 3, 5, 6, 0xd86f72);
    rect(c, x - p->facing * 12 - 2, scarf_y + 3, 2, 5, 0xffb995);
    int arm_y = y + 11 < foot - 2 ? y + 11 : foot - 2;
    for (int side = -1; side <= 1; side += 2) {
        int ax = x + side * 12, ay = arm_y;
        if (p->state == PET_WAVE && side == 1) {
            ay += (int)lroundf((wave(p->uptime_ms, 571, 0) * 2 - 9) * pose);
            ax += (int)lroundf(2 * pose);
        } else if (p->state == PET_STRETCH) {
            ax += side * (int)lroundf(6 * pose);
            ay -= (int)lroundf((15 + rhythm * 2) * pose);
        } else if (p->state == PET_DANCE) {
            ax += side * (int)lroundf(5 * pose);
            ay -= (int)lroundf((6 + rhythm * side * 6) * pose);
        } else if (p->state == PET_BALANCE) {
            ax += side * (int)lroundf(6 * pose);
            ay -= (int)lroundf((5 + side * p->tilt * 5) * pose);
        } else if (p->state == PET_GROOM && side == (rhythm > 0 ? 1 : -1)) {
            ax = face_x + side * 12;
            ay -= (int)lroundf((10 + fabsf(rhythm) * 4) * pose);
        }
        line(c, x + side * 10, arm_y, ax, ay, ink);
        oval(c, ax, ay, 4, 3, ink);
        oval(c, ax, ay - 1, 3, 2, mint);
        rect(c, ax - 1, ay - 2, 2, 1, light);
    }
    if (p->state == PET_EAT) {
        int chew = (p->state_ms / 180) % 2;
        berry(c, face_x, y + 9 + chew, p->state_ms > 1200);
        oval(c, face_x - 5, y + 10, 3, 2, mint);
        oval(c, face_x + 5, y + 10, 3, 2, mint);
        if (chew) {
            rect(c, face_x + 5, y + 5, 1, 1, 0xffd6b8);
        }
    }
    if (p->state == PET_SNIFF) {
        int fx = face_x + p->facing * 21;
        line(c, fx, foot, fx, foot - 8, 0x76b395);
        star(c, fx, foot - 9, 2, 0xf5c494);
        rect(c, fx, foot - 9, 1, 1, 0xfff5d1);
        rect(c, face_x + p->facing * 18, y + 5 + (p->state_ms / 250 % 2), 1, 1, 0xd1e7b0);
    }
    if (p->state == PET_LOVE) {
        for (int i = 0; i < 3; ++i) {
            float phase = fmodf(p->state_ms * .001f + i * .55f, 1.65f);
            heart(c, x + (i - 1) * 24, 26 - (int)(phase * 12), i == 1 ? 0xffc2a5 : 0xf08f9b);
        }
    }
    if (p->state == PET_SLEEP) {
        /* A pictorial dream replaces the old floating Z glyphs. */
        int dx = x > 87 ? x - 25 : x + 25;
        int dy = 20 + (int)lroundf(wave(p->uptime_ms, 3000, 0) * 2);
        oval(c, dx, dy, 8, 7, 0x315163);
        oval(c, dx, dy - 1, 7, 5, 0xb9d6cf);
        oval(c, dx - 3, dy + 9, 2, 2, 0xb9d6cf);
        berry(c, dx, dy - 1, false);
    }
    if (p->state == PET_SURPRISE) {
        for (int i = 0; i < 3; ++i) {
            float angle = phase(p->uptime_ms, 1600) + i * 2.094f;
            star(c, x + (int)(cosf(angle) * 23), 12 + (int)(sinf(angle) * 4), 2, 0xffd88a);
        }
    }
    if (p->state == PET_PLAY || p->state == PET_LOOK) {
        int fx =
            p->state == PET_PLAY ? (int)p->target_x : x + (int)(24 * wave(p->uptime_ms, 8000, 0));
        int fy = 13 + (int)(wave(p->uptime_ms, 2300, 0) * 3);
        if (night > .5f) {
            firefly(c, fx, fy, p->uptime_ms);
        } else {
            butterfly(c, fx, fy, p->uptime_ms);
        }
    }
    if (p->state == PET_DANCE || p->state == PET_STRETCH || p->state == PET_GROOM) {
        for (int i = 0; i < 2; ++i) {
            int sx = x + (i ? 25 : -25), sy = 29 + (int)(rhythm * (i ? 4 : -4));
            star(c, sx, sy, wave(p->uptime_ms, 1600, i * 3) > 0 ? 2 : 1, 0xf9d6a1);
        }
    }
    if (p->state == PET_BALANCE) {
        for (int i = 0; i < 3; ++i) {
            int lx = (int)((p->uptime_ms / 55 + i * 31) % 120) + 7;
            int ly = (int)((p->uptime_ms / 140 + i * 17) % 47) + 7;
            oval(c, lx, ly, 2, 1, i % 2 ? 0xdb9d78 : 0xa2bb7f);
        }
    }
}
void pet_paint(const pet_t *p, uint16_t pixels[PET_W * PET_H])
{
    canvas_t c = {pixels};
    world(&c, p);
    if (p->food) {
        berry(&c, (int)p->food_x, 47, false);
    }
    mascot(&c, p);
    foreground(&c, p);
}
bool pet_expand_strip(const uint16_t *pixels, int y, int rows, uint16_t *out)
{
    if (!pixels || !out || y < 0 || rows <= 0 || y > DISPLAY_H - rows || y % PET_SCALE ||
        rows % PET_SCALE) {
        return false;
    }
    for (int row = 0; row < rows; row += PET_SCALE) {
        uint16_t *dest = out + row * DISPLAY_W;
        const uint16_t *src = pixels + ((y + row) / PET_SCALE) * PET_W;
        for (int x = 0; x < PET_W; ++x) {
            uint16_t wire = (uint16_t)((src[x] << 8) | (src[x] >> 8));
            for (int sx = 0; sx < PET_SCALE; ++sx) {
                dest[x * PET_SCALE + sx] = wire;
            }
        }
        for (int sy = 1; sy < PET_SCALE; ++sy) {
            memcpy(dest + sy * DISPLAY_W, dest, DISPLAY_W * sizeof(uint16_t));
        }
    }
    return true;
}
