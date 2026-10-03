// SPDX-License-Identifier: GPL-3.0-only
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "life.h"
#include "palette.h"

static uint16_t colors[LIFE_MAX_AGE + 1][LIFE_CELL_PIXELS * LIFE_CELL_PIXELS];

int main(void)
{
    palette_build(colors);
    for (int pixel = 0; pixel < LIFE_CELL_PIXELS * LIFE_CELL_PIXELS; ++pixel) {
        assert(colors[0][pixel] == 0);
        assert(colors[LIFE_ALIVE + 1][pixel] == colors[LIFE_MAX_AGE][pixel]);
    }
    const int core = 2 * LIFE_CELL_PIXELS + 2;
    const int rim = 2;
    assert(colors[LIFE_ALIVE][core] == 0xffff);
    assert(colors[LIFE_ALIVE + 1][core] != colors[LIFE_ALIVE + 1][rim]);
    assert(colors[4][core] != colors[4][rim]);
    assert(colors[1][core] != colors[4][core]);
    puts("Palette state transitions and cyber LED tiles: OK");
    return 0;
}
