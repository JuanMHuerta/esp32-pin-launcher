// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "maze.h"

void board_init(void);
void board_present(const uint16_t pixels[MAZE_W * MAZE_H]);
bool board_button(void);
