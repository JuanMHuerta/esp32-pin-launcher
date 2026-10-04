// SPDX-License-Identifier: GPL-3.0-only
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "menu_render.h"

static void render(uint16_t *frame, unsigned selected, const launcher_menu_app_t *apps,
                   unsigned app_count)
{
    uint16_t strip[LAUNCHER_MENU_WIDTH * LAUNCHER_MENU_STRIP_ROWS];
    for (int y0 = 0; y0 < LAUNCHER_MENU_HEIGHT; y0 += LAUNCHER_MENU_STRIP_ROWS) {
        launcher_menu_render_strip(strip, y0, selected, apps, app_count);
        for (int y = 0; y < LAUNCHER_MENU_STRIP_ROWS; ++y) {
            for (int x = 0; x < LAUNCHER_MENU_WIDTH; ++x) {
                frame[(y0 + y) * LAUNCHER_MENU_WIDTH + x] = strip[y * LAUNCHER_MENU_WIDTH + x];
            }
        }
    }
}

int main(void)
{
    static uint16_t frame[LAUNCHER_MENU_WIDTH * LAUNCHER_MENU_HEIGHT];
    render(frame, 0, launcher_catalog, LAUNCHER_MENU_APP_COUNT);
    assert(frame[52 * LAUNCHER_MENU_WIDTH + 20] == 0x2104);
    assert(frame[0] == 0x0000);
    assert(frame[16] == 0x0841);

    render(frame, 1, launcher_catalog, LAUNCHER_MENU_APP_COUNT);
    assert(frame[52 * LAUNCHER_MENU_WIDTH + 20] != 0x2104);
    assert(frame[80 * LAUNCHER_MENU_WIDTH + 20] == 0x2104);

    render(frame, LAUNCHER_MENU_APP_COUNT, launcher_catalog, LAUNCHER_MENU_APP_COUNT);
    assert(frame[80 * LAUNCHER_MENU_WIDTH + 20] == 0x2104);
    puts("launcher menu renderer passed");
    return 0;
}
