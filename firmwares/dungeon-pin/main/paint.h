#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "dungeon.h"

void dungeon_paint(const dungeon_t *dungeon, uint16_t pixels[DUNGEON_W * DUNGEON_H]);
bool dungeon_expand_strip(const uint16_t *pixels, int y, int rows, uint16_t *out);
