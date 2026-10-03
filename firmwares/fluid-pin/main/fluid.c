// SPDX-License-Identifier: GPL-3.0-only
#ifndef ESP_PLATFORM
#define _POSIX_C_SOURCE 200809L
#endif

#include "fluid.h"
#include "hot.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_timer.h"
#else
#include <time.h>
#endif

/* u[x, y] is the vertical face at (x, y + .5), while v[x, y] is the
 * horizontal face at (x + .5, y).  They use the same compact 38 x 18 backing
 * storage because particle centres never need the unused far ghost faces. */
struct fluid {
    Particle particles[PARTICLE_COUNT];
    float u[GRID_CELLS];
    float v[GRID_CELLS];
    float previous_u[GRID_CELLS];
    float previous_v[GRID_CELLS];
    float u_weight[GRID_CELLS];
    float v_weight[GRID_CELLS];
    float particle_density[GRID_CELLS];
    float density_drift[GRID_CELLS];
    float pressure[GRID_CELLS];
    uint8_t cell_type[GRID_CELLS];
    uint8_t solid_mask[GRID_CELLS];
    uint8_t fluid_neighbour_mask[GRID_CELLS];
    uint16_t active_cells[GRID_CELLS];

    /* Flat counting-sort spatial hash.  No lists or allocations are made in
     * the separation passes. */
    uint16_t bucket_count[HASH_BUCKETS];
    uint16_t bucket_offset[HASH_BUCKETS + 1];
    uint16_t particle_indices[PARTICLE_COUNT];

    float rest_density;
    float acceleration_x;
    float acceleration_y;
    fluid_metrics metrics;
};

typedef struct {
    int index[4];
    int x[4];
    int y[4];
    float weight[4];
} stencil;

static inline int grid_index(int x, int y)
{
    return y * GRID_X + x;
}
static inline float maximum(float a, float b)
{
    return a > b ? a : b;
}
static inline float minimum(float a, float b)
{
    return a < b ? a : b;
}
static inline float clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}
static inline int clampi(int x, int lo, int hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

/* This is the only fast-math path in the solver.  Separation dominated the
 * measured ESP32-S3 step time; two Newton refinements give an inverse square
 * root accurate enough that it is indistinguishable from sqrtf here, without
 * changing the pair-correction equation or either required pass. */
static inline float inverse_sqrt(float x)
{
    union {
        float f;
        uint32_t u;
    } value = {.f = x};
    value.u = 0x5f375a86u - (value.u >> 1);
    float result = value.f;
    float half_x = 0.5f * x;
    result *= 1.5f - half_x * result * result;
    result *= 1.5f - half_x * result * result;
    return result;
}

static uint32_t now_us(void)
{
#ifdef ESP_PLATFORM
    return (uint32_t)esp_timer_get_time();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u);
#endif
}

static void *allocate(size_t bytes)
{
#ifdef ESP_PLATFORM
    return heap_caps_calloc(1, bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
    return calloc(1, bytes);
#endif
}

static uint32_t random_next(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static float random_jitter(uint32_t *state)
{
    return ((float)(random_next(state) >> 8) * (1.0f / 16777215.0f) - 0.5f) * 0.030f;
}

static float random_unit(uint32_t *state)
{
    return (float)(random_next(state) >> 8) * (1.0f / 16777215.0f);
}

static stencil make_stencil(float x, float y)
{
    /* The adjusted coordinates need x + 1 and y + 1 to remain in the compact
     * array.  Bounds are normally guaranteed by particle collision handling;
     * clamping also makes the invalid-state recovery path safe. */
    x = clampf(x, 0.0f, (float)GRID_X - 1.0001f);
    y = clampf(y, 0.0f, (float)GRID_Y - 1.0001f);
    /* x and y are non-negative after the preceding clamp, so truncation is
       floor.  Avoiding the libm calls matters in the four hot transfer paths. */
    int x0 = clampi((int)x, 0, GRID_X - 2);
    int y0 = clampi((int)y, 0, GRID_Y - 2);
    float fx = clampf(x - x0, 0.0f, 1.0f);
    float fy = clampf(y - y0, 0.0f, 1.0f);
    stencil s = {
        .x = {x0, x0 + 1, x0, x0 + 1},
        .y = {y0, y0, y0 + 1, y0 + 1},
        .weight = {(1.0f - fx) * (1.0f - fy), fx * (1.0f - fy), (1.0f - fx) * fy, fx * fy},
    };
    for (int i = 0; i < 4; ++i) {
        s.index[i] = grid_index(s.x[i], s.y[i]);
    }
    return s;
}

static bool cell_supports_velocity(const fluid_t *f, int x, int y)
{
    /* Exterior of the compact grid is a solid container boundary. */
    return x < 0 || y < 0 || x >= GRID_X || y >= GRID_Y ||
           f->cell_type[grid_index(x, y)] != CELL_AIR;
}

static void resolve_container(fluid_t *f)
{
    /* Separation can push a centre outside even while its velocity points
     * into the tank.  Correct the position but discard only velocity into the wall. */
    for (int i = 0; i < PARTICLE_COUNT; ++i) {
        Particle *p = &f->particles[i];
        if (p->x < MIN_X) {
            p->x = MIN_X;
            p->vx = maximum(p->vx, 0.0f);
        } else if (p->x > MAX_X) {
            p->x = MAX_X;
            p->vx = minimum(p->vx, 0.0f);
        }
        if (p->y < MIN_Y) {
            p->y = MIN_Y;
            p->vy = maximum(p->vy, 0.0f);
        } else if (p->y > MAX_Y) {
            p->y = MAX_Y;
            p->vy = minimum(p->vy, 0.0f);
        }
    }
}

static int hash_index(float x, float y)
{
    /* ESP32-S3 emits a software divide for x / HASH_CELL_SIZE.  This hot
     * path runs six times per particle across the two separation passes. */
    const float inverse_cell_size = 1.0f / HASH_CELL_SIZE;
    int hx = clampi((int)(x * inverse_cell_size), 0, HASH_X - 1);
    int hy = clampi((int)(y * inverse_cell_size), 0, HASH_Y - 1);
    return hy * HASH_X + hx;
}

static void build_hash(fluid_t *f)
{
    memset(f->bucket_count, 0, sizeof(f->bucket_count));
    for (int i = 0; i < PARTICLE_COUNT; ++i) {
        ++f->bucket_count[hash_index(f->particles[i].x, f->particles[i].y)];
    }

    f->bucket_offset[0] = 0;
    for (int i = 0; i < HASH_BUCKETS; ++i) {
        f->bucket_offset[i + 1] = f->bucket_offset[i] + f->bucket_count[i];
        f->bucket_count[i] = f->bucket_offset[i];
    }
    for (int i = 0; i < PARTICLE_COUNT; ++i) {
        int bucket = hash_index(f->particles[i].x, f->particles[i].y);
        f->particle_indices[f->bucket_count[bucket]++] = (uint16_t)i;
    }
}

static void FLOW_HOT separate_particles(fluid_t *f)
{
    const float distance = 2.0f * PARTICLE_RADIUS;
    const float distance2_limit = distance * distance;
    for (int pass = 0; pass < PARTICLE_SEPARATION_ITERS; ++pass) {
        build_hash(f);
        for (int i = 0; i < PARTICLE_COUNT; ++i) {
            Particle *a = &f->particles[i];
            int bucket = hash_index(a->x, a->y);
            int hx = bucket % HASH_X;
            int hy = bucket / HASH_X;
            int min_y = hy > 0 ? hy - 1 : 0;
            int max_y = hy + 1 < HASH_Y ? hy + 1 : HASH_Y - 1;
            int min_x = hx > 0 ? hx - 1 : 0;
            int max_x = hx + 1 < HASH_X ? hx + 1 : HASH_X - 1;
            for (int y = min_y; y <= max_y; ++y) {
                for (int x = min_x; x <= max_x; ++x) {
                    int b = y * HASH_X + x;
                    for (uint16_t at = f->bucket_offset[b]; at < f->bucket_offset[b + 1]; ++at) {
                        int j = f->particle_indices[at];
                        if (j <= i) {
                            continue;
                        }
                        Particle *other = &f->particles[j];
                        float dx = other->x - a->x;
                        float dy = other->y - a->y;
                        float d2 = dx * dx + dy * dy;
                        if (d2 >= distance2_limit) {
                            continue;
                        }
                        if (d2 < 1e-12f) {
                            /* A deterministic direction handles the only
                             * degenerate case without a random hot-path cost. */
                            dx = 0.0001f;
                            dy = 0.0f;
                            d2 = dx * dx;
                        }
                        float correction = 0.5f * (distance * inverse_sqrt(d2) - 1.0f);
                        dx *= correction;
                        dy *= correction;
                        a->x -= dx;
                        a->y -= dy;
                        other->x += dx;
                        other->y += dy;
                    }
                }
            }
        }
    }
}

static void classify_cells(fluid_t *f)
{
    int active = 0;
    for (int y = 0; y < GRID_Y; ++y) {
        for (int x = 0; x < GRID_X; ++x) {
            int c = grid_index(x, y);
            f->solid_mask[c] = x == 0 || y == 0 || x == GRID_X - 1 || y == GRID_Y - 1;
            f->cell_type[c] = f->solid_mask[c] ? CELL_SOLID : CELL_AIR;
        }
    }
    for (int i = 0; i < PARTICLE_COUNT; ++i) {
        const Particle *p = &f->particles[i];
        int x = clampi((int)floorf(p->x), 1, GRID_X - 2);
        int y = clampi((int)floorf(p->y), 1, GRID_Y - 2);
        f->cell_type[grid_index(x, y)] = CELL_FLUID;
    }
    for (int c = 0; c < GRID_CELLS; ++c) {
        if (f->cell_type[c] != CELL_FLUID) {
            continue;
        }
        /* A fluid cell is always interior, so these four compact neighbours
         * are in bounds.  Cache the mask for all 22 projection iterations. */
        uint8_t mask = (!f->solid_mask[c - 1]) | (!f->solid_mask[c + 1] << 1) |
                       (!f->solid_mask[c - GRID_X] << 2) | (!f->solid_mask[c + GRID_X] << 3);
        f->fluid_neighbour_mask[c] = mask;
        f->active_cells[active++] = (uint16_t)c;
    }
    f->metrics.fluid_cells = active;
}

static void extrapolate_tangential_faces(float *u, float *v)
{
    for (int x = 0; x < GRID_X; ++x) {
        u[grid_index(x, 0)] = u[grid_index(x, 1)];
        u[grid_index(x, GRID_Y - 1)] = u[grid_index(x, GRID_Y - 2)];
    }
    for (int y = 0; y < GRID_Y; ++y) {
        v[grid_index(0, y)] = v[grid_index(1, y)];
        v[grid_index(GRID_X - 1, y)] = v[grid_index(GRID_X - 2, y)];
    }
}

static void enforce_grid_boundaries(fluid_t *f)
{
    /* The container is a rectangle.  Touch just its boundary faces instead
     * of scanning every grid cell three times per simulation step. */
    for (int y = 0; y < GRID_Y; ++y) {
        f->u[grid_index(0, y)] = f->u[grid_index(1, y)] = 0.0f;
        f->u[grid_index(GRID_X - 1, y)] = 0.0f;
    }
    for (int x = 0; x < GRID_X; ++x) {
        f->v[grid_index(x, 0)] = f->v[grid_index(x, 1)] = 0.0f;
        f->v[grid_index(x, GRID_Y - 1)] = 0.0f;
    }
    /* Tangential ghost samples carry the adjacent fluid velocity.  Zeroing
     * them would add numerical friction to the PIC gather at a side wall. */
    extrapolate_tangential_faces(f->u, f->v);
}

static void extrapolate_separating_faces(fluid_t *f)
{
    /* The pressure solve uses impermeable wall faces.  Once a wall cell is
     * separating at zero pressure, those zero samples describe the solid,
     * not the departing fluid.  Extend the interior velocity for the particle
     * gather so a thin row is not slowed by interpolation against the wall.
     * Extend the old grid by the same stencil to preserve the FLIP delta. */
    for (int y = 1; y < GRID_Y - 1; ++y) {
        int left = grid_index(1, y), right = grid_index(GRID_X - 2, y);
        if (f->cell_type[left] == CELL_FLUID && f->pressure[left] == 0.0f &&
            f->u[left + 1] > 0.0f) {
            f->u[left] = f->u[left + 1];
            f->previous_u[left] = f->previous_u[left + 1];
        }
        if (f->cell_type[right] == CELL_FLUID && f->pressure[right] == 0.0f && f->u[right] < 0.0f) {
            f->u[right + 1] = f->u[right];
            f->previous_u[right + 1] = f->previous_u[right];
        }
    }
    for (int x = 1; x < GRID_X - 1; ++x) {
        int top = grid_index(x, 1), bottom = grid_index(x, GRID_Y - 2);
        if (f->cell_type[top] == CELL_FLUID && f->pressure[top] == 0.0f &&
            f->v[top + GRID_X] > 0.0f) {
            f->v[top] = f->v[top + GRID_X];
            f->previous_v[top] = f->previous_v[top + GRID_X];
        }
        if (f->cell_type[bottom] == CELL_FLUID && f->pressure[bottom] == 0.0f &&
            f->v[bottom] < 0.0f) {
            f->v[bottom + GRID_X] = f->v[bottom];
            f->previous_v[bottom + GRID_X] = f->previous_v[bottom];
        }
    }
    extrapolate_tangential_faces(f->u, f->v);
    extrapolate_tangential_faces(f->previous_u, f->previous_v);
}

static void scatter_velocity(fluid_t *f, bool vertical)
{
    float *velocity = vertical ? f->v : f->u;
    float *weight = vertical ? f->v_weight : f->u_weight;
    memset(velocity, 0, GRID_CELLS * sizeof(*velocity));
    memset(weight, 0, GRID_CELLS * sizeof(*weight));
    for (int i = 0; i < PARTICLE_COUNT; ++i) {
        const Particle *p = &f->particles[i];
        stencil s = make_stencil(p->x - (vertical ? 0.5f : 0.0f), p->y - (vertical ? 0.0f : 0.5f));
        float value = vertical ? p->vy : p->vx;
        for (int k = 0; k < 4; ++k) {
            velocity[s.index[k]] += value * s.weight[k];
            weight[s.index[k]] += s.weight[k];
        }
    }
    for (int c = 0; c < GRID_CELLS; ++c) {
        if (weight[c] > 0.0f) {
            velocity[c] /= weight[c];
        }
    }
}

static void particle_to_grid(fluid_t *f)
{
    memset(f->density_drift, 0, sizeof(f->density_drift));
    classify_cells(f);
    scatter_velocity(f, false);
    scatter_velocity(f, true);
    enforce_grid_boundaries(f);
}

static void calculate_density(fluid_t *f)
{
    memset(f->particle_density, 0, sizeof(f->particle_density));
    for (int i = 0; i < PARTICLE_COUNT; ++i) {
        const Particle *p = &f->particles[i];
        stencil s = make_stencil(p->x - 0.5f, p->y - 0.5f);
        for (int k = 0; k < 4; ++k) {
            f->particle_density[s.index[k]] += s.weight[k];
        }
    }
    if (f->rest_density == 0.0f) {
        float total = 0.0f;
        for (int i = 0; i < f->metrics.fluid_cells; ++i) {
            total += f->particle_density[f->active_cells[i]];
        }
        f->rest_density = total / maximum(1, f->metrics.fluid_cells);
    }
}

static inline float divergence(const fluid_t *f, int c)
{
    /* Only interior active cells reach projection. */
    return f->u[c + 1] - f->u[c] + f->v[c + GRID_X] - f->v[c];
}

static float pressure_residual(const fluid_t *f, int c)
{
    float residual = divergence(f, c) - f->density_drift[c];
    /* A wall cell at zero pressure may lose liquid as its surface detaches.
     * Positive divergence there is allowed by the inequality constraint. */
    if (f->fluid_neighbour_mask[c] != 15 && f->pressure[c] == 0.0f) {
        return minimum(residual, 0.0f);
    }
    return residual;
}

static void FLOW_HOT project_pressure(fluid_t *f)
{
    memset(f->pressure, 0, sizeof(f->pressure));
    float before = 0.0f;
    float after = 0.0f;
    float density_max = 0.0f;
    for (int i = 0; i < f->metrics.fluid_cells; ++i) {
        int c = f->active_cells[i];
        float compression = f->particle_density[c] - f->rest_density;
        f->density_drift[c] = compression > 0.0f ? DENSITY_DRIFT_K * compression : 0.0f;
        float d = pressure_residual(f, c);
        before += d * d;
        density_max = maximum(density_max, f->particle_density[c]);
    }

    for (int iteration = 0; iteration < PRESSURE_ITERS; ++iteration) {
        for (int i = 0; i < f->metrics.fluid_cells; ++i) {
            int c = f->active_cells[i];
            uint8_t mask = f->fluid_neighbour_mask[c];
            static const float inverse_neighbours[16] = {
                0.0f, 1.0f, 1.0f, 0.5f,        1.0f, 0.5f,        0.5f,        1.0f / 3.0f,
                1.0f, 0.5f, 0.5f, 1.0f / 3.0f, 0.5f, 1.0f / 3.0f, 1.0f / 3.0f, 0.25f,
            };
            float correction = -PRESSURE_OVER_RELAXATION *
                               (divergence(f, c) - f->density_drift[c]) * inverse_neighbours[mask];
            /* Projected Gauss-Seidel at solid-adjacent cells: the wall may
             * support a pool with positive pressure, but cannot pull it back
             * with negative pressure.  Interior fluid remains incompressible.
             * Clamp accumulated pressure, not each signed iteration update. */
            if (mask != 15) {
                float pressure = maximum(f->pressure[c] + correction, 0.0f);
                correction = pressure - f->pressure[c];
                f->pressure[c] = pressure;
            }
            if (mask & 1) {
                f->u[c] -= correction;
            }
            if (mask & 2) {
                f->u[c + 1] += correction;
            }
            if (mask & 4) {
                f->v[c] -= correction;
            }
            if (mask & 8) {
                f->v[c + GRID_X] += correction;
            }
        }
    }
    enforce_grid_boundaries(f);
    for (int i = 0; i < f->metrics.fluid_cells; ++i) {
        int c = f->active_cells[i];
        float d = pressure_residual(f, c);
        after += d * d;
    }
    int count = maximum(1, f->metrics.fluid_cells);
    f->metrics.divergence_before = sqrtf(before / count);
    f->metrics.divergence_after = sqrtf(after / count);
    f->metrics.density_max_ratio = f->rest_density > 0.0f ? density_max / f->rest_density : 0.0f;
}

static bool valid_u_face(const fluid_t *f, int x, int y)
{
    return cell_supports_velocity(f, x - 1, y) || cell_supports_velocity(f, x, y);
}

static bool valid_v_face(const fluid_t *f, int x, int y)
{
    return cell_supports_velocity(f, x, y - 1) || cell_supports_velocity(f, x, y);
}

static void FLOW_HOT grid_to_particles(fluid_t *f, float target_vx, float target_vy)
{
    extrapolate_separating_faces(f);
    for (int component = 0; component < 2; ++component) {
        const bool vertical = component != 0;
        const float *grid = vertical ? f->v : f->u;
        const float *previous = vertical ? f->previous_v : f->previous_u;
        for (int i = 0; i < PARTICLE_COUNT; ++i) {
            Particle *p = &f->particles[i];
            stencil s =
                make_stencil(p->x - (vertical ? 0.5f : 0.0f), p->y - (vertical ? 0.0f : 0.5f));
            float pic = 0.0f;
            float flip_delta = 0.0f;
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                bool valid =
                    vertical ? valid_v_face(f, s.x[k], s.y[k]) : valid_u_face(f, s.x[k], s.y[k]);
                if (!valid) {
                    continue;
                }
                pic += grid[s.index[k]] * s.weight[k];
                flip_delta += (grid[s.index[k]] - previous[s.index[k]]) * s.weight[k];
                sum += s.weight[k];
            }
            if (sum == 0.0f) {
                continue;
            }
            float inverse_weight = 1.0f / sum;
            pic *= inverse_weight;
            flip_delta *= inverse_weight;
            float old_particle_velocity = vertical ? p->vy : p->vx;
            float flip = old_particle_velocity + flip_delta;
            float updated = FLIP_RATIO * flip + (1.0f - FLIP_RATIO) * pic;
            if (vertical) {
                p->vy = updated;
            } else {
                p->vx = updated;
            }
        }
    }
    float mean_vx = 0.0f, mean_vy = 0.0f;
    for (int i = 0; i < PARTICLE_COUNT; ++i) {
        mean_vx += f->particles[i].vx;
        mean_vy += f->particles[i].vy;
    }
    mean_vx /= PARTICLE_COUNT;
    mean_vy /= PARTICLE_COUNT;
    float middle_x = 0.5f * (MIN_X + MAX_X);
    float blend_x =
        clampf((fabsf(target_vx) - TRANSLATION_TARGET_DEADZONE) / TRANSLATION_TARGET_BLEND_RANGE,
               0.0f, 1.0f);
    float blend_y =
        clampf((fabsf(target_vy) - TRANSLATION_TARGET_DEADZONE) / TRANSLATION_TARGET_BLEND_RANGE,
               0.0f, 1.0f);
    /* A brief strong shift must pull fluid away from a wall in the next few
     * steps.  Preserve the gentler correction for ordinary hand slides. */
    float strong_x = clampf((fabsf(target_vx) - STRONG_TRANSLATION_TARGET_START) /
                                (STRONG_TRANSLATION_TARGET_FULL - STRONG_TRANSLATION_TARGET_START),
                            0.0f, 1.0f);
    float limit_x =
        TRANSLATION_CORRECTION_LIMIT +
        (STRONG_TRANSLATION_CORRECTION_LIMIT - TRANSLATION_CORRECTION_LIMIT) * strong_x +
        (VERY_STRONG_TRANSLATION_CORRECTION_LIMIT - STRONG_TRANSLATION_CORRECTION_LIMIT) *
            clampf((fabsf(target_vx) - STRONG_TRANSLATION_TARGET_FULL) /
                       (VERY_STRONG_TRANSLATION_TARGET_FULL - STRONG_TRANSLATION_TARGET_FULL),
                   0.0f, 1.0f);
    float correction_x = clampf((target_vx - mean_vx) * blend_x, -limit_x, limit_x);
    float correction_y = clampf((target_vy - mean_vy) * blend_y, -TRANSLATION_CORRECTION_LIMIT,
                                TRANSLATION_CORRECTION_LIMIT);
    float upright = clampf((f->acceleration_y / GRAVITY - 0.65f) / 0.25f, 0.0f, 1.0f);
    float surface_top = MAX_Y - (MAX_Y - MIN_Y) * FILL_RATIO;
    float inverse_half_width = 2.0f / (MAX_X - MIN_X);
    for (int i = 0; i < PARTICLE_COUNT; ++i) {
        Particle *p = &f->particles[i];
        float surface = clampf(
            (surface_top + VERTICAL_SURFACE_DEPTH - p->y) / VERTICAL_SURFACE_DEPTH, 0.0f, 1.0f);
        float spread = clampf((p->x - middle_x) * inverse_half_width, -1.0f, 1.0f);
        float surface_push =
            -target_vy * VERTICAL_SURFACE_RESPONSE * upright * surface * spread * blend_y;
        p->vx = (p->vx + correction_x + surface_push) * VELOCITY_DAMPING;
        p->vy = (p->vy + correction_y) * VELOCITY_DAMPING;
    }
}

static bool state_is_valid(const fluid_t *f)
{
    if (!isfinite(f->rest_density) || f->rest_density < 0.0f) {
        return false;
    }
    for (int i = 0; i < PARTICLE_COUNT; ++i) {
        const Particle *p = &f->particles[i];
        if (!isfinite(p->x) || !isfinite(p->y) || !isfinite(p->vx) || !isfinite(p->vy)) {
            return false;
        }
    }
    for (int i = 0; i < GRID_CELLS; ++i) {
        if (!isfinite(f->u[i]) || !isfinite(f->v[i])) {
            return false;
        }
    }
    return true;
}

static bool recover(fluid_t *f)
{
    ++f->metrics.resets;
    fluid_reset(f);
    return false;
}

fluid_t *fluid_create(void)
{
    fluid_t *f = allocate(sizeof(*f));
    if (!f) {
        return NULL;
    }
    fluid_reset(f);
    return f;
}

void fluid_destroy(fluid_t *f)
{
    free(f);
}

void fluid_reset(fluid_t *f)
{
    uint32_t resets = f->metrics.resets;
    memset(f->particles, 0, sizeof(f->particles));
    memset(f->u, 0, sizeof(f->u));
    memset(f->v, 0, sizeof(f->v));
    memset(f->previous_u, 0, sizeof(f->previous_u));
    memset(f->previous_v, 0, sizeof(f->previous_v));
    memset(f->u_weight, 0, sizeof(f->u_weight));
    memset(f->v_weight, 0, sizeof(f->v_weight));
    memset(f->particle_density, 0, sizeof(f->particle_density));
    memset(f->density_drift, 0, sizeof(f->density_drift));
    memset(f->cell_type, CELL_AIR, sizeof(f->cell_type));
    memset(f->solid_mask, 0, sizeof(f->solid_mask));
    memset(f->fluid_neighbour_mask, 0, sizeof(f->fluid_neighbour_mask));
    f->rest_density = 0.0f;
    f->acceleration_x = f->acceleration_y = 0.0f;
    f->metrics = (fluid_metrics){.particle_count = PARTICLE_COUNT, .resets = resets};

    const float spacing = 1.80f * PARTICLE_RADIUS;
    const float row_spacing = spacing * 0.866025403784f;
    const int columns = (int)floorf((MAX_X - MIN_X) / spacing) + 1;
    const float fluid_top = MAX_Y - (MAX_Y - MIN_Y) * FILL_RATIO;
    uint32_t random = 0x8c9d5a31u;
    const int rows_needed = (PARTICLE_COUNT + columns - 1) / columns;
    const int omitted_positions = rows_needed * columns - PARTICLE_COUNT;
    int seeded = 0;
    for (int row = 0; row < rows_needed; ++row) {
        float y = MAX_Y - row * row_spacing;
        if (y < fluid_top) {
            break;
        }
        float offset = (row & 1) ? spacing * 0.5f : 0.0f;
        /* Spread the seven unavoidable holes over different rows rather than
         * making the last seeded row visibly shorter. */
        int omitted_column = row < omitted_positions ? (row * 17) % columns : -1;
        for (int column = 0; column < columns && seeded < PARTICLE_COUNT; ++column) {
            if (column == omitted_column) {
                continue;
            }
            float x = MIN_X + column * spacing + offset;
            if (x > MAX_X) {
                continue;
            }
            Particle *p = &f->particles[seeded++];
            p->x = clampf(x + random_jitter(&random), MIN_X, MAX_X);
            p->y = clampf(y + random_jitter(&random), MIN_Y, MAX_Y);
        }
    }
    /* Geometric packing is expected to provide all 573 particles.  The
     * fallback retains the requested lower-volume invariant if it ever does
     * not, for example after a future radius experiment. */
    while (seeded < PARTICLE_COUNT) {
        Particle *p = &f->particles[seeded++];
        p->x = MIN_X + random_unit(&random) * (MAX_X - MIN_X);
        p->y = fluid_top + random_unit(&random) * (MAX_Y - fluid_top);
    }
    resolve_container(f);
}

bool FLOW_HOT fluid_step_with_translation(fluid_t *f, float acceleration_x, float acceleration_y,
                                          float target_vx, float target_vy)
{
    uint32_t step_start = now_us();
    if (!isfinite(acceleration_x) || !isfinite(acceleration_y) || !isfinite(target_vx) ||
        !isfinite(target_vy) || !state_is_valid(f)) {
        return recover(f);
    }
    f->acceleration_x = acceleration_x;
    f->acceleration_y = acceleration_y;

    uint32_t started = now_us();
    for (int i = 0; i < PARTICLE_COUNT; ++i) {
        Particle *p = &f->particles[i];
        p->x += p->vx * SIM_DT;
        p->y += p->vy * SIM_DT;
    }
    f->metrics.integration_us = now_us() - started;

    started = now_us();
    separate_particles(f);
    resolve_container(f);
    f->metrics.separation_us = now_us() - started;

    started = now_us();
    particle_to_grid(f);
    f->metrics.p2g_us = now_us() - started;

    started = now_us();
    calculate_density(f);
    memcpy(f->previous_u, f->u, sizeof(f->u));
    memcpy(f->previous_v, f->v, sizeof(f->v));
    /* Save the unforced grid for FLIP, then add forces once on the grid.
     * Pressure supports resting water before the next particle advection. */
    for (int c = 0; c < GRID_CELLS; ++c) {
        f->u[c] += acceleration_x * SIM_DT;
        f->v[c] += acceleration_y * SIM_DT;
    }
    enforce_grid_boundaries(f);
    f->metrics.density_us = now_us() - started;

    started = now_us();
    project_pressure(f);
    f->metrics.pressure_us = now_us() - started;

    started = now_us();
    grid_to_particles(f, target_vx, target_vy);
    f->metrics.g2p_us = now_us() - started;

    if (!state_is_valid(f)) {
        return recover(f);
    }
    float speed_total = 0.0f;
    float speed_max = 0.0f;
    float mean_x = 0.0f, mean_y = 0.0f, mean_vx = 0.0f, mean_vy = 0.0f;
    int ceiling = 0;
    for (int i = 0; i < PARTICLE_COUNT; ++i) {
        const Particle *p = &f->particles[i];
        float speed = sqrtf(p->vx * p->vx + p->vy * p->vy);
        speed_total += speed;
        speed_max = maximum(speed_max, speed);
        mean_x += p->x;
        mean_y += p->y;
        mean_vx += p->vx;
        mean_vy += p->vy;
        if (p->y < MIN_Y + 1.0f) {
            ++ceiling;
        }
    }
    f->metrics.average_speed = speed_total / PARTICLE_COUNT;
    f->metrics.maximum_speed = speed_max;
    f->metrics.centroid_x = mean_x / PARTICLE_COUNT;
    f->metrics.centroid_y = mean_y / PARTICLE_COUNT;
    f->metrics.mean_vx = mean_vx / PARTICLE_COUNT;
    f->metrics.mean_vy = mean_vy / PARTICLE_COUNT;
    f->metrics.ceiling_particles = ceiling;
    f->metrics.particle_count = PARTICLE_COUNT;
    ++f->metrics.steps;
    f->metrics.step_us = now_us() - step_start;
    return true;
}

bool fluid_step(fluid_t *f, float acceleration_x, float acceleration_y)
{
    return fluid_step_with_translation(f, acceleration_x, acceleration_y, 0.0f, 0.0f);
}

void fluid_get_metrics(const fluid_t *f, fluid_metrics *out)
{
    *out = f->metrics;
}

void fluid_publish(const fluid_t *f, fluid_snapshot *out)
{
    uint32_t started = now_us();
    memset(out->density, 0, sizeof(out->density));
    memset(out->speed, 0, sizeof(out->speed));
    for (int i = 0; i < PARTICLE_COUNT; ++i) {
        const Particle *p = &f->particles[i];
        float sx = (p->x - MIN_X) * ((float)(PIXEL_GRID_X - 1) / (MAX_X - MIN_X));
        float sy = (p->y - MIN_Y) * ((float)(PIXEL_GRID_Y - 1) / (MAX_Y - MIN_Y));
        int x0 = clampi((int)floorf(sx), 0, PIXEL_GRID_X - 2);
        int y0 = clampi((int)floorf(sy), 0, PIXEL_GRID_Y - 2);
        float fx = clampf(sx - x0, 0.0f, 1.0f);
        float fy = clampf(sy - y0, 0.0f, 1.0f);
        int cells[4] = {y0 * PIXEL_GRID_X + x0, y0 * PIXEL_GRID_X + x0 + 1,
                        (y0 + 1) * PIXEL_GRID_X + x0, (y0 + 1) * PIXEL_GRID_X + x0 + 1};
        float weights[4] = {(1.0f - fx) * (1.0f - fy), fx * (1.0f - fy), (1.0f - fx) * fy, fx * fy};
        uint8_t speed = (uint8_t)lroundf(
            clampf(sqrtf(p->vx * p->vx + p->vy * p->vy) / 8.0f, 0.0f, 1.0f) * 255.0f);
        for (int k = 0; k < 4; ++k) {
            out->density[cells[k]] += weights[k];
            if (speed > out->speed[cells[k]]) {
                out->speed[cells[k]] = speed;
            }
        }
    }
    float mass = 0.0f;
    for (int i = 0; i < PIXEL_COUNT; ++i) {
        mass += out->density[i];
    }
    out->sequence = f->metrics.steps;
    out->acceleration_x = f->acceleration_x;
    out->acceleration_y = f->acceleration_y;
    out->metrics = f->metrics;
    out->metrics.render_mass = mass;
    out->metrics.render_mass_ratio = mass / PARTICLE_COUNT;
    out->metrics.render_splat_us = now_us() - started;
}

size_t fluid_memory_bytes(void)
{
    return sizeof(fluid_t);
}

#ifndef ESP_PLATFORM
void fluid_inject_invalid(fluid_t *f)
{
    f->particles[0].vx = NAN;
}
#endif
