// SPDX-License-Identifier: GPL-3.0-only
#include <stdint.h>
#include <stdio.h>

#include "menu_render.h"

int main(void)
{
    static uint16_t strip[LAUNCHER_MENU_WIDTH * LAUNCHER_MENU_STRIP_ROWS];
    static uint16_t frame[LAUNCHER_MENU_WIDTH * LAUNCHER_MENU_HEIGHT];
    uint8_t row[LAUNCHER_MENU_WIDTH * 3];

    for (unsigned selected = 0; selected <= LAUNCHER_MENU_APP_COUNT; ++selected) {
        for (int y0 = 0; y0 < LAUNCHER_MENU_HEIGHT; y0 += LAUNCHER_MENU_STRIP_ROWS) {
            launcher_menu_render_strip(strip, y0, selected, launcher_catalog,
                                       LAUNCHER_MENU_APP_COUNT);
            for (int y = 0; y < LAUNCHER_MENU_STRIP_ROWS; ++y) {
                for (int x = 0; x < LAUNCHER_MENU_WIDTH; ++x) {
                    frame[(y0 + y) * LAUNCHER_MENU_WIDTH + x] = strip[y * LAUNCHER_MENU_WIDTH + x];
                }
            }
        }

        for (int y = 0; y < LAUNCHER_MENU_HEIGHT; ++y) {
            for (int x = 0; x < LAUNCHER_MENU_WIDTH; ++x) {
                const uint16_t pixel = frame[y * LAUNCHER_MENU_WIDTH + x];
                row[x * 3] = (uint8_t)(((pixel >> 11) & 0x1f) * 255 / 31);
                row[x * 3 + 1] = (uint8_t)(((pixel >> 5) & 0x3f) * 255 / 63);
                row[x * 3 + 2] = (uint8_t)((pixel & 0x1f) * 255 / 31);
            }
            if (fwrite(row, 1, sizeof(row), stdout) != sizeof(row)) {
                return 1;
            }
        }
    }
    return 0;
}
