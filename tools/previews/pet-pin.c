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
    static const pet_state_t showcase[] = {PET_WAVE,  PET_GROOM,   PET_DANCE,  PET_LOOK,
                                           PET_SLEEP, PET_STRETCH, PET_BALANCE};
    unsigned prior = PET_STATE_COUNT;
    for (unsigned frame = 0; frame < frames; ++frame) {
        unsigned scene = frame * (sizeof(showcase) / sizeof(showcase[0])) / frames;
        if (scene != prior) {
            pet_set_state(&pet, showcase[scene], 600000);
            prior = scene;
        }
        // Compress a six-minute cycle for the README; poses use real frame time.
        pet.uptime_ms = (uint64_t)frame * PET_DAY_MS / frames;
        pet_set_tilt(&pet, showcase[scene] == PET_BALANCE
                               ? .75f + .2f * sinf((float)frame / fps * 1.5f)
                               : 0);
        pet_paint(&pet, pixels);
        if (!preview_write(pixels, PET_W * PET_H)) {
            return 3;
        }
        pet_step(&pet, (frame + 1) * 1000 / fps - frame * 1000 / fps);
    }
    return fflush(stdout) != 0;
}
