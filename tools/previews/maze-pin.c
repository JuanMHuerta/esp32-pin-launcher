// SPDX-License-Identifier: GPL-3.0-only
#include "preview.h"
#include "maze.h"
#include "paint.h"

static uint16_t pixels[MAZE_W * MAZE_H];

int main(int argc, char **argv)
{
    unsigned frames, fps;
    if (!preview_parameters(argc, argv, &frames, &fps)) {
        return 2;
    }
    maze_t maze;
    maze_init(&maze, 12345);
    for (unsigned t = 0; t < 1500; t += 25) {
        maze_step(&maze, 25);
    }
    for (unsigned frame = 0; frame < frames; ++frame) {
        maze_paint(&maze, pixels);
        if (!preview_write(pixels, MAZE_W * MAZE_H)) {
            return 3;
        }
        unsigned remaining = (frame + 1) * 1000 / fps - frame * 1000 / fps;
        while (remaining) {
            unsigned step = remaining > 25 ? 25 : remaining;
            maze_step(&maze, step);
            remaining -= step;
        }
    }
    return fflush(stdout) != 0;
}
