// SPDX-License-Identifier: GPL-3.0-only
#ifndef MENU_RENDER_H
#define MENU_RENDER_H

#include <stdint.h>

enum {
    LAUNCHER_MENU_WIDTH = 536,
    LAUNCHER_MENU_HEIGHT = 240,
    LAUNCHER_MENU_STRIP_ROWS = 30,
    LAUNCHER_MENU_APP_COUNT = 9,
};

typedef struct {
    const char *name;
    const char *hint;
    const char *label;
    char shortcut;
    uint16_t color;
} launcher_menu_app_t;

extern const launcher_menu_app_t launcher_catalog[LAUNCHER_MENU_APP_COUNT];

void launcher_menu_render_strip(uint16_t *pixels, int y0, unsigned selected,
                                const launcher_menu_app_t *apps, unsigned app_count);

#endif
