// SPDX-License-Identifier: GPL-3.0-only
#include "fluid.h"
#include "render.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static fluid_metrics step(fluid_t *f, float ax, float ay)
{
    assert(fluid_step(f, ax, ay));
    fluid_metrics m;
    fluid_get_metrics(f, &m);
    assert(m.resets == 0 && m.particle_count == PARTICLE_COUNT);
    return m;
}

static float edge_mass(const fluid_snapshot *image, int axis, int side)
{
    float mass = 0;
    int edge = side < 0 ? 0 : (axis ? PIXEL_GRID_Y : PIXEL_GRID_X) - 1;
    for (int i = 0; i < (axis ? PIXEL_GRID_X : PIXEL_GRID_Y); ++i) {
        mass += image->density[axis ? edge * PIXEL_GRID_X + i : i * PIXEL_GRID_X + edge];
    }
    return mass;
}

static int edge_pixels(const fluid_renderer *renderer, int axis, int side)
{
    uint16_t row[PANEL_W];
    int lit = 0;
    int edge = side < 0 ? 0 : (axis ? PIXEL_GRID_Y : PIXEL_GRID_X) - 1;
    if (axis) {
        render_band(renderer, edge * PIXEL_NATIVE_SIZE + 3, 1, row);
        for (int x = 0; x < PIXEL_GRID_X; ++x) {
            lit += row[x * PIXEL_NATIVE_SIZE + 3] != 0;
        }
    } else {
        for (int y = 0; y < PIXEL_GRID_Y; ++y) {
            render_band(renderer, y * PIXEL_NATIVE_SIZE + 3, 1, row);
            lit += row[edge * PIXEL_NATIVE_SIZE + 3] != 0;
        }
    }
    return lit;
}

static int contact_gaps(const fluid_renderer *renderer, int axis, int side)
{
    uint16_t row[PANEL_W];
    int gaps = 0;
    int edge = side < 0 ? 0 : (axis ? PIXEL_GRID_Y : PIXEL_GRID_X) - 1;
    int inboard = edge - side;
    if (axis) {
        render_band(renderer, edge * PIXEL_NATIVE_SIZE + 3, 1, row);
        for (int x = 0; x < PIXEL_GRID_X; ++x) {
            uint16_t edge_core = row[x * PIXEL_NATIVE_SIZE + 3];
            render_band(renderer, inboard * PIXEL_NATIVE_SIZE + 3, 1, row);
            gaps += row[x * PIXEL_NATIVE_SIZE + 3] != 0 && edge_core == 0;
        }
    } else {
        for (int y = 0; y < PIXEL_GRID_Y; ++y) {
            render_band(renderer, y * PIXEL_NATIVE_SIZE + 3, 1, row);
            uint16_t edge_core = row[edge * PIXEL_NATIVE_SIZE + 3];
            gaps += row[inboard * PIXEL_NATIVE_SIZE + 3] != 0 && edge_core == 0;
        }
    }
    return gaps;
}

static void departure(int axis, int side)
{
    fluid_t *f = fluid_create();
    fluid_renderer *renderer = render_create();
    assert(f && renderer);
    fluid_metrics m = {0};
    for (int i = 0; i < 10 * PHYSICS_HZ; ++i) {
        m = step(f, axis ? 0 : side * GRAVITY, axis ? side * GRAVITY : 0);
    }
    fluid_snapshot snapshot = {0};
    fluid_publish(f, &snapshot);
    render_reconstruct(renderer, &snapshot);
    assert(edge_mass(&snapshot, axis, side) > 10.0f);
    assert(edge_pixels(renderer, axis, side) > 10);
    printf("contact axis=%c wall=%d gaps=%d\n", axis ? 'y' : 'x', side,
           contact_gaps(renderer, axis, side));
    assert(contact_gaps(renderer, axis, side) == 0);
    float origin = axis ? m.centroid_y : m.centroid_x;
    float initial_v = axis ? m.mean_vy : m.mean_vx;
    float previous_speed = -side * initial_v;
    float first_impulse = 0, displacement = 0;
    /* For the first 200 ms there is room to fall without meeting another
     * wall.  Compare the first impulse against g*dt, then require continuing
     * acceleration and displacement.  No sensor or bulk target is involved. */
    for (int i = 0; i < PHYSICS_HZ / 5; ++i) {
        m = step(f, axis ? 0 : -side * GRAVITY, axis ? -side * GRAVITY : 0);
        float speed = -side * (axis ? m.mean_vy : m.mean_vx);
        if (i == 0) {
            first_impulse = (speed - previous_speed) / (GRAVITY * SIM_DT);
            assert(first_impulse > 0.65f && first_impulse < 1.15f);
        }
        assert(speed > previous_speed + 0.40f * GRAVITY * SIM_DT);
        previous_speed = speed;
        displacement = side * (origin - (axis ? m.centroid_y : m.centroid_x));
        fluid_publish(f, &snapshot);
        render_reconstruct(renderer, &snapshot);
        /* A logical pixel spans about 0.53 cells.  Free fall clears it in
         * about 120 ms; allow 167 ms for advection and the settled corner
         * particles' initial collisions.  Check the actual edge layer;
         * a moving centroid can conceal a stuck row. */
        if (i + 1 == 10) {
            float residue = edge_mass(&snapshot, axis, side);
            printf("edge axis=%c wall=%d mass_167ms=%.6f\n", axis ? 'y' : 'x', side, residue);
            assert(residue < 0.001f);
        }
    }
    float ballistic = 0.5f * GRAVITY * powf(SPEED_MULTIPLIER * 0.2f, 2.0f);
    assert(displacement > 0.65f * ballistic && displacement < 1.15f * ballistic);
    printf(
        "departure axis=%c wall=%d first_impulse/gdt=%.3f displacement_200ms=%.3f ballistic=%.3f\n",
        axis ? 'y' : 'x', side, first_impulse, displacement, ballistic);
    /* Allow visual persistence two more frames, then require a black edge. */
    assert(edge_pixels(renderer, axis, side) == 0);
    assert(fabsf(snapshot.metrics.render_mass_ratio - 1.0f) < 0.005f);
    render_destroy(renderer);
    fluid_destroy(f);
}

static void rapid_rotation(int direction)
{
    fluid_t *f = fluid_create();
    fluid_renderer *renderer = render_create();
    assert(f && renderer);
    fluid_snapshot snapshot = {0};
    for (int i = 0; i < 10 * PHYSICS_HZ; ++i) {
        step(f, 0, GRAVITY);
    }
    /* Five turns at 2.5 Hz wet the ceiling and corners, then stop upright.
     * Isolate the solver from sensor assists so gravity alone must drain it. */
    for (int i = 0; i < 2 * PHYSICS_HZ; ++i) {
        float angle = direction * i * 3.14159265f / 12.0f;
        step(f, GRAVITY * sinf(angle), GRAVITY * cosf(angle));
        fluid_publish(f, &snapshot);
        render_reconstruct(renderer, &snapshot);
    }
    assert(edge_mass(&snapshot, 1, -1) > 0.1f);
    for (int i = 0; i < PHYSICS_HZ / 2; ++i) {
        step(f, 0, GRAVITY);
        fluid_publish(f, &snapshot);
        render_reconstruct(renderer, &snapshot);
        if (i + 1 == 12) {
            printf("rotation direction=%d ceiling_mass_200ms=%.6f pixels=%d\n", direction,
                   edge_mass(&snapshot, 1, -1), edge_pixels(renderer, 1, -1));
            assert(edge_mass(&snapshot, 1, -1) < 0.001f);
        }
        if (i + 1 >= 15) {
            assert(edge_pixels(renderer, 1, -1) == 0);
        }
        assert(fabsf(snapshot.metrics.render_mass_ratio - 1.0f) < 0.005f);
    }
    render_destroy(renderer);
    fluid_destroy(f);
}

static float upper_mass(fluid_t *f)
{
    fluid_snapshot image = {0};
    fluid_publish(f, &image);
    float mass = 0;
    for (int y = 0; y < PIXEL_GRID_Y / 2; ++y) {
        for (int x = 0; x < PIXEL_GRID_X; ++x) {
            mass += image.density[y * PIXEL_GRID_X + x];
        }
    }
    return mass / PARTICLE_COUNT;
}

static void column_collapse(int side)
{
    fluid_t *f = fluid_create();
    assert(f);
    for (int i = 0; i < 10 * PHYSICS_HZ; ++i) {
        step(f, side * GRAVITY, 0);
    }
    float start = upper_mass(f);
    assert(start > 0.40f);
    fluid_metrics m = step(f, 0, GRAVITY);
    /* Returning upright must start the fall immediately, with no sideways
     * sensor kick or reversed x force to trigger an artificial release. */
    assert(m.mean_vy > 0.20f * GRAVITY * SIM_DT);
    for (int i = 1; i < PHYSICS_HZ / 2; ++i) {
        step(f, 0, GRAVITY);
    }
    float half_second = upper_mass(f);
    for (int i = PHYSICS_HZ / 2; i < PHYSICS_HZ; ++i) {
        step(f, 0, GRAVITY);
    }
    float one_second = upper_mass(f);
    printf("column side=%d upper_mass=%.3f after_500ms=%.3f after_1s=%.3f\n", side, start,
           half_second, one_second);
    assert(half_second < start * 0.60f);
    assert(one_second < 0.10f);
    for (int i = 0; i < 14 * PHYSICS_HZ; ++i) {
        m = step(f, 0, GRAVITY);
    }
    assert(m.average_speed < 0.15f && m.ceiling_particles == 0);
    fluid_destroy(f);
}

int main(void)
{
    for (int axis = 0; axis < 2; ++axis) {
        for (int side = -1; side <= 1; side += 2) {
            departure(axis, side);
        }
    }
    column_collapse(-1);
    column_collapse(1);
    rapid_rotation(-1);
    rapid_rotation(1);
    puts("PASS: wall acceleration, edge pixels, rotation residue, column drainage, settling");
}
