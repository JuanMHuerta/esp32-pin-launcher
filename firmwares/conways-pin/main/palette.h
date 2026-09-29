#pragma once

#include <stdint.h>

#include "life.h"

// RGB565 LED tiles indexed by Life state and pixel position within each cell.
void palette_build(uint16_t colors[LIFE_MAX_AGE + 1]
                   [LIFE_CELL_PIXELS * LIFE_CELL_PIXELS]);
