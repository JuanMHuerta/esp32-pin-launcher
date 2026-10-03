// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "fluid_config.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { CELL_AIR, CELL_FLUID, CELL_SOLID } fluid_cell;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
} Particle;

typedef struct {
    uint32_t step_us;
    uint32_t integration_us;
    uint32_t separation_us;
    uint32_t p2g_us;
    uint32_t density_us;
    uint32_t pressure_us;
    uint32_t g2p_us;
    uint32_t render_splat_us;
    uint32_t steps;
    uint32_t resets;
    int particle_count;
    int fluid_cells;
    int ceiling_particles;
    float average_speed;
    float maximum_speed;
    float centroid_x;
    float centroid_y;
    float mean_vx;
    float mean_vy;
    float render_mass;
    float render_mass_ratio;
    float divergence_before;
    float divergence_after;
    float density_max_ratio;
} fluid_metrics;

/* A complete renderer-owned logical image.  Three of these are static task
 * snapshots; there is no particle access from the render task. */
typedef struct {
    float density[PIXEL_COUNT];
    /* Maximum normalised particle speed for each logical pixel.  Density
     * remains float for the exact visual-mass invariant; speed does not need
     * that precision and this keeps all three snapshots in internal SRAM. */
    uint8_t speed[PIXEL_COUNT];
    uint32_t sequence;
    int64_t sensor_us;
    float acceleration_x;
    float acceleration_y;
    fluid_metrics metrics;
} fluid_snapshot;

typedef struct fluid fluid_t;

fluid_t *fluid_create(void);
void fluid_destroy(fluid_t *f);
void fluid_reset(fluid_t *f);

/* acceleration is already in simulation-cell units per second squared. */
bool fluid_step(fluid_t *f, float acceleration_x, float acceleration_y);
/* Translation is a bounded motion assist for bulk particle velocity in
 * cells/s.  It keeps small enclosure slides visible through their coast
 * interval; gravity and wall separation also work with both targets zero. */
bool fluid_step_with_translation(fluid_t *f, float acceleration_x, float acceleration_y,
                                 float target_vx, float target_vy);
void fluid_get_metrics(const fluid_t *f, fluid_metrics *out);
void fluid_publish(const fluid_t *f, fluid_snapshot *out);
size_t fluid_memory_bytes(void);

#ifndef ESP_PLATFORM
void fluid_inject_invalid(fluid_t *f);
#endif
