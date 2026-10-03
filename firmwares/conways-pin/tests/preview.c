// SPDX-License-Identifier: GPL-3.0-only
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "life.h"
#include "palette.h"

#define DISPLAY_WIDTH LIFE_DISPLAY_WIDTH
#define DISPLAY_HEIGHT LIFE_DISPLAY_HEIGHT

static uint8_t first[LIFE_CELLS];
static uint8_t second[LIFE_CELLS];
static uint16_t colors[LIFE_MAX_AGE + 1][LIFE_CELL_PIXELS * LIFE_CELL_PIXELS];

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s GENERATION OUTPUT.ppm\n", argv[0]);
        return 2;
    }
    int target = atoi(argv[1]);
    if (target < 0) {
        return 2;
    }

    life_t life;
    life_init(&life, first, second, 123);
    life_seed(&life);
    palette_build(colors);
    for (int i = 0; i < target; ++i) {
        life_step(&life);
        if (life.generation % LIFE_EDGE_INTERVAL == 0 ||
            life.quiet_generations >= LIFE_STILL_GENERATIONS) {
            if (life_inject_edge(&life)) {
                life.quiet_generations = 0;
            }
        }
    }

    FILE *out = fopen(argv[2], "wb");
    if (!out) {
        perror(argv[2]);
        return 1;
    }
    fprintf(out, "P6\n%d %d\n255\n", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    uint8_t row[DISPLAY_WIDTH * 3];
    for (int y = 0; y < LIFE_HEIGHT; ++y) {
        for (int pixel_row = 0; pixel_row < LIFE_CELL_PIXELS; ++pixel_row) {
            memset(row, 0, sizeof(row));
            for (int x = 0; x < LIFE_WIDTH; ++x) {
                uint8_t state = life.current[y * LIFE_WIDTH + x];
                for (int pixel = 0; pixel < LIFE_CELL_PIXELS; ++pixel) {
                    uint16_t color = colors[state][pixel_row * LIFE_CELL_PIXELS + pixel];
                    uint8_t red = (color >> 11) & 31;
                    uint8_t green = (color >> 5) & 63;
                    uint8_t blue = color & 31;
                    int at = (LIFE_OFFSET_X + LIFE_CELL_PIXELS * x + pixel) * 3;
                    row[at] = (red << 3) | (red >> 2);
                    row[at + 1] = (green << 2) | (green >> 4);
                    row[at + 2] = (blue << 3) | (blue >> 2);
                }
            }
            fwrite(row, 1, sizeof(row), out);
        }
    }
    if (fclose(out) != 0) {
        perror(argv[2]);
        return 1;
    }
    return 0;
}
