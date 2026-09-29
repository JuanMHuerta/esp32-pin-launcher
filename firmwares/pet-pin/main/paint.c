#include "paint.h"
#include <math.h>
#include <string.h>

typedef struct { uint16_t *pixels; } canvas_t;
/* Reduce before converting to float, keeping loops continuous even after months. */
static float phase(uint64_t ms, unsigned period)
{ return (float)(ms % period) * (6.2831853f / period); }
static float wave(uint64_t ms, unsigned period, float offset)
{ return sinf(phase(ms, period) + offset); }
uint16_t pet_rgb(unsigned c)
{ return ((c >> 8) & 0xf800) | ((c >> 5) & 0x07e0) | ((c >> 3) & 0x001f); }
static void rect(canvas_t *c, int x, int y, int w, int h, unsigned rgb)
{
    int right = x + w, bottom = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (right > PET_W) right = PET_W;
    if (bottom > PET_H) bottom = PET_H;
    const uint16_t color = pet_rgb(rgb);
    for (int iy = y; iy < bottom; ++iy)
        for (int ix = x; ix < right; ++ix) c->pixels[iy * PET_W + ix] = color;
}
static void oval(canvas_t *c, int x, int y, int rx, int ry, unsigned rgb)
{
    if (rx < 1 || ry < 1) return;
    const int r = rx * rx * ry * ry;
    for (int dy = -ry; dy <= ry; ++dy) {
        int dx = rx;
        while (dx > 0 && dx * dx * ry * ry + dy * dy * rx * rx > r) --dx;
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
        if (x0 == x1 && y0 == y1) break;
        int e = err * 2;
        if (e >= dy) { err += dy; x0 += sx; }
        if (e <= dx) { err += dx; y0 += sy; }
    }
}
static void heart(canvas_t *c, int x, int y, unsigned color)
{
    rect(c, x - 2, y, 2, 1, color); rect(c, x + 1, y, 2, 1, color);
    rect(c, x - 3, y + 1, 7, 2, color); rect(c, x - 2, y + 3, 5, 1, color);
    rect(c, x - 1, y + 4, 3, 1, color); rect(c, x, y + 5, 1, 1, color);
}
static void star(canvas_t *c, int x, int y, int r, unsigned color)
{ rect(c, x - r, y, r * 2 + 1, 1, color); rect(c, x, y - r, 1, r * 2 + 1, color); }
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
    oval(c, x, y, 3, 3, 0x77395b); oval(c, x, y - 1, 2, 2, 0xff8090);
    rect(c, x - 1, y - 2, 1, 1, 0xffd6b8);
    line(c, x, y - 3, x + 2, y - 5, 0x8adc9a);
    if (bitten) rect(c, x + 1, y - 1, 2, 2, 0xffe5b4);
}
static void world(canvas_t *c, const pet_t *p)
{
    const float t = (float)(p->uptime_ms % 360000) / 90000;
    const int phase = (int)t;
    const float blend = (t - phase) * (t - phase) * (3 - 2 * (t - phase));
    static const unsigned skies[] = {0x101e2d, 0x241d36, 0x0d1328, 0x142e35};
    static const unsigned hills[] = {0x203c46, 0x393349, 0x20263f, 0x234547};
    unsigned sky = mix(skies[phase], skies[(phase + 1) % 4], blend);
    unsigned hill = mix(hills[phase], hills[(phase + 1) % 4], blend);
    rect(c, 0, 0, PET_W, PET_H, sky);
    /* Restrained scenery gives the face almost the entire vertical canvas. */
    for (int x = 0; x < PET_W; ++x) {
        int h = (int)(4 * sinf(x * .057f) + 2 * sinf(x * .13f));
        rect(c, x, 41 + h, 1, 19 - h, hill);
    }
    rect(c, 0, 50, PET_W, 10, 0x172c32);
    rect(c, 0, 50, PET_W, 1, 0x39605b);
    rect(c, 0, 57, PET_W, 3, 0x102127);
    for (int i = 0; i < 20; ++i) {
        int x = (i * 37 + 8) % PET_W;
        rect(c, x, 53 + (i * 7 % 4), 2, 1, i % 3 ? 0x29413f : 0x41635a);
    }
    /* A compact name stitched into the upper left; no UI obscures the pet. */
    static const uint8_t name[4][5] = {{17,27,21,17,17},{7,2,2,2,7},
                                      {7,4,7,1,7},{7,5,5,5,7}};
    for (int n = 0; n < 4; ++n)
        for (int y = 0; y < 5; ++y)
            for (int x = 0; x < (n ? 3 : 5); ++x)
                if (name[n][y] & (1 << ((n ? 2 : 4) - x)))
                    rect(c, 7 + (n ? 6 + (n - 1) * 4 : 0) + x, 6 + y, 1, 1, 0x86afa7);
    float night = .5f - .5f * cosf(t * 1.5707963f);
    oval(c, 119, 12, 5, 5, mix(0xf8bc7b, 0xffedbb, night));
    int crescent = (int)lroundf(fmaxf(0, (night - .3f) / .7f) * 4);
    if (crescent) oval(c, 121, 10, crescent, crescent, sky);
    for (int i = 0; i < 8; ++i) {
        int x = (i * 31 + 33) % 123 + 5, y = 5 + (i * 11) % 24;
        float twinkle = wave(p->uptime_ms, 3700, i * 2.3f);
        star(c, x, y, twinkle > .8f ? 1 : 0, mix(0x3c606c, 0x71849d, night));
    }
    int cloud = (int)((p->uptime_ms / 500) % 170) - 20;
    rect(c, cloud, 18, 15, 2, mix(sky, hill, .5f));
    rect(c, cloud + 4, 16, 7, 2, mix(sky, hill, .5f));
    // Fern on the left and a warm mushroom lantern on the right.
    line(c, 14, 48, 14, 37, 0x63917b);
    for (int i = 0; i < 3; ++i) {
        line(c, 14, 41 + i * 3, 9 - i, 37 + i * 3, 0x629c88);
        line(c, 14, 43 + i * 2, 19 + i, 39 + i * 2, 0x7dc39b);
    }
    rect(c, 111, 44, 3, 6, 0xd6bd9d);
    oval(c, 112, 43, 6, 3, 0xc96d73);
    rect(c, 106, 44, 13, 1, 0xffb99d);
    rect(c, 109, 41, 2, 1, 0xffe0af); rect(c, 114, 42, 2, 1, 0xffe0af);
    for (int i = 0; i < 3; ++i) {
        int x = (int)(22 + i * 43 + 5 * wave(p->uptime_ms, 10000, (float)i));
        int y = (int)(29 + 6 * wave(p->uptime_ms, 7000, i * 2.0f));
        rect(c, x, y, 1, 1, 0xcde79a);
    }
}
static void mascot(canvas_t *c, const pet_t *p)
{
    const unsigned ink = 0x183d42, shade = 0x58a99c, mint = 0xa0e4bc,
                   light = 0xdbf4c8, blush = 0xf2aa9b;
    float awake = 1 - p->sleep_blend;
    int step = (int)lroundf(sinf(p->gait) * fminf(fabsf(p->speed) / 4, 2));
    int bob = (int)lroundf(wave(p->uptime_ms, 2400, 0) * .6f + fabsf((float)step) * .45f);
    int x = (int)lroundf(p->x), foot = 49 - (int)lroundf(p->lift);
    int y = foot - 17 + (int)lroundf(p->sleep_blend * 9) - bob;
    int lean = (int)lroundf(p->lean * awake +
               (p->state == PET_SNIFF ? p->facing * (1 + wave(p->uptime_ms, 1500, 0)) : 0));
    int face_x = x + lean;
    int tail_sway = (int)lroundf(wave(p->uptime_ms, 1800, 0) * 2);
    oval(c, x, 50, 17 - (int)fminf(4, p->lift * .5f), 2, 0x0b2029);
    // Tail, ears and limbs have a continuous one-pixel dark silhouette.
    oval(c, x - p->facing * 17, foot - 8 + tail_sway, 9, 6, ink);
    oval(c, x - p->facing * 18, foot - 9 + tail_sway, 7, 4, shade);
    oval(c, x - p->facing * 21, foot - 10 + tail_sway, 4, 3, mint);
    int ear_drop = (int)lroundf(p->sleep_blend * 4);
    oval(c, face_x - 12, y - 11 + ear_drop, 5, 9 - ear_drop, ink);
    oval(c, face_x + 12, y - 11 + ear_drop, 5, 9 - ear_drop, ink);
    oval(c, face_x - 12, y - 12 + ear_drop, 3, 6 - ear_drop, mint);
    oval(c, face_x + 12, y - 12 + ear_drop, 3, 6 - ear_drop, mint);
    rect(c, face_x - 13, y - 16 + ear_drop, 2, 6 - ear_drop, blush);
    rect(c, face_x + 11, y - 16 + ear_drop, 2, 6 - ear_drop, blush);
    oval(c, x, foot - 7, 12 + (int)(p->sleep_blend * 3), 9, ink);
    oval(c, x, foot - 8, 11 + (int)(p->sleep_blend * 3), 8, shade);
    oval(c, x - 1, foot - 9, 9, 7, mint);
    oval(c, x, foot - 6, 6, 5, light);
    oval(c, x - 7, foot - (step > 0 ? step : 0), 5, 2, ink);
    oval(c, x + 7, foot + (step < 0 ? step : 0), 5, 2, ink);
    rect(c, x - 10, foot - 1 - (step > 0 ? step : 0), 7, 2, mint);
    rect(c, x + 4, foot - 1 + (step < 0 ? step : 0), 7, 2, mint);
    // Red-orange scarf is visible from across the room.
    int scarf_y = y + 7 < foot - 8 ? y + 7 : foot - 8;
    rect(c, x - 12, scarf_y, 25, 5, ink);
    rect(c, x - 11, scarf_y + 1, 23, 3, 0xef927e);
    rect(c, x - p->facing * 12 - 2, scarf_y + 3, 5, 6, 0xd86f72);
    rect(c, x - p->facing * 12 - 2, scarf_y + 3, 2, 5, 0xffb995);
    // Rounded head with deliberately stepped edges and a cream muzzle.
    oval(c, face_x, y, 17, 12 - (int)(p->sleep_blend * 3), ink);
    oval(c, face_x, y - 1, 16, 11 - (int)(p->sleep_blend * 3), shade);
    oval(c, face_x - 1, y - 2, 15, 10 - (int)(p->sleep_blend * 3), mint);
    oval(c, face_x - 4, y - 6, 8, 4, light);
    oval(c, face_x, y + 5, 8, 4, light);
    int look = (int)lroundf(p->gaze * awake);
    bool shut = p->state == PET_SLEEP || p->state == PET_LOVE ||
                (p->blink_ms >= p->next_blink_ms);
    for (int side = -1; side <= 1; side += 2) {
        int ex = face_x + side * 7 + look;
        if (shut) {
            rect(c, ex - 2, y, 5, 1, ink);
            rect(c, ex - 3, y + (p->state == PET_LOVE ? 1 : -1), 1, 1, ink);
            rect(c, ex + 3, y + (p->state == PET_LOVE ? 1 : -1), 1, 1, ink);
        } else {
            oval(c, ex, y - 1, 3, p->state == PET_SURPRISE ? 5 : 4, 0x102b36);
            rect(c, ex - 1, y - 3, 2, 2, 0xf5ffe8);
            rect(c, ex + 1, y + 1, 1, 1, 0x4e8f88);
        }
        rect(c, face_x + side * 12 - 2, y + 4, 4, 2, blush);
    }
    rect(c, face_x - 1, y + 3, 3, 1, ink);
    if (p->state == PET_SURPRISE) oval(c, face_x, y + 6, 2, 2, ink);
    else {
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
    int arm = p->state == PET_WAVE ? (int)lroundf(wave(p->uptime_ms, 571, 0) * 2) - 7 : 0;
    int arm_y = y + 11 < foot - 2 ? y + 11 : foot - 2;
    oval(c, x + 12, arm_y + arm, 4, 3, ink);
    oval(c, x + 12, arm_y - 1 + arm, 3, 2, mint);
    if (p->state == PET_EAT) {
        int chew = (p->state_ms / 180) % 2;
        berry(c, face_x, y + 9 + chew, p->state_ms > 1200);
        oval(c, face_x - 5, y + 10, 3, 2, mint);
        oval(c, face_x + 5, y + 10, 3, 2, mint);
        if (chew) rect(c, face_x + 5, y + 5, 1, 1, 0xffd6b8);
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
            heart(c, x + (i - 1) * 24, 26 - (int)(phase * 12),
                  i == 1 ? 0xffc2a5 : 0xf08f9b);
        }
    }
    if (p->state == PET_SLEEP) {
        int dy = (int)(p->state_ms / 350 % 5);
        for (int i = 0; i < 2; ++i) {
            int zx = x + 20 + i * 7, zy = 22 - dy - i * 6;
            rect(c, zx, zy, 4, 1, 0xa8c4cd);
            line(c, zx + 3, zy, zx, zy + 3, 0xa8c4cd);
            rect(c, zx, zy + 3, 4, 1, 0xa8c4cd);
        }
    }
    if (p->state == PET_SURPRISE) {
        for (int i = 0; i < 3; ++i) {
            float angle = phase(p->uptime_ms, 1600) + i * 2.094f;
            star(c, x + (int)(cosf(angle) * 23), 12 + (int)(sinf(angle) * 4), 2, 0xffd88a);
        }
    }
    if (p->state == PET_PLAY) {
        int fx = (int)p->target_x, fy = 22 + (int)(wave(p->uptime_ms, 1250, 0) * 4);
        star(c, fx, fy, 2, 0xffe8a2);
        rect(c, fx - 2, fy - 2, 1, 1, 0xa1d1ae);
        rect(c, fx + 2, fy - 2, 1, 1, 0xa1d1ae);
    }
}
void pet_paint(const pet_t *p, uint16_t pixels[PET_W * PET_H])
{
    canvas_t c = {pixels};
    world(&c, p);
    if (p->food) berry(&c, (int)p->food_x, 47, false);
    mascot(&c, p);
}
bool pet_expand_strip(const uint16_t *pixels, int y, int rows, uint16_t *out)
{
    if (!pixels || !out || y < 0 || rows <= 0 || y > DISPLAY_H - rows ||
        y % PET_SCALE || rows % PET_SCALE) return false;
    for (int row = 0; row < rows; row += PET_SCALE) {
        uint16_t *dest = out + row * DISPLAY_W;
        const uint16_t *src = pixels + ((y + row) / PET_SCALE) * PET_W;
        for (int x = 0; x < PET_W; ++x) {
            uint16_t wire = (uint16_t)((src[x] << 8) | (src[x] >> 8));
            for (int sx = 0; sx < PET_SCALE; ++sx) dest[x * PET_SCALE + sx] = wire;
        }
        for (int sy = 1; sy < PET_SCALE; ++sy)
            memcpy(dest + sy * DISPLAY_W, dest, DISPLAY_W * sizeof(uint16_t));
    }
    return true;
}
