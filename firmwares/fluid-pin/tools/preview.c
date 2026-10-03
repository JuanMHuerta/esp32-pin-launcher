// SPDX-License-Identifier: GPL-3.0-only
#include "fluid.h"
#include "render.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_preview(fluid_renderer *renderer, const fluid_snapshot *snapshot,
                          const char *path)
{
    render_reconstruct(renderer, snapshot);
    FILE *file = fopen(path, "wb");
    assert(file);
    fprintf(file, "P6\n%d %d\n255\n", PANEL_W, PANEL_H);
    uint16_t band[PANEL_W * FLOW_BAND_ROWS + 1];
    for (int y = 0; y < PANEL_H; y += FLOW_BAND_ROWS) {
        band[PANEL_W * FLOW_BAND_ROWS] = 0xabcd;
        render_band(renderer, y, FLOW_BAND_ROWS, band);
        assert(band[PANEL_W * FLOW_BAND_ROWS] == 0xabcd);
        for (int i = 0; i < PANEL_W * FLOW_BAND_ROWS; ++i) {
            uint16_t color = __builtin_bswap16(band[i]);
            uint8_t rgb[3] = {((color >> 11) & 31) * 255 / 31, ((color >> 5) & 63) * 255 / 63,
                              (color & 31) * 255 / 31};
            fwrite(rgb, 1, sizeof(rgb), file);
        }
    }
    fclose(file);
}

static fluid_metrics publish_and_check(fluid_t *fluid, fluid_snapshot *snapshot,
                                       fluid_renderer *renderer)
{
    fluid_publish(fluid, snapshot);
    render_reconstruct(renderer, snapshot);
    fluid_metrics metrics = snapshot->metrics;
    assert(metrics.particle_count == PARTICLE_COUNT);
    assert(metrics.render_mass_ratio >= 0.995f && metrics.render_mass_ratio <= 1.005f);
    assert(render_mass_ratio(renderer) >= 0.995f && render_mass_ratio(renderer) <= 1.005f);
    return metrics;
}

static fluid_metrics advance(fluid_t *fluid, fluid_snapshot *snapshot, fluid_renderer *renderer,
                             int count, float ax, float ay)
{
    fluid_metrics metrics = {0};
    for (int i = 0; i < count; ++i) {
        assert(fluid_step(fluid, ax, ay));
        metrics = publish_and_check(fluid, snapshot, renderer);
    }
    return metrics;
}

static void synthetic_stress(fluid_t *fluid, fluid_snapshot *snapshot, fluid_renderer *renderer)
{
    for (int impulse = 0; impulse < 20; ++impulse) {
        float angle = impulse * 2.39996323f;
        float ax = 2.0f * GRAVITY * cosf(angle);
        float ay = GRAVITY + 2.0f * GRAVITY * sinf(angle);
        advance(fluid, snapshot, renderer, 4, ax, ay);
    }
    fluid_metrics after_impulses = publish_and_check(fluid, snapshot, renderer);
    assert(after_impulses.particle_count == 573);
    assert(after_impulses.resets == 0);
    printf("impulses n=%d mass=%.6f ceiling=%d max_speed=%.3f\n", after_impulses.particle_count,
           after_impulses.render_mass_ratio, after_impulses.ceiling_particles,
           after_impulses.maximum_speed);
}

static void simulated_long_run(fluid_t *fluid, fluid_snapshot *snapshot, fluid_renderer *renderer)
{
    /* 30 simulated minutes.  This is opt-in because the default validation is
     * designed for the edit/test loop; it exercises the exact steady-state
     * code path, with no host-only solver branch. */
    for (int step = 0; step < 30 * 60 * PHYSICS_HZ; ++step) {
        int phase = step % (3 * PHYSICS_HZ);
        float ax = 0.0f;
        float ay = GRAVITY;
        if (phase < 8) {
            float angle = (step / 8) * 1.61803399f;
            ax = 2.0f * GRAVITY * cosf(angle);
            ay += 2.0f * GRAVITY * sinf(angle);
        }
        assert(fluid_step(fluid, ax, ay));
        if ((step % 20) == 0) {
            (void)publish_and_check(fluid, snapshot, renderer);
        }
    }
    fluid_metrics metrics = publish_and_check(fluid, snapshot, renderer);
    assert(metrics.particle_count == PARTICLE_COUNT && metrics.resets == 0);
    printf("30min-sim n=%d mass=%.6f ceiling=%d avg_speed=%.4f resets=%u\n", metrics.particle_count,
           metrics.render_mass_ratio, metrics.ceiling_particles, metrics.average_speed,
           metrics.resets);
}

int main(int argc, char **argv)
{
    bool soak = argc > 1 && strcmp(argv[1], "--soak") == 0;
    fluid_t *fluid = fluid_create();
    fluid_renderer *renderer = render_create();
    fluid_snapshot snapshot = {0};
    assert(fluid && renderer);

    fluid_metrics initial = publish_and_check(fluid, &snapshot, renderer);
    printf("V9 n=%d solver_bytes=%zu initial_mass=%.6f\n", initial.particle_count,
           fluid_memory_bytes(), initial.render_mass_ratio);
    write_preview(renderer, &snapshot, "fluid-seed.ppm");

    synthetic_stress(fluid, &snapshot, renderer);
    fluid_metrics five_seconds = advance(fluid, &snapshot, renderer, 5 * PHYSICS_HZ, 0.0f, GRAVITY);
    printf("five_seconds ceiling=%d avg_speed=%.4f\n", five_seconds.ceiling_particles,
           five_seconds.average_speed);
    assert(five_seconds.ceiling_particles <= 4);
    fluid_metrics settled = advance(fluid, &snapshot, renderer, 10 * PHYSICS_HZ, 0.0f, GRAVITY);
    printf("settled ceiling=%d avg_speed=%.4f max_speed=%.4f div=%.4f->%.4f\n",
           settled.ceiling_particles, settled.average_speed, settled.maximum_speed,
           settled.divergence_before, settled.divergence_after);
    assert(settled.ceiling_particles == 0);
    assert(settled.average_speed < 0.35f);
    assert(settled.resets == 0);
    /* Check the solver's physical constraint, beyond the fixed-particle
     * visual-mass check: projection should remove most residual divergence. */
    assert(settled.divergence_after < 0.05f);
    assert(settled.divergence_after < settled.divergence_before * 0.25f);
    write_preview(renderer, &snapshot, "fluid-settled.ppm");

    fluid_metrics right = advance(fluid, &snapshot, renderer, 4 * PHYSICS_HZ, GRAVITY, 0.0f);
    assert(right.particle_count == PARTICLE_COUNT);
    assert(right.resets == 0);
    write_preview(renderer, &snapshot, "fluid-right.ppm");
    if (soak) {
        simulated_long_run(fluid, &snapshot, renderer);
    }

    assert(!fluid_step(fluid, NAN, 0.0f));
    assert(fluid_step(fluid, 0.0f, GRAVITY));
    fluid_inject_invalid(fluid);
    assert(!fluid_step(fluid, 0.0f, GRAVITY));
    fluid_destroy(fluid);
    render_destroy(renderer);
    puts("PASS: particle conservation, render-mass invariant, 20 impulses, ceiling recovery, "
         "settling, pixel bands");
}
