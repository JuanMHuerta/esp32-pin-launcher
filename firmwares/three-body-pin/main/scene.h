// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "pin_gfx.h"
enum { ORBIT_PRESETS = 8, ORBIT_TRAIL = 512 };
typedef struct {
    double x, y, vx, vy, m;
} orbit_body_t;
typedef struct {
    orbit_body_t body[3];
    float trail[ORBIT_TRAIL][3][2];
    unsigned head, count, preset, generation;
    uint32_t elapsed_ms;
    double time, energy0, softening, scale;
    uint32_t touch_started_ms;
    bool touch_down, touch_fired, panel_visible;
} orbit_t;
void orbit_init(orbit_t *s, unsigned preset);
void orbit_next(orbit_t *s);
void orbit_step(orbit_t *s, uint32_t ms);
void orbit_touch(orbit_t *s, bool down, uint32_t now_ms);
/* Explicit physical-time integration, also used by conservation tests. */
void orbit_advance(orbit_t *s, double dt);
double orbit_energy(const orbit_t *s);
const char *orbit_name(unsigned preset);
void orbit_paint(const orbit_t *s, uint16_t *pixels);
