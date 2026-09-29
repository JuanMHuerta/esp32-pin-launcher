#include <stdio.h>
#include <stdlib.h>

#include "renderer.h"

int main(int argc, char **argv)
{
    if (argc != 4 && argc != 11) {
        fprintf(stderr, "usage: preview MOOD TIME_MS OUTPUT.ppm [TILT_X TILT_Y ROTATION ENERGY PULSE_AGE_MS PULSE_X PULSE_Y]\n");
        return 2;
    }
    const int mood = atoi(argv[1]);
    const int time_ms = atoi(argv[2]);
    FILE *out = fopen(argv[3], "wb");
    if (!out) return 1;
    uint16_t *pixels = malloc(PIN_WIDTH * PIN_HEIGHT * sizeof(*pixels));
    if (!pixels) return 1;
    renderer_t scene;
    renderer_init();
    float tilt_x = argc == 11 ? strtof(argv[4], NULL) : 0.15f;
    float tilt_y = argc == 11 ? strtof(argv[5], NULL) : -0.2f;
    float rotation = argc == 11 ? strtof(argv[6], NULL) : 0.0f;
    float energy = argc == 11 ? strtof(argv[7], NULL) : 0.0f;
    int pulse_age = argc == 11 ? atoi(argv[8]) : -1;
    int pulse_x = argc == 11 ? atoi(argv[9]) : PIN_WIDTH / 2;
    int pulse_y = argc == 11 ? atoi(argv[10]) : PIN_HEIGHT / 2;
    renderer_prepare(&scene, time_ms, tilt_x, tilt_y, rotation, energy,
                     mood, pulse_x, pulse_y, pulse_age);
    renderer_strip(&scene, 0, PIN_HEIGHT, pixels);
    fprintf(out, "P6\n%d %d\n255\n", PIN_WIDTH, PIN_HEIGHT);
    for (int i = 0; i < PIN_WIDTH * PIN_HEIGHT; ++i) {
        uint16_t color = __builtin_bswap16(pixels[i]);
        unsigned char rgb[3] = {
            (unsigned char)(((color >> 11) & 31) * 255 / 31),
            (unsigned char)(((color >> 5) & 63) * 255 / 63),
            (unsigned char)((color & 31) * 255 / 31),
        };
        fwrite(rgb, 1, sizeof(rgb), out);
    }
    fclose(out);
    free(pixels);
    return 0;
}
