#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "mech.h"

void board_init(void);
void board_present(const uint16_t pixels[MECH_W * MECH_H]);
bool board_button(void);
