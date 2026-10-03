// SPDX-License-Identifier: GPL-3.0-only
#include "preview.h"
#include "pet.h"
#include "paint.h"
#include <math.h>

static uint16_t pixels[PET_W * PET_H];

int main(int argc, char **argv)
{
    unsigned frames, fps;
    if (!preview_parameters(argc, argv, &frames, &fps)) {
        return 2;
    }
    pet_t pet;
    pet_init(&pet, 42);
    pet_set_state(&pet, PET_WAVE, 1800);
    for (unsigned frame = 0; frame < frames; ++frame) {
        if (frame == 3 * fps) {
            pet_event(&pet, PET_TAP, (int)pet.x, 30);
        }
        if (frame == 6 * fps) {
            pet_event(&pet, PET_TAP, 110, 30);
        }
        pet_set_tilt(&pet, 0.15f * sinf((float)frame / fps));
        pet_paint(&pet, pixels);
        if (!preview_write(pixels, PET_W * PET_H)) {
            return 3;
        }
        pet_step(&pet, (frame + 1) * 1000 / fps - frame * 1000 / fps);
    }
    return fflush(stdout) != 0;
}
