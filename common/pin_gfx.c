// SPDX-License-Identifier: GPL-3.0-only
#include "pin_gfx.h"
#include <stdlib.h>
#include <string.h>
static int clamp(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}
uint16_t pin_rgb(int r, int g, int b)
{
    return (uint16_t)(((clamp(r, 0, 255) & 248) << 8) | ((clamp(g, 0, 255) & 252) << 3) |
                      (clamp(b, 0, 255) >> 3));
}
uint16_t pin_dim(uint16_t c, int a)
{
    a = clamp(a, 0, 256);
    return (uint16_t)(((((c >> 11) & 31) * a >> 8) << 11) | ((((c >> 5) & 63) * a >> 8) << 5) |
                      ((c & 31) * a >> 8));
}
void pin_rect(uint16_t *p, int x, int y, int w, int h, uint16_t c)
{
    if (!p || w <= 0 || h <= 0) {
        return;
    }
    int64_t right = (int64_t)x + w, bottom = (int64_t)y + h;
    int l = clamp(x, 0, PIN_W), r = right < 0 ? 0 : right > PIN_W ? PIN_W : (int)right;
    int t = clamp(y, 0, PIN_H), b = bottom < 0 ? 0 : bottom > PIN_H ? PIN_H : (int)bottom;
    for (int yy = t; yy < b; yy++) {
        for (int xx = l; xx < r; xx++) {
            p[yy * PIN_W + xx] = c;
        }
    }
}
void pin_line(uint16_t *p, int x0, int y0, int x1, int y1, uint16_t c)
{
    /* Reject unbounded coordinates before Bresenham; callers project inside viewport. */
    if (!p || x0 < -4096 || x0 > 4096 || x1 < -4096 || x1 > 4096 || y0 < -4096 || y0 > 4096 ||
        y1 < -4096 || y1 > 4096) {
        return;
    }
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1,
        e = dx + dy;
    for (;;) {
        if (x0 >= 0 && x0 < PIN_W && y0 >= 0 && y0 < PIN_H) {
            p[y0 * PIN_W + x0] = c;
        }
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * e;
        if (e2 >= dy) {
            e += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            e += dx;
            y0 += sy;
        }
    }
}
static const uint8_t *glyph(char ch)
{
    static const uint8_t blank[7] = {0};
    static const uint8_t alpha[26][7] = {
        {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30}, {14, 17, 16, 16, 16, 17, 14},
        {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
        {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17}, {14, 4, 4, 4, 4, 4, 14},
        {1, 1, 1, 1, 17, 17, 14},     {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
        {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17}, {14, 17, 17, 17, 17, 17, 14},
        {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
        {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},       {17, 17, 17, 17, 17, 17, 14},
        {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
        {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31}};
    static const uint8_t digits[10][7] = {{14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},
                                          {14, 17, 1, 2, 4, 8, 31},     {30, 1, 1, 14, 1, 1, 30},
                                          {2, 6, 10, 18, 31, 2, 2},     {31, 16, 16, 30, 1, 1, 30},
                                          {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},
                                          {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14}};
    if (ch >= 'A' && ch <= 'Z') {
        return alpha[ch - 'A'];
    }
    if (ch >= '0' && ch <= '9') {
        return digits[ch - '0'];
    }
    static const uint8_t lower[26][7] = {
        {0, 0, 14, 1, 15, 17, 15},  {16, 16, 30, 17, 17, 17, 30}, {0, 0, 14, 16, 16, 17, 14},
        {1, 1, 15, 17, 17, 17, 15}, {0, 0, 14, 17, 31, 16, 14},   {6, 8, 8, 28, 8, 8, 8},
        {0, 0, 15, 17, 15, 1, 14},  {16, 16, 30, 17, 17, 17, 17}, {4, 0, 12, 4, 4, 4, 14},
        {2, 0, 6, 2, 2, 18, 12},    {16, 16, 18, 20, 24, 20, 18}, {12, 4, 4, 4, 4, 4, 14},
        {0, 0, 26, 21, 21, 21, 21}, {0, 0, 30, 17, 17, 17, 17},   {0, 0, 14, 17, 17, 17, 14},
        {0, 0, 30, 17, 30, 16, 16}, {0, 0, 15, 17, 15, 1, 1},     {0, 0, 22, 25, 16, 16, 16},
        {0, 0, 15, 16, 14, 1, 30},  {8, 8, 28, 8, 8, 9, 6},       {0, 0, 17, 17, 17, 19, 13},
        {0, 0, 17, 17, 17, 10, 4},  {0, 0, 17, 17, 21, 21, 10},   {0, 0, 17, 10, 4, 10, 17},
        {0, 0, 17, 17, 15, 1, 14},  {0, 0, 31, 2, 4, 8, 31}};
    if (ch >= 'a' && ch <= 'z') {
        return lower[ch - 'a'];
    }
    switch (ch) {
    case '.': {
        static const uint8_t g[7] = {0, 0, 0, 0, 0, 0, 4};
        return g;
    }
    case ',': {
        static const uint8_t g[7] = {0, 0, 0, 0, 0, 4, 8};
        return g;
    }
    case ':': {
        static const uint8_t g[7] = {0, 4, 4, 0, 4, 4, 0};
        return g;
    }
    case ';': {
        static const uint8_t g[7] = {0, 4, 4, 0, 4, 4, 8};
        return g;
    }
    case '/': {
        static const uint8_t g[7] = {1, 2, 2, 4, 8, 8, 16};
        return g;
    }
    case '-': {
        static const uint8_t g[7] = {0, 0, 0, 31, 0, 0, 0};
        return g;
    }
    case '_': {
        static const uint8_t g[7] = {0, 0, 0, 0, 0, 0, 31};
        return g;
    }
    case '+': {
        static const uint8_t g[7] = {0, 4, 4, 31, 4, 4, 0};
        return g;
    }
    case '=': {
        static const uint8_t g[7] = {0, 0, 31, 0, 31, 0, 0};
        return g;
    }
    case '>': {
        static const uint8_t g[7] = {16, 8, 4, 2, 4, 8, 16};
        return g;
    }
    case '<': {
        static const uint8_t g[7] = {1, 2, 4, 8, 4, 2, 1};
        return g;
    }
    case '[': {
        static const uint8_t g[7] = {14, 8, 8, 8, 8, 8, 14};
        return g;
    }
    case ']': {
        static const uint8_t g[7] = {14, 2, 2, 2, 2, 2, 14};
        return g;
    }
    case '(': {
        static const uint8_t g[7] = {2, 4, 8, 8, 8, 4, 2};
        return g;
    }
    case ')': {
        static const uint8_t g[7] = {8, 4, 2, 2, 2, 4, 8};
        return g;
    }
    case '|': {
        static const uint8_t g[7] = {4, 4, 4, 4, 4, 4, 4};
        return g;
    }
    case '#': {
        static const uint8_t g[7] = {10, 31, 10, 10, 31, 10, 0};
        return g;
    }
    case '$': {
        static const uint8_t g[7] = {4, 15, 20, 14, 5, 30, 4};
        return g;
    }
    case '%': {
        static const uint8_t g[7] = {25, 26, 2, 4, 8, 11, 19};
        return g;
    }
    case '!': {
        static const uint8_t g[7] = {4, 4, 4, 4, 4, 0, 4};
        return g;
    }
    case '~': {
        static const uint8_t g[7] = {0, 0, 9, 22, 0, 0, 0};
        return g;
    }
    case '@': {
        static const uint8_t g[7] = {14, 17, 23, 21, 23, 16, 14};
        return g;
    }
    case '?': {
        static const uint8_t g[7] = {14, 17, 1, 2, 4, 0, 4};
        return g;
    }
    case '*': {
        static const uint8_t g[7] = {0, 21, 14, 31, 14, 21, 0};
        return g;
    }
    default:
        return blank;
    }
}

void pin_text(uint16_t *p, int x, int y, const char *s, uint16_t c)
{
    for (; *s; s++, x += 6) {
        const uint8_t *g = glyph(*s);
        for (int yy = 0; yy < 7; yy++) {
            for (int xx = 0; xx < 5; xx++) {
                if (g[yy] & (1u << (4 - xx))) {
                    pin_rect(p, x + xx, y + yy, 1, 1, c);
                }
            }
        }
    }
}
bool pin_expand_strip(const uint16_t *p, int y, int rows, uint16_t *out)
{
    if (!p || !out || y < 0 || rows <= 0 || y > PIN_DISPLAY_H - rows || y % 2 || rows % 2) {
        return false;
    }
    for (int r = 0; r < rows; r += 2) {
        uint16_t *dst = out + r * PIN_DISPLAY_W;
        const uint16_t *src = p + ((y + r) / 2) * PIN_W;
        for (int x = 0; x < PIN_W; x++) {
            dst[x * 2] = dst[x * 2 + 1] = __builtin_bswap16(src[x]);
        }
        memcpy(dst + PIN_DISPLAY_W, dst, PIN_DISPLAY_W * sizeof(*dst));
    }
    return true;
}
