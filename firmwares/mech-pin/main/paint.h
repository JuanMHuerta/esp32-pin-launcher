#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "mech.h"

void mech_paint(const mech_t *mech, uint16_t pixels[MECH_W * MECH_H]);
bool mech_expand_strip(const uint16_t *pixels, int y, int rows, uint16_t *out);
