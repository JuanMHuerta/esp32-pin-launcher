#include "paint.h"
#include <string.h>

typedef struct { uint16_t *pixels; } canvas_t;

static uint16_t rgb(unsigned c)
{ return ((c >> 8) & 0xf800) | ((c >> 5) & 0x07e0) | ((c >> 3) & 0x001f); }

static void rect(canvas_t *c, int x, int y, int w, int h, unsigned color)
{
    int r = x + w, b = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (r > MECH_W) r = MECH_W;
    if (b > MECH_H) b = MECH_H;
    for (int yy = y; yy < b; ++yy)
        for (int xx = x; xx < r; ++xx) c->pixels[yy * MECH_W + xx] = rgb(color);
}

static void line(canvas_t *c, int x0, int y0, int x1, int y1, unsigned color)
{
    int dx = x0 < x1 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = -(y0 < y1 ? y1 - y0 : y0 - y1), sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) {
        rect(c, x0, y0, 1, 1, color);
        if (x0 == x1 && y0 == y1) return;
        int twice = e * 2;
        if (twice >= dy) { e += dy; x0 += sx; }
        if (twice <= dx) { e += dx; y0 += sy; }
    }
}

static const uint8_t *glyph(char ch)
{
    static const uint8_t blank[7] = {0};
    static const uint8_t alpha[26][7] = {
        {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
        {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
        {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
        {1,1,1,1,17,17,14},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
        {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
        {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
    };
    static const uint8_t digit[10][7] = {
        {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
        {2,6,10,18,31,2,2},{31,16,30,1,1,17,14},{6,8,16,30,17,17,14},{31,1,2,4,8,8,8},
        {14,17,17,14,17,17,14},{14,17,17,15,1,2,12},
    };
    if (ch >= 'A' && ch <= 'Z') return alpha[ch - 'A'];
    if (ch >= '0' && ch <= '9') return digit[ch - '0'];
    return blank;
}

static void text(canvas_t *c, int x, int y, const char *s, int scale, unsigned color)
{
    for (; *s; ++s, x += 6 * scale) {
        const uint8_t *g = glyph(*s);
        for (int gy = 0; gy < 7; ++gy) for (int gx = 0; gx < 5; ++gx)
            if (g[gy] & (1 << (4 - gx))) rect(c, x + gx * scale, y + gy * scale, scale, scale, color);
    }
}

static void hangar(canvas_t *c, const mech_t *m)
{
    rect(c, 0, 0, MECH_W, MECH_H, 0x03070d);
    for (int x = -6; x < MECH_W; x += 18) {
        line(c, 67, 5, x, 56, 0x0b2330);
        line(c, 67, 5, x + 1, 56, 0x07151f);
    }
    for (int y = 12; y < 58; y += 9) line(c, 0, y, MECH_W - 1, y, 0x0a1c28);
    rect(c, 0, 56, MECH_W, 4, 0x102c36);
    rect(c, 0, 58, MECH_W, 2, 0x061117);
    for (int i = 0; i < 9; ++i) {
        int x = 8 + i * 15;
        rect(c, x, 53, 7, 1, (i + m->uptime_ms / 250) % 3 ? 0x155066 : 0x35b8d2);
    }
    rect(c, 2, 5, 2, 45, 0x17495d); rect(c, 130, 5, 2, 45, 0x17495d);
    rect(c, 0, 3, MECH_W, 2, 0x1b5368);
}

static void mech_body(canvas_t *c, const mech_t *m, int lift)
{
    int y = lift;
    unsigned edge = 0x77c5d1, armor = 0x234b5c, shadow = 0x102633, hot = 0xffaa2f;
    int stride = (int)((m->uptime_ms / 230) % 4) - 2;
    // Exhaust and a strong, readable black outline form the silhouette.
    if (m->state == MECH_POWERUP || m->state == MECH_LAUNCH) {
        int flame = m->state == MECH_LAUNCH ? 18 : 5 + (int)((m->uptime_ms / 90) % 5);
        rect(c, 57, y + 50, 7, flame, 0xf05225); rect(c, 60, y + 50, 2, flame, 0xffdb55);
        rect(c, 70, y + 50, 7, flame, 0xf05225); rect(c, 72, y + 50, 2, flame, 0xffdb55);
    }
    rect(c, 50, y + 43, 13, 11, 0x050b10); rect(c, 70, y + 43, 13, 11, 0x050b10);
    rect(c, 52, y + 43, 9, 9, shadow); rect(c, 72, y + 43, 9, 9, shadow);
    rect(c, 51 + stride, y + 51, 13, 3, edge); rect(c, 70 - stride, y + 51, 13, 3, edge);
    rect(c, 48, y + 23, 38, 24, 0x050b10);
    rect(c, 50, y + 24, 34, 20, armor); rect(c, 52, y + 26, 30, 16, 0x315f70);
    rect(c, 55, y + 28, 24, 6, 0x0d202b); rect(c, 57, y + 29, 20, 3, edge);
    rect(c, 64, y + 35, 8, 7, 0x0c202b); rect(c, 66, y + 36, 4, 2, hot);
    rect(c, 40, y + 26, 10, 16, 0x050b10); rect(c, 84, y + 26, 10, 16, 0x050b10);
    rect(c, 41, y + 28, 8, 11, armor); rect(c, 85, y + 28, 8, 11, armor);
    rect(c, 36, y + 37, 8, 5, edge); rect(c, 90, y + 37, 8, 5, edge);
    rect(c, 54, y + 13, 28, 12, 0x040a10); rect(c, 56, y + 14, 24, 10, armor);
    rect(c, 58, y + 16, 20, 4, 0x86dbe5); rect(c, 60, y + 17, 16, 2, 0xc8f7ff);
    rect(c, 52, y + 11, 4, 5, edge); rect(c, 78, y + 11, 4, 5, edge);
    rect(c, 46, y + 45, 5, 2, edge); rect(c, 83, y + 45, 5, 2, edge);
}

void mech_paint(const mech_t *m, uint16_t pixels[MECH_W * MECH_H])
{
    canvas_t c = {pixels};
    hangar(&c, m);
    if (m->state == MECH_TITLE) {
        int flicker = (m->state_ms / 120) % 5 == 0;
        rect(&c, 0, 0, MECH_W, MECH_H, 0x020408);
        text(&c, 34, 16, "MECH", 3, flicker ? 0x3b8fa1 : 0x91e5ed);
        text(&c, 48, 42, "BAY 07", 1, 0xf1a63a);
        for (int x = 28; x < 106; x += 9) rect(&c, x, 54, 5, 1, 0x275766);
        return;
    }
    int lift = 0;
    if (m->state == MECH_LAUNCH) lift = -((int)m->state_ms * 76 / 4100);
    if (m->state == MECH_RETURN) lift = -76 + ((int)m->state_ms * 76 / 3000);
    mech_body(&c, m, lift);
    if (m->state == MECH_SCAN) {
        int y = 9 + (int)(m->state_ms * 42 / 2600);
        rect(&c, 6, y, 122, 1, 0x4ceeff); rect(&c, 44, y - 1, 48, 3, 0x216e82);
    }
    if (m->state == MECH_POWERUP) {
        int n = 3 + (int)(m->state_ms / 180 % 7);
        for (int i = 0; i < n; ++i) {
            int x = 43 + (i * 17 + (int)(m->uptime_ms / 20)) % 49;
            line(&c, x, 48, 67, 38, i & 1 ? 0xffc344 : 0xf05225);
        }
        text(&c, 7, 7, "ARMED", 1, 0xffc344);
    }
    if (m->state == MECH_LAUNCH) text(&c, 39, 7, "LAUNCH", 1, 0xffc344);
    if (m->state == MECH_RETURN) text(&c, 43, 7, "RETURN", 1, 0x73cbd9);
    if (m->state == MECH_IDLE) text(&c, 5, 7, "BAY 07", 1, 0x5e8993);
}

bool mech_expand_strip(const uint16_t *pixels, int y, int rows, uint16_t *out)
{
    if (!pixels || !out || y < 0 || rows <= 0 || y > DISPLAY_H - rows ||
        y % MECH_SCALE || rows % MECH_SCALE) return false;
    for (int row = 0; row < rows; row += MECH_SCALE) {
        uint16_t *dest = out + row * DISPLAY_W;
        const uint16_t *src = pixels + ((y + row) / MECH_SCALE) * MECH_W;
        for (int x = 0; x < MECH_W; ++x) {
            uint16_t wire = (uint16_t)((src[x] << 8) | (src[x] >> 8));
            for (int sx = 0; sx < MECH_SCALE; ++sx) dest[x * MECH_SCALE + sx] = wire;
        }
        for (int sy = 1; sy < MECH_SCALE; ++sy)
            memcpy(dest + sy * DISPLAY_W, dest, DISPLAY_W * sizeof(*dest));
    }
    return true;
}
