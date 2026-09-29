#pragma once
#include "pet.h"
#include <stddef.h>

/* Native RGB565 logical frame. No allocation; reentrant; every pixel written. */
void pet_paint(const pet_t *pet, uint16_t pixels[PET_W * PET_H]);
/* Expand complete logical rows to the panel's big endian RGB565 wire format. */
bool pet_expand_strip(const uint16_t *pixels, int y, int rows, uint16_t *out);
uint16_t pet_rgb(unsigned rgb);
