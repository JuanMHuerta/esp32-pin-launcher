// SPDX-License-Identifier: GPL-3.0-only
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "life.h"

static uint8_t first[LIFE_CELLS];
static uint8_t second[LIFE_CELLS];

static int alive(const life_t *life, int x, int y)
{
    return life->current[y * LIFE_WIDTH + x] >= LIFE_ALIVE;
}

static void live_sums(const life_t *life, int *count, int *x_sum, int *y_sum)
{
    *count = *x_sum = *y_sum = 0;
    for (int y = 0; y < LIFE_HEIGHT; ++y) {
        for (int x = 0; x < LIFE_WIDTH; ++x) {
            if (alive(life, x, y)) {
                (*count)++;
                *x_sum += x;
                *y_sum += y;
            }
        }
    }
}

static void live_bounds(const life_t *life, int *width, int *height)
{
    int min_x = LIFE_WIDTH, min_y = LIFE_HEIGHT, max_x = -1, max_y = -1;
    for (int y = 0; y < LIFE_HEIGHT; ++y) {
        for (int x = 0; x < LIFE_WIDTH; ++x) {
            if (alive(life, x, y)) {
                if (x < min_x) {
                    min_x = x;
                }
                if (x > max_x) {
                    max_x = x;
                }
                if (y < min_y) {
                    min_y = y;
                }
                if (y > max_y) {
                    max_y = y;
                }
            }
        }
    }
    *width = max_x - min_x + 1;
    *height = max_y - min_y + 1;
}

static void expect_rows(const life_t *life, int x0, int y0, const char *const *rows, int width,
                        int height)
{
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            assert(alive(life, x0 + x, y0 + y) == (rows[y][x] == 'O'));
        }
    }
}

int main(void)
{
    life_t life;
    life_init(&life, first, second, 123);

    // A horizontal blinker crossing both screen seams becomes vertical.
    life.current[LIFE_WIDTH - 1] = LIFE_ALIVE;
    life.current[0] = LIFE_ALIVE;
    life.current[1] = LIFE_ALIVE;
    life_stats_t stats = life_step(&life);
    assert(stats.living == 3 && stats.births == 2);
    assert(alive(&life, 0, LIFE_HEIGHT - 1));
    assert(alive(&life, 0, 0));
    assert(alive(&life, 0, 1));
    assert(!alive(&life, LIFE_WIDTH - 1, 0));

    stats = life_step(&life);
    assert(stats.living == 3 && stats.births == 2);
    assert(alive(&life, LIFE_WIDTH - 1, 0));
    assert(alive(&life, 0, 0));
    assert(alive(&life, 1, 0));

    // Every tap pattern has the requested cell count and all four rotations
    // remain bounded when placed near any corner.
    const int pattern_count[] = {5, 7, 7, 9};
    const int pattern_width[] = {3, 4, 8, 3};
    const int pattern_height[] = {3, 3, 3, 3};
    for (int kind = 0; kind < LIFE_STAMP_COUNT; ++kind) {
        for (int rotation = 0; rotation < 4; ++rotation) {
            for (int corner = 0; corner < 4; ++corner) {
                const int x = corner & 1 ? LIFE_WIDTH - 1 : 0;
                const int y = corner & 2 ? LIFE_HEIGHT - 1 : 0;
                life_init(&life, first, second, 123);
                assert(life_stamp_pattern(&life, x, y, kind, rotation) == pattern_count[kind]);
                int count, x_sum, y_sum, width, height;
                live_sums(&life, &count, &x_sum, &y_sum);
                live_bounds(&life, &width, &height);
                assert(count == pattern_count[kind]);
                assert(width == (rotation & 1 ? pattern_height[kind] : pattern_width[kind]));
                assert(height == (rotation & 1 ? pattern_width[kind] : pattern_height[kind]));
            }
        }
    }
    life_init(&life, first, second, 123);
    assert(life_stamp_pattern(&life, LIFE_WIDTH / 2, LIFE_HEIGHT / 2, LIFE_STAMP_SQUARE, 0) == 9);
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            assert(alive(&life, LIFE_WIDTH / 2 + dx, LIFE_HEIGHT / 2 + dy));
        }
    }
    static const char *const b_rows[] = {"O.OO", "OOO.", ".O.."};
    static const char *const diehard_rows[] = {
        "......O.",
        "OO......",
        ".O...OOO",
    };
    life_init(&life, first, second, 123);
    assert(life_stamp_pattern(&life, 60, 30, LIFE_STAMP_B_HEPTOMINO, 0) == 7);
    expect_rows(&life, 58, 29, b_rows, 4, 3);
    life_init(&life, first, second, 123);
    assert(life_stamp_pattern(&life, 60, 30, LIFE_STAMP_DIEHARD, 0) == 7);
    expect_rows(&life, 56, 29, diehard_rows, 8, 3);

    bool seen[LIFE_STAMP_COUNT] = {false};
    for (uint32_t seed = 1; seed <= 64; ++seed) {
        life_init(&life, first, second, seed);
        const int count = life_stamp(&life, 60, 30);
        int width, height;
        live_bounds(&life, &width, &height);
        const int kind = count == 5                  ? LIFE_STAMP_R_PENTOMINO
                         : count == 9                ? LIFE_STAMP_SQUARE
                         : width == 8 || height == 8 ? LIFE_STAMP_DIEHARD
                                                     : LIFE_STAMP_B_HEPTOMINO;
        seen[kind] = true;
    }
    for (int kind = 0; kind < LIFE_STAMP_COUNT; ++kind) {
        assert(seen[kind]);
    }

    // Stillness means sustained low live/dead movement, even if stable cells
    // remain on screen. New user input starts a fresh stillness window.
    life_init(&life, first, second, 123);
    for (int generation = 0; generation < LIFE_STILL_GENERATIONS; ++generation) {
        stats = life_step(&life);
        assert(stats.motion == 0);
        assert(life.quiet_generations == generation + 1);
    }
    assert(life_stamp_pattern(&life, 50, 20, LIFE_STAMP_B_HEPTOMINO, 0) == 7);
    assert(life.quiet_generations == 0);
    life_stamp_pattern(&life, 70, 20, LIFE_STAMP_SQUARE, 0);
    life_stamp_pattern(&life, 90, 20, LIFE_STAMP_SQUARE, 0);
    stats = life_step(&life);
    assert(stats.motion >= LIFE_STILL_MOTION_THRESHOLD);
    assert(life.quiet_generations == 0);

    // Each edge launch is one glider moving inward after one full period.
    const int move_x[8] = {1, -1, -1, 1, -1, -1, 1, 1};
    const int move_y[8] = {1, -1, -1, 1, 1, 1, -1, -1};
    for (int launch = 0; launch < 8; ++launch) {
        life_init(&life, first, second, 123);
        life.launches = launch;
        assert(life_inject_edge(&life) == 1);
        int before_count, before_x, before_y;
        live_sums(&life, &before_count, &before_x, &before_y);
        assert(before_count == 5);
        for (int step = 0; step < 4; ++step) {
            stats = life_step(&life);
        }
        int after_count, after_x, after_y;
        live_sums(&life, &after_count, &after_x, &after_y);
        assert(after_count == 5);
        assert(after_x - before_x == 5 * move_x[launch]);
        assert(after_y - before_y == 5 * move_y[launch]);
    }

    life_init(&life, first, second, 123);

    life_seed(&life);
    assert(life.generation == 0);
    int seeded = 0;
    for (int i = 0; i < LIFE_CELLS; ++i) {
        seeded += life.current[i] >= LIFE_ALIVE;
    }
    assert(seeded == 20);
    uint32_t peak_living = 0;
    for (int i = 0; i < 420; ++i) {
        stats = life_step(&life);
        if (stats.living > peak_living) {
            peak_living = stats.living;
        }
        assert(stats.living <= LIFE_CELLS);
        for (int j = 0; j < LIFE_CELLS; ++j) {
            assert(life.current[j] <= LIFE_MAX_AGE);
        }
    }
    uint32_t worst_peak = peak_living;
    for (uint32_t seed = 1; seed <= 32; ++seed) {
        life_init(&life, first, second, seed * 0x9e3779b9U);
        life_seed(&life);
        int seed_count = 0;
        for (int cell = 0; cell < LIFE_CELLS; ++cell) {
            seed_count += life.current[cell] >= LIFE_ALIVE;
        }
        assert(seed_count == 20);
        for (int generation = 0; generation < 420; ++generation) {
            stats = life_step(&life);
            if (stats.living > worst_peak) {
                worst_peak = stats.living;
            }
        }
    }
    // Sparse seeding is checked above; pattern evolution can increase density.
    assert(worst_peak <= LIFE_CELLS);
    uint32_t continuous_peak = 0;
    uint32_t long_run_final = 0;
    for (uint32_t seed = 1; seed <= 3; ++seed) {
        life_init(&life, first, second, seed * 0x9e3779b9U);
        life_seed(&life);
        const int steps = seed == 1 ? 50000 : 5000;
        for (int generation = 0; generation < steps; ++generation) {
            stats = life_step(&life);
            if (life.generation % LIFE_EDGE_INTERVAL == 0 ||
                life.quiet_generations >= LIFE_STILL_GENERATIONS) {
                if (life_inject_edge(&life)) {
                    life.quiet_generations = 0;
                }
            }
            if (stats.living > continuous_peak) {
                continuous_peak = stats.living;
            }
        }
        assert(life.generation == (uint32_t)steps);
        if (seed == 1) {
            long_run_final = stats.living;
        }
    }
    printf("Life rules and inward edge gliders: OK; 420-generation peak %lu, worst seed %lu, "
           "50000-generation peak %lu, final %lu\n",
           (unsigned long)peak_living, (unsigned long)worst_peak, (unsigned long)continuous_peak,
           (unsigned long)long_run_final);
    return 0;
}
