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
    assert(colors[LIFE_ALIVE][5] == 0xffff); // white birth core
    assert(colors[LIFE_ALIVE + 1][5] != colors[LIFE_ALIVE + 1][1]); // cyan core + rim
    assert(colors[4][5] != colors[4][1]); // hot magenta core + deep-red rim
    assert(colors[1][5] != colors[4][5]); // death trace fades
    puts("Palette state transitions and cyber LED tiles: OK");
    return 0;
}
