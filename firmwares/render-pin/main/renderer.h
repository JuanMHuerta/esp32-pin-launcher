// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdint.h>

#define PIN_WIDTH 536
#define PIN_HEIGHT 240
#define DUST_COUNT 640
#define CONSTELLATIONS 12
#define NODES_PER_CONSTELLATION 6
#define STAR_COUNT (DUST_COUNT + CONSTELLATIONS * NODES_PER_CONSTELLATION)
#define EDGE_COUNT (CONSTELLATIONS * 5)

typedef struct {
    int16_t x, y;
    uint8_t alpha, size, hue, frac_x, frac_y;
} projected_star_t;
typedef struct {
    uint16_t a, b;
    uint8_t alpha, phase;
} projected_edge_t;
typedef struct {
    projected_star_t stars[STAR_COUNT];
    projected_edge_t edges[EDGE_COUNT];
    uint8_t mood;
} renderer_t;

void renderer_init(void);
/* View X/Y are normalized tilt; roll uses 1/1024 turns; energy is 0..1. */
void renderer_prepare(renderer_t *state, int32_t time_ms, float view_x, float view_y, float roll,
                      float energy, uint8_t mood, int pulse_x, int pulse_y, int32_t pulse_age_ms);
/* Output is RGB565 with bytes swapped for SH8601 QSPI DMA. */
void renderer_strip(const renderer_t *state, int y0, int rows, uint16_t *pixels);
