// SPDX-License-Identifier: GPL-3.0-only
#include "preview.h"
#include "life.h"
#include "palette.h"

static uint8_t first[LIFE_CELLS], second[LIFE_CELLS];
static uint16_t colors[LIFE_MAX_AGE + 1][LIFE_CELL_PIXELS * LIFE_CELL_PIXELS];
static uint16_t pixels[LIFE_DISPLAY_WIDTH * LIFE_DISPLAY_HEIGHT];

int main(int argc, char **argv)
{
    unsigned frames, fps;
    if (!preview_parameters(argc, argv, &frames, &fps)) {
        return 2;
    }
    life_t life;
    life_init(&life, first, second, 123);
    life_seed(&life);
    palette_build(colors);
    for (unsigned frame = 0; frame < frames; ++frame) {
        for (int y = 0; y < LIFE_HEIGHT; ++y) {
            for (int x = 0; x < LIFE_WIDTH; ++x) {
                uint8_t state = life.current[y * LIFE_WIDTH + x];
                for (int row = 0; row < LIFE_CELL_PIXELS; ++row) {
                    for (int col = 0; col < LIFE_CELL_PIXELS; ++col) {
                        int px = LIFE_OFFSET_X + x * LIFE_CELL_PIXELS + col;
                        int py = y * LIFE_CELL_PIXELS + row;
                        pixels[py * LIFE_DISPLAY_WIDTH + px] =
                            colors[state][row * LIFE_CELL_PIXELS + col];
                    }
                }
            }
        }
        if (!preview_write(pixels, LIFE_DISPLAY_WIDTH * LIFE_DISPLAY_HEIGHT)) {
            return 3;
        }
        unsigned next = (frame + 1) * 10 / fps;
        while (life.generation < next) {
            life_step(&life);
            if (life.generation % LIFE_EDGE_INTERVAL == 0 ||
                life.quiet_generations >= LIFE_STILL_GENERATIONS) {
                if (life_inject_edge(&life)) {
                    life.quiet_generations = 0;
                }
            }
        }
    }
    return fflush(stdout) != 0;
}
