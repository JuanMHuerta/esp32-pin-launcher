// SPDX-License-Identifier: GPL-3.0-only
#include "preview.h"
#include "fluid.h"
#include "render.h"
#include <math.h>

static fluid_snapshot snapshot;
static uint16_t pixels[PANEL_W * PANEL_H];

int main(int argc, char **argv)
{
    unsigned frames, fps;
    if (!preview_parameters(argc, argv, &frames, &fps)) {
        return 2;
    }
    fluid_t *fluid = fluid_create();
    fluid_renderer *renderer = render_create();
    if (!fluid || !renderer) {
        return 3;
    }
    for (unsigned step = 0; step < PHYSICS_HZ; ++step) {
        if (!fluid_step(fluid, 0, GRAVITY)) {
            return 4;
        }
    }
    unsigned step = 0;
    for (unsigned frame = 0; frame < frames; ++frame) {
        unsigned target = (frame + 1) * PHYSICS_HZ / fps;
        while (step < target) {
            float time = (float)step++ / PHYSICS_HZ;
            float angle = 1.7f * sinf(time * 0.85f);
            if (!fluid_step(fluid, GRAVITY * sinf(angle), GRAVITY * cosf(angle))) {
                return 4;
            }
        }
        fluid_publish(fluid, &snapshot);
        render_reconstruct(renderer, &snapshot);
        render_band(renderer, 0, PANEL_H, pixels);
        for (int i = 0; i < PANEL_W * PANEL_H; ++i) {
            pixels[i] = __builtin_bswap16(pixels[i]);
        }
        if (!preview_write(pixels, PANEL_W * PANEL_H)) {
            return 5;
        }
    }
    fluid_destroy(fluid);
    render_destroy(renderer);
    return fflush(stdout) != 0;
}
