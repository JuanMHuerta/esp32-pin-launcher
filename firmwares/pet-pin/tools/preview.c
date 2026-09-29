#include "pet.h"
#include "paint.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 5) { fprintf(stderr, "usage: preview state frames day_ms output.rgb\n"); return 2; }
    int state = atoi(argv[1]), frames = atoi(argv[2]);
    if (state < 0 || state >= PET_STATE_COUNT || frames < 1 || frames > 20000) return 2;
    FILE *file = fopen(argv[4], "wb");
    if (!file) return 1;
    pet_t pet;
    pet_init(&pet, 42);
    pet.uptime_ms = strtoull(argv[3], NULL, 10);
    pet_set_state(&pet, (pet_state_t)state, 600000);
    uint16_t pixels[PET_W * PET_H];
    unsigned char rgb[PET_W * PET_H * 3];
    for (int i = 0; i < frames; ++i) {
        pet_step(&pet, i % 3 == 0 ? 34 : 33);
        pet_paint(&pet, pixels);
        for (int k = 0; k < PET_W * PET_H; ++k) {
            rgb[k * 3] = ((pixels[k] >> 11) & 31) * 255 / 31;
            rgb[k * 3 + 1] = ((pixels[k] >> 5) & 63) * 255 / 63;
            rgb[k * 3 + 2] = (pixels[k] & 31) * 255 / 31;
        }
        if (fwrite(rgb, sizeof(rgb), 1, file) != 1) return 1;
    }
    return fclose(file) != 0;
}
