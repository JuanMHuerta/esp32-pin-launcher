// SPDX-License-Identifier: GPL-3.0-only
#include "menu_render.h"

static const uint8_t *glyph(char c)
{
    static const uint8_t space[] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t letters[][7] = {
        {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30}, {14, 17, 16, 16, 16, 17, 14},
        {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
        {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17}, {14, 4, 4, 4, 4, 4, 14},
        {1, 1, 1, 1, 17, 17, 14},     {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
        {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17}, {14, 17, 17, 17, 17, 17, 14},
        {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
        {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},       {17, 17, 17, 17, 17, 17, 14},
        {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
        {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31},
    };
    static const uint8_t digits[][7] = {
        {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},  {14, 17, 1, 2, 4, 8, 31},
        {30, 1, 1, 14, 1, 1, 30},     {2, 6, 10, 18, 31, 2, 2}, {31, 16, 16, 30, 1, 1, 30},
        {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},   {14, 17, 17, 14, 17, 17, 14},
        {14, 17, 17, 15, 1, 1, 14},
    };
    if (c >= 'A' && c <= 'Z') {
        return letters[c - 'A'];
    }
    if (c >= '0' && c <= '9') {
        return digits[c - '0'];
    }
    return space;
}

static void text(uint16_t *pixels, int y0, int x, int y, const char *s, uint16_t color, int scale)
{
    for (; *s; ++s, x += 6 * scale) {
        const uint8_t *rows = glyph(*s);
        for (int gy = 0; gy < 7; ++gy) {
            for (int gx = 0; gx < 5; ++gx) {
                if (!(rows[gy] & (1 << (4 - gx)))) {
                    continue;
                }
                for (int sy = 0; sy < scale; ++sy) {
                    for (int sx = 0; sx < scale; ++sx) {
                        const int px = x + gx * scale + sx;
                        const int py = y + gy * scale + sy;
                        if (px >= 0 && px < LAUNCHER_MENU_WIDTH && py >= y0 &&
                            py < y0 + LAUNCHER_MENU_STRIP_ROWS) {
                            pixels[(py - y0) * LAUNCHER_MENU_WIDTH + px] = color;
                        }
                    }
                }
            }
        }
    }
}

void launcher_menu_render_strip(uint16_t *pixels, int y0, unsigned selected,
                                const launcher_menu_app_t *apps, unsigned app_count)
{
    for (int y = 0; y < LAUNCHER_MENU_STRIP_ROWS; ++y) {
        for (int x = 0; x < LAUNCHER_MENU_WIDTH; ++x) {
            pixels[y * LAUNCHER_MENU_WIDTH + x] = ((x / 16 + (y + y0) / 16) & 1) ? 0x0841 : 0x0000;
        }
    }
    text(pixels, y0, 28, 9, "PIN LIBRARY", 0xffff, 2);
    text(pixels, y0, 30, 32, "SHORT PRESS SELECT", 0x8410, 1);
    text(pixels, y0, 30, 43, "HOLD BOOT TO START", 0x8410, 1);
    text(pixels, y0, 400, 32,
         app_count < 8 ? "PAGE 1 OF 1" : (selected < 8 ? "PAGE 1 OF 2" : "PAGE 2 OF 2"), 0x8410, 1);
    for (unsigned i = 0; i < app_count; ++i) {
        const unsigned page_start = selected < 8 ? 0 : 8;
        if (i < page_start || i >= page_start + 8) {
            continue;
        }
        const int row = 54 + (int)(i - page_start) * 22;
        const uint16_t ink = i == selected ? apps[i].color : 0x7bef;
        if (i == selected) {
            for (int yy = row - 3; yy < row + 20; ++yy) {
                if (yy >= y0 && yy < y0 + LAUNCHER_MENU_STRIP_ROWS) {
                    for (int xx = 18; xx < 518; ++xx) {
                        pixels[(yy - y0) * LAUNCHER_MENU_WIDTH + xx] = 0x2104;
                    }
                }
            }
        }
        text(pixels, y0, 34, row, apps[i].name, ink, 2);
        text(pixels, y0, 180, row + 5, apps[i].hint, 0xbdf7, 1);
    }
    if (app_count && (app_count < 8 || selected >= 8)) {
        const unsigned page_start = selected < 8 ? 0 : 8;
        const int row = 54 + (int)(app_count - page_start) * 22;
        const uint16_t ink = selected == app_count ? 0xffe0 : 0x7bef;
        if (selected == app_count) {
            for (int yy = row - 3; yy < row + 20; ++yy) {
                if (yy >= y0 && yy < y0 + LAUNCHER_MENU_STRIP_ROWS) {
                    for (int xx = 18; xx < 518; ++xx) {
                        pixels[(yy - y0) * LAUNCHER_MENU_WIDTH + xx] = 0x2104;
                    }
                }
            }
        }
        text(pixels, y0, 34, row, "DEMO", ink, 2);
        text(pixels, y0, 180, row + 5, "5 MINUTES EACH - INSTALLED APPS", 0xbdf7, 1);
    }
    if (!app_count) {
        text(pixels, y0, 34, 80, "NO APPS INSTALLED", 0xffff, 2);
        text(pixels, y0, 34, 110, "INSTALL APPS WITH THE WEB FLASHER", 0xbdf7, 1);
    }
}

const launcher_menu_app_t launcher_catalog[LAUNCHER_MENU_APP_COUNT] = {
    {"CONWAY", "GAME OF LIFE", "conways", '1', 0x07ff},
    {"FLUID", "MOTION WATER", "fluid", '2', 0x04ff},
    {"MISO", "WOODLAND PET", "miso", '3', 0xffe0},
    {"LUMEN", "STARFIELD", "lumen", '4', 0xf81f},
    {"DUNGEON", "SEED CRAWLER", "dungeon", '5', 0x07f0},
    {"3D MAZE", "CLASSIC WALK", "maze", '6', 0xfd20},
    {"WAYFARER", "PIXEL HAULER", "wayfarer", '7', 0xffdf},
    {"3 BODY", "GRAVITY LAB", "threebody", '8', 0xfdae},
    {"CRT", "BOOT TERMINAL", "crt", '9', 0x07f0},
};
