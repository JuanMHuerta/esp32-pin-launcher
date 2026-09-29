#include "life.h"

#include <stdbool.h>
#include <string.h>

static uint32_t random32(life_t *life)
{
    uint32_t x = life->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    life->rng = x;
    return x;
}

static void rotate_cell(int dx, int dy, int width, int height, int rotation,
                        int *x, int *y)
{
    *x = rotation == 1 ? height - 1 - dy :
         rotation == 2 ? width - 1 - dx : rotation == 3 ? dy : dx;
    *y = rotation == 1 ? dx : rotation == 2 ? height - 1 - dy :
         rotation == 3 ? width - 1 - dx : dy;
}

static const uint8_t glider[5][2] = {
    {1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2},
};

static const uint8_t r_pentomino[5][2] = {
    {1, 0}, {2, 0}, {0, 1}, {1, 1}, {1, 2},
};

// LifeWiki RLE: ob2o$3o$bo!
static const uint8_t b_heptomino[7][2] = {
    {0, 0}, {2, 0}, {3, 0}, {0, 1}, {1, 1}, {2, 1}, {1, 2},
};

// LifeWiki RLE: 6bo$2o$bo3b3o!
static const uint8_t diehard[7][2] = {
    {6, 0}, {0, 1}, {1, 1}, {1, 2}, {5, 2}, {6, 2}, {7, 2},
};

static const uint8_t solid_square[9][2] = {
    {0, 0}, {1, 0}, {2, 0},
    {0, 1}, {1, 1}, {2, 1},
    {0, 2}, {1, 2}, {2, 2},
};

typedef struct {
    const uint8_t (*cells)[2];
    int count;
    int width;
    int height;
} pattern_t;

static pattern_t stamp_pattern(life_stamp_kind_t kind)
{
    switch (kind) {
    case LIFE_STAMP_B_HEPTOMINO:
        return (pattern_t) {b_heptomino, 7, 4, 3};
    case LIFE_STAMP_DIEHARD:
        return (pattern_t) {diehard, 7, 8, 3};
    case LIFE_STAMP_SQUARE:
        return (pattern_t) {solid_square, 9, 3, 3};
    case LIFE_STAMP_R_PENTOMINO:
    default:
        return (pattern_t) {r_pentomino, 5, 3, 3};
    }
}

void life_init(life_t *life, uint8_t *first, uint8_t *second, uint32_t seed)
{
    *life = (life_t) {
        .current = first,
        .next = second,
        .rng = seed ? seed : 0x6d2b79f5,
    };
    memset(first, 0, LIFE_CELLS);
    memset(second, 0, LIFE_CELLS);
}

void life_seed(life_t *life)
{
    memset(life->current, 0, LIFE_CELLS);

    const int phase = (int)(random32(life) % 5);
    // Four spaced motifs spread the initial activity across the screen.
    for (int tile = 0; tile < 4; ++tile) {
        const int column = tile % 2;
        const int row = tile / 2;
        const int x_slot = LIFE_WIDTH / 2;
        const int y_slot = LIFE_HEIGHT / 2;
        const int x0 = column * x_slot + 3 + (int)(random32(life) % (x_slot - 6));
        const int y0 = row * y_slot + 3 + (int)(random32(life) % (y_slot - 6));
        const int rotation = (int)(random32(life) & 3);
        const uint8_t (*pattern)[2] = (tile + phase) % 5 == 0 ? r_pentomino : glider;
        for (int cell = 0; cell < 5; ++cell) {
            int dx, dy;
            rotate_cell(pattern[cell][0], pattern[cell][1], 3, 3,
                        rotation, &dx, &dy);
            life->current[(y0 + dy) * LIFE_WIDTH + x0 + dx] = LIFE_ALIVE;
        }
    }
    life->generation = 0;
    life->quiet_generations = 0;
}

int life_stamp(life_t *life, int cell_x, int cell_y)
{
    const life_stamp_kind_t kind = (life_stamp_kind_t)(random32(life) % LIFE_STAMP_COUNT);
    const int rotation = (int)(random32(life) & 3);
    return life_stamp_pattern(life, cell_x, cell_y, kind, rotation);
}

int life_stamp_pattern(life_t *life, int cell_x, int cell_y,
                       life_stamp_kind_t kind, int rotation)
{
    const pattern_t pattern = stamp_pattern(kind);
    rotation &= 3;
    const int width = rotation & 1 ? pattern.height : pattern.width;
    const int height = rotation & 1 ? pattern.width : pattern.height;
    int x0 = cell_x - width / 2;
    int y0 = cell_y - height / 2;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x0 > LIFE_WIDTH - width) x0 = LIFE_WIDTH - width;
    if (y0 > LIFE_HEIGHT - height) y0 = LIFE_HEIGHT - height;
    int added = 0;
    for (int cell = 0; cell < pattern.count; ++cell) {
        int dx, dy;
        rotate_cell(pattern.cells[cell][0], pattern.cells[cell][1],
                    pattern.width, pattern.height, rotation, &dx, &dy);
        uint8_t *state = &life->current[(y0 + dy) * LIFE_WIDTH + x0 + dx];
        added += *state < LIFE_ALIVE;
        *state = LIFE_ALIVE;
    }
    life->quiet_generations = 0;
    return added;
}

int life_inject_edge(life_t *life)
{
    const unsigned launch = life->launches++;
    const int edge = launch & 3; // top, right, bottom, left
    const int alternate = (launch >> 2) & 1;
    const int rotation = edge == 0 ? (alternate ? 1 : 0) :
                         edge == 1 ? (alternate ? 1 : 2) :
                         edge == 2 ? (alternate ? 3 : 2) : (alternate ? 3 : 0);
    const int span = (edge & 1) ? LIFE_HEIGHT : LIFE_WIDTH;

    for (int attempt = 0; attempt < 16; ++attempt) {
        const int lane = 4 + (int)(random32(life) % (span - 36));
        bool clear = true;
        const int x0 = edge == 1 ? LIFE_WIDTH - 5 : edge == 3 ? 2 : lane;
        const int y0 = edge == 0 ? 2 : edge == 2 ? LIFE_HEIGHT - 5 : lane;
        for (int y = y0 - 1; y <= y0 + 3 && clear; ++y) {
            for (int x = x0 - 1; x <= x0 + 3; ++x) {
                if (life->current[y * LIFE_WIDTH + x] >= LIFE_ALIVE) {
                    clear = false;
                    break;
                }
            }
        }
        if (!clear) {
            continue;
        }
        for (int cell = 0; cell < 5; ++cell) {
            int dx, dy;
            rotate_cell(glider[cell][0], glider[cell][1], 3, 3,
                        rotation, &dx, &dy);
            life->current[(y0 + dy) * LIFE_WIDTH + x0 + dx] = LIFE_ALIVE;
        }
        return 1;
    }
    return 0;
}

life_stats_t life_step(life_t *life)
{
    const uint8_t *src = life->current;
    uint8_t *dst = life->next;
    life_stats_t stats = {0};

    for (int y = 0; y < LIFE_HEIGHT; ++y) {
        const int up = (y ? y - 1 : LIFE_HEIGHT - 1) * LIFE_WIDTH;
        const int row = y * LIFE_WIDTH;
        const int down = (y + 1 == LIFE_HEIGHT ? 0 : y + 1) * LIFE_WIDTH;
        for (int x = 0; x < LIFE_WIDTH; ++x) {
            const int left = x ? x - 1 : LIFE_WIDTH - 1;
            const int right = x + 1 == LIFE_WIDTH ? 0 : x + 1;
            const unsigned neighbors =
                (src[up + left] >= LIFE_ALIVE) + (src[up + x] >= LIFE_ALIVE) +
                (src[up + right] >= LIFE_ALIVE) + (src[row + left] >= LIFE_ALIVE) +
                (src[row + right] >= LIFE_ALIVE) + (src[down + left] >= LIFE_ALIVE) +
                (src[down + x] >= LIFE_ALIVE) + (src[down + right] >= LIFE_ALIVE);
            const uint8_t old = src[row + x];
            uint8_t value;
            if (neighbors == 3 || (old >= LIFE_ALIVE && neighbors == 2)) {
                value = old >= LIFE_ALIVE ? (old < LIFE_MAX_AGE ? old + 1 : old) : LIFE_ALIVE;
                stats.living++;
                stats.births += old < LIFE_ALIVE;
            } else {
                value = old >= LIFE_ALIVE ? 4 : (old ? old - 1 : 0);
            }
            stats.motion += (old >= LIFE_ALIVE) != (value >= LIFE_ALIVE);
            dst[row + x] = value;
        }
    }
    life->current = dst;
    life->next = (uint8_t *)src;
    life->generation++;
    if (stats.motion < LIFE_STILL_MOTION_THRESHOLD) {
        if (life->quiet_generations < LIFE_STILL_GENERATIONS) {
            life->quiet_generations++;
        }
    } else {
        life->quiet_generations = 0;
    }
    return stats;
}
