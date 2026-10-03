// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "maze.h"
void maze_paint(const maze_t *m, uint16_t pixels[MAZE_W * MAZE_H]);
bool maze_expand_strip(const uint16_t *pixels, int y, int rows, uint16_t *out);
