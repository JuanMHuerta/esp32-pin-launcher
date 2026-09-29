#include "touch_map.h"

#include "life.h"

touch_cell_t touch_cell_from_raw(uint16_t raw_x, uint16_t raw_y)
{
    const int display_width = LIFE_WIDTH * LIFE_CELL_PIXELS;
    const int display_height = LIFE_HEIGHT * LIFE_CELL_PIXELS;
    const int raw_grid_x = (int)raw_x - LIFE_OFFSET_X;
    const int x = raw_grid_x < 0 ? 0 :
                  raw_grid_x >= display_width ? display_width - 1 : raw_grid_x;
    const int y = raw_y < display_height ? raw_y : display_height - 1;
    return (touch_cell_t) {
        .x = x / LIFE_CELL_PIXELS,
        .y = (display_height - 1 - y) / LIFE_CELL_PIXELS,
    };
}
