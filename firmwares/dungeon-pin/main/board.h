// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "dungeon.h"

void board_init(void);
void board_present(const uint16_t pixels[DUNGEON_W * DUNGEON_H]);
bool board_button(void);
