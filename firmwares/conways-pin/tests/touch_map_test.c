#include <assert.h>
#include <stdio.h>

#include "life.h"
#include "touch_map.h"

static void expect_cell(uint16_t raw_x, uint16_t raw_y, int x, int y)
{
    const touch_cell_t cell = touch_cell_from_raw(raw_x, raw_y);
    assert(cell.x == x);
    assert(cell.y == y);
}

int main(void)
{
    const int max_x = LIFE_OFFSET_X + LIFE_WIDTH * LIFE_CELL_PIXELS - 1;
    const int max_y = LIFE_HEIGHT * LIFE_CELL_PIXELS - 1;
    expect_cell(LIFE_OFFSET_X, max_y, 0, 0);
    expect_cell(max_x, max_y, LIFE_WIDTH - 1, 0);
    expect_cell(0, 0, 0, LIFE_HEIGHT - 1);
    expect_cell(max_x, 0, LIFE_WIDTH - 1, LIFE_HEIGHT - 1);
    expect_cell(LIFE_DISPLAY_WIDTH / 2, max_y / 2, LIFE_WIDTH / 2, LIFE_HEIGHT / 2);
    expect_cell(4095, 4095, LIFE_WIDTH - 1, 0);
    puts("Touch coordinates map to the expected screen corners: OK");
    return 0;
}
