// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
#include <stdint.h>

enum {
    MAZE_W = 268,
    MAZE_H = 120,
    MAZE_SCALE = 2,
    DISPLAY_W = 536,
    DISPLAY_H = 240,
    MAZE_GRID = 10
};
/* Directions are clockwise in map coordinates: east, south, west, north. */
typedef enum { MAZE_TITLE, MAZE_WALK, MAZE_EXIT } maze_mode_t;
typedef struct {
    uint8_t walls[MAZE_GRID][MAZE_GRID];
    uint32_t rng, generation, elapsed_ms, mode_ms;
    int cell_x, cell_y, next_x, next_y, exit_x, exit_y, heading;
    float x, y, angle;
    float object_x, object_y;
    int rat_x, rat_y, rat_next_x, rat_next_y, rat_heading;
    uint8_t object_kind; /* 0 none, 1 rat, 2 octahedron */
    maze_mode_t mode;
} maze_t;

void maze_init(maze_t *m, uint32_t seed);
void maze_step(maze_t *m, uint32_t dt_ms);
bool maze_connected(const maze_t *m);
bool maze_open(const maze_t *m, int x, int y);
bool maze_passage(const maze_t *m, int x, int y, int direction);
