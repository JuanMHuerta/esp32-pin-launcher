// SPDX-License-Identifier: GPL-3.0-only
#ifndef TOUCH_MAP_H
#define TOUCH_MAP_H

#include <stdint.h>

typedef struct {
    uint16_t x;
    uint16_t y;
} touch_cell_t;

// The FT3168's raw X follows the long display axis. Its raw Y is inverted
// relative to the landscape display initialized with MADCTL 0xF0.
touch_cell_t touch_cell_from_raw(uint16_t raw_x, uint16_t raw_y);

#endif
