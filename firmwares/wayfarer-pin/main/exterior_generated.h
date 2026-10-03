/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
typedef struct { int width, height; const uint8_t *pixels; const uint16_t *palette; } wayfarer_indexed_t;
extern const wayfarer_indexed_t wayfarer_worlds[6];
extern const wayfarer_indexed_t wayfarer_shuttle, wayfarer_tug, wayfarer_station;
extern const wayfarer_indexed_t wayfarer_ship_freighter, wayfarer_ship_courier, wayfarer_ship_tanker;
