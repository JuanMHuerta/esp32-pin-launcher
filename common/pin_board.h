// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "pin_gfx.h"
void pin_board_init(void);
void pin_board_present(const uint16_t pixels[PIN_W * PIN_H]);
bool pin_board_button(void);
