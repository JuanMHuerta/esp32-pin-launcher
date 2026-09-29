#include "fluid.h"
#include "render.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void occupied_pixels(const fluid_renderer *renderer, bool occupied[PIXEL_COUNT])
{
    uint16_t row[PANEL_W];
    for (int y = 0; y < PIXEL_GRID_Y; ++y) {
        render_band(renderer, y * PIXEL_NATIVE_SIZE + 3, 1, row);
        for (int x = 0; x < PIXEL_GRID_X; ++x)
            occupied[y * PIXEL_GRID_X + x] = row[x * PIXEL_NATIVE_SIZE + 3] != 0;
    }
}

static int interior_holes(const fluid_renderer *renderer)
{
    bool occupied[PIXEL_COUNT], outside[PIXEL_COUNT] = {0};
    int queue[PIXEL_COUNT], head = 0, tail = 0;
    occupied_pixels(renderer, occupied);
    /* Flood air from the container boundary.  Any unlit logical pixel left
     * inside the settled liquid is a hole.  Native pixel gutters do not count. */
    for (int y = 0; y < PIXEL_GRID_Y; ++y)
        for (int x = 0; x < PIXEL_GRID_X; ++x) {
            int c = y * PIXEL_GRID_X + x;
            if ((x == 0 || x == PIXEL_GRID_X - 1 || y == 0 || y == PIXEL_GRID_Y - 1) &&
                !occupied[c]) {
                outside[c] = true;
                queue[tail++] = c;
            }
        }
    while (head < tail) {
        int c = queue[head++], x = c % PIXEL_GRID_X, y = c / PIXEL_GRID_X;
        int neighbours[4] = {x > 0 ? c - 1 : c, x + 1 < PIXEL_GRID_X ? c + 1 : c,
                            y > 0 ? c - PIXEL_GRID_X : c,
                            y + 1 < PIXEL_GRID_Y ? c + PIXEL_GRID_X : c};
        for (int k = 0; k < 4; ++k) {
            int next = neighbours[k];
            if (!occupied[next] && !outside[next]) {
                outside[next] = true;
                queue[tail++] = next;
            }
        }
    }
    int holes = 0;
    for (int c = 0; c < PIXEL_COUNT; ++c) holes += !occupied[c] && !outside[c];
    return holes;
}

static float interior_contrast(const fluid_renderer *renderer)
{
    uint8_t green[PIXEL_COUNT];
    uint16_t row[PANEL_W];
    int brightest = 0, dimmest = 63, samples = 0;
    for (int y = 0; y < PIXEL_GRID_Y; ++y) {
        render_band(renderer, y * PIXEL_NATIVE_SIZE + 3, 1, row);
        for (int x = 0; x < PIXEL_GRID_X; ++x) {
            uint16_t rgb = __builtin_bswap16(row[x * PIXEL_NATIVE_SIZE + 3]);
            int value = (rgb >> 5) & 63;
            green[y * PIXEL_GRID_X + x] = value;
            if (value > brightest) brightest = value;
        }
    }
    for (int y = 1; y < PIXEL_GRID_Y - 1; ++y)
        for (int x = 1; x < PIXEL_GRID_X - 1; ++x) {
            int c = y * PIXEL_GRID_X + x;
            /* A lit core surrounded by water must not look like a black
             * seam.  Hole counting alone misses dim, connected V-shaped
             * channels such as the one reported on the physical display. */
            if (green[c] && green[c - 1] && green[c + 1] &&
                green[c - PIXEL_GRID_X] && green[c + PIXEL_GRID_X]) {
                if (green[c] < dimmest) dimmest = green[c];
                ++samples;
            }
        }
    assert(samples > 100 && brightest > 0);
    return (float)dimmest / brightest;
}

static void shake(fluid_t *f)
{
    for (int impulse = 0; impulse < 20; ++impulse) {
        float angle = impulse * 2.39996323f;
        for (int i = 0; i < 4; ++i)
            assert(fluid_step(f, 2 * GRAVITY * cosf(angle), GRAVITY + 2 * GRAVITY * sinf(angle)));
    }
}

static void long_idle_color(const char *name, float ax, float ay, bool shaken)
{
    fluid_t *f = fluid_create();
    fluid_renderer *renderer = render_create();
    assert(f && renderer);
    fluid_snapshot snapshot = {0};
    if (shaken) shake(f);
    float lowest_contrast = 1;
    int checked = 0;
    /* Include cold startup: pre-shaking every test hides the seeded packing
     * pattern.  Five-minute holds cover the slow formation of density seams. */
    for (int i = 0; i < 5 * 60 * PHYSICS_HZ; ++i) {
        assert(fluid_step(f, ax, ay));
        fluid_publish(f, &snapshot);
        render_reconstruct(renderer, &snapshot);
        assert(snapshot.metrics.resets == 0 && snapshot.metrics.particle_count == PARTICLE_COUNT);
        assert(fabsf(render_mass_ratio(renderer) - 1.0f) < 0.005f);
        if (i >= 15 * PHYSICS_HZ && i % (PHYSICS_HZ / 10) == 0) {
            float contrast = interior_contrast(renderer);
            if (contrast < lowest_contrast) lowest_contrast = contrast;
            ++checked;
        }
    }
    printf("idle color %s frames=%d minimum_interior_brightness=%.3f\n",
           name, checked, lowest_contrast);
    assert(lowest_contrast >= 0.65f);
    render_destroy(renderer);
    fluid_destroy(f);
}

static void idle_pool(const char *name, float ax, float ay)
{
    fluid_t *f = fluid_create();
    fluid_renderer *renderer = render_create();
    assert(f && renderer);
    fluid_snapshot snapshot = {0};
    shake(f);
    /* A face-up pin has no screen-plane gravity.  Start its hold with a
     * settled pool rather than the air gaps of an ongoing mid-shake splash. */
    if (ax == 0 && ay == 0)
        for (int i = 0; i < 15 * PHYSICS_HZ; ++i) assert(fluid_step(f, 0, GRAVITY));
    int worst = 0, checked = 0;
    for (int i = 0; i < 60 * PHYSICS_HZ; ++i) {
        assert(fluid_step(f, ax, ay));
        fluid_publish(f, &snapshot);
        render_reconstruct(renderer, &snapshot);
        assert(snapshot.metrics.resets == 0 && snapshot.metrics.particle_count == PARTICLE_COUNT);
        assert(fabsf(snapshot.metrics.render_mass_ratio - 1.0f) < 0.005f);
        assert(fabsf(render_mass_ratio(renderer) - 1.0f) < 0.005f);
        if (i >= 15 * PHYSICS_HZ) {
            int holes = interior_holes(renderer);
            if (holes > worst) worst = holes;
            ++checked;
        }
    }
    printf("idle %s frames=%d worst_interior_holes=%d speed=%.4f mass=%.6f\n",
           name, checked, worst, snapshot.metrics.average_speed, render_mass_ratio(renderer));
    assert(worst == 0);
    render_destroy(renderer);
    fluid_destroy(f);
}

static void air_and_mass(void)
{
    fluid_renderer *renderer = render_create();
    assert(renderer);
    fluid_snapshot snapshot = {0}, original;
    bool occupied[PIXEL_COUNT];
    /* Two separated pools must retain a real air gap.  This also checks
     * conservation at the two opposite display corners. */
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x) {
            snapshot.density[y * PIXEL_GRID_X + x] = 1;
            snapshot.density[(PIXEL_GRID_Y - 1 - y) * PIXEL_GRID_X + PIXEL_GRID_X - 1 - x] = 1;
        }
    original = snapshot;
    render_reconstruct(renderer, &snapshot);
    assert(memcmp(&snapshot, &original, sizeof(snapshot)) == 0);
    assert(fabsf(render_mass_ratio(renderer) * PARTICLE_COUNT - 50) < 0.001f);
    occupied_pixels(renderer, occupied);
    assert(occupied[0] && occupied[PIXEL_COUNT - 1]);
    for (int y = 0; y < PIXEL_GRID_Y; ++y)
        for (int x = 7; x < PIXEL_GRID_X - 7; ++x)
            assert(!occupied[y * PIXEL_GRID_X + x]);

    /* Once liquid leaves, persistence must clear rather than freezing an
     * occupancy mask.  Eight empty frames exceed the existing short trail. */
    memset(&snapshot, 0, sizeof(snapshot));
    for (int i = 0; i < 8; ++i) render_reconstruct(renderer, &snapshot);
    occupied_pixels(renderer, occupied);
    for (int i = 0; i < PIXEL_COUNT; ++i) assert(!occupied[i]);
    assert(render_mass_ratio(renderer) == 0);
    render_destroy(renderer);
}

static void wall_contact_reconstruction(void)
{
    fluid_renderer *renderer = render_create();
    fluid_snapshot snapshot = {0};
    bool occupied[PIXEL_COUNT];
    assert(renderer);

    /* A sparse splash against a pushed wall can place coverage in the second
     * display column.  It must reconstruct a continuous liquid boundary. */
    for (int y = 8; y < 16; ++y)
        snapshot.density[y * PIXEL_GRID_X + 1] = 0.20f;
    snapshot.acceleration_x = -GRAVITY;
    render_reconstruct(renderer, &snapshot);
    occupied_pixels(renderer, occupied);
    for (int y = 8; y < 16; ++y) {
        assert(occupied[y * PIXEL_GRID_X]);
        assert(occupied[y * PIXEL_GRID_X + 1]);
    }
    assert(fabsf(render_mass_ratio(renderer) * PARTICLE_COUNT - 1.6f) < 0.001f);

    /* The same sparse coverage must not keep a wall painted after force turns
     * away.  Eight frames exceed the short display persistence. */
    snapshot.acceleration_x = GRAVITY;
    for (int i = 0; i < 8; ++i) render_reconstruct(renderer, &snapshot);
    occupied_pixels(renderer, occupied);
    for (int y = 8; y < 16; ++y) {
        assert(!occupied[y * PIXEL_GRID_X]);
        assert(occupied[y * PIXEL_GRID_X + 1]);
    }
    render_destroy(renderer);
}

int main(void)
{
    air_and_mass();
    wall_contact_reconstruction();
    idle_pool("upright", 0, GRAVITY);
    idle_pool("inverted", 0, -GRAVITY);
    idle_pool("left", -GRAVITY, 0);
    idle_pool("right", GRAVITY, 0);
    idle_pool("diagonal-left", -GRAVITY * 0.70710678f, GRAVITY * 0.70710678f);
    idle_pool("diagonal-right", GRAVITY * 0.70710678f, GRAVITY * 0.70710678f);
    idle_pool("shallow-pitch", 0, GRAVITY * 0.25f);
    idle_pool("face-up", 0, 0);
    long_idle_color("cold-upright", 0, GRAVITY, false);
    long_idle_color("cold-inclined", 0.30f * GRAVITY, GRAVITY, false);
    long_idle_color("after-shake", 0, GRAVITY, true);
    puts("PASS: idle coverage and brightness, air gaps, density mass, clearing, immutable snapshots");
}
