#pragma once

#include <stdint.h>

#define LIFE_CELL_PIXELS 6
#define LIFE_DISPLAY_WIDTH 536
#define LIFE_DISPLAY_HEIGHT 240
#define LIFE_WIDTH (LIFE_DISPLAY_WIDTH / LIFE_CELL_PIXELS)
#define LIFE_HEIGHT (LIFE_DISPLAY_HEIGHT / LIFE_CELL_PIXELS)
#define LIFE_OFFSET_X ((LIFE_DISPLAY_WIDTH - LIFE_WIDTH * LIFE_CELL_PIXELS) / 2)
#define LIFE_CELLS (LIFE_WIDTH * LIFE_HEIGHT)
#define LIFE_EDGE_INTERVAL 150
#define LIFE_STILL_MOTION_THRESHOLD 12
#define LIFE_STILL_GENERATIONS 80

// 0: black; 1..4: fading magenta death trace; 5: white birth; 6..20: cyan live cell.
#define LIFE_ALIVE 5
#define LIFE_MAX_AGE 20

typedef struct {
    uint8_t *current;
    uint8_t *next;
    uint32_t generation;
    uint32_t rng;
    uint32_t launches;
    uint16_t quiet_generations;
} life_t;

typedef struct {
    uint32_t living;
    uint32_t births;
    uint32_t motion;
} life_stats_t;

typedef enum {
    LIFE_STAMP_R_PENTOMINO,
    LIFE_STAMP_B_HEPTOMINO,
    LIFE_STAMP_DIEHARD,
    LIFE_STAMP_SQUARE,
    LIFE_STAMP_COUNT,
} life_stamp_kind_t;

void life_init(life_t *life, uint8_t *first, uint8_t *second, uint32_t seed);
void life_seed(life_t *life);
// Add one of four patterns at random, with a random quarter-turn.
// Returns the number of newly live cells.
int life_stamp(life_t *life, int cell_x, int cell_y);
// Explicit pattern and quarter-turn, also used by the host pattern checks.
int life_stamp_pattern(life_t *life, int cell_x, int cell_y,
                       life_stamp_kind_t kind, int rotation);
life_stats_t life_step(life_t *life);
// Launch a glider inward from the next visible screen edge.
// Returns one when placed, or zero if no clear entry lane exists.
int life_inject_edge(life_t *life);
