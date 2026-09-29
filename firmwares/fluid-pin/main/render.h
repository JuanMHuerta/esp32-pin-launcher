#pragma once

#include "fluid.h"

typedef struct fluid_renderer fluid_renderer;

fluid_renderer *render_create(void);
void render_destroy(fluid_renderer *r);

/* Reconstructs density from an immutable snapshot, then applies persistence. */
void render_reconstruct(fluid_renderer *r, const fluid_snapshot *snapshot);

/* Fills an RGB565-on-the-wire band.  Logical pixels are never interpolated. */
void render_band(const fluid_renderer *r, int y, int rows, uint16_t *out);

float render_mass_ratio(const fluid_renderer *r);
