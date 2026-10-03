// SPDX-License-Identifier: GPL-3.0-only
#include "maze.h"
#include "paint.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const int dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};
static uint16_t guarded[MAZE_W * MAZE_H + 2];

static void write_ppm(const char *path, const uint16_t *pixels)
{
    FILE *f = fopen(path, "wb");
    assert(f);
    fprintf(f, "P6\n%d %d\n255\n", MAZE_W, MAZE_H);
    for (int i = 0; i < MAZE_W * MAZE_H; ++i) {
        uint16_t c = pixels[i];
        fputc(((c >> 11) & 31) * 255 / 31, f);
        fputc(((c >> 5) & 63) * 255 / 63, f);
        fputc((c & 31) * 255 / 31, f);
    }
    fclose(f);
}

static void paint_checked(const maze_t *m)
{
    guarded[0] = guarded[MAZE_W * MAZE_H + 1] = 0xbeef;
    maze_paint(m, guarded + 1);
    assert(guarded[0] == 0xbeef && guarded[MAZE_W * MAZE_H + 1] == 0xbeef);
}

static void check_crossing(const maze_t *m, float x, float y, float nx, float ny)
{
    assert(maze_open(m, (int)nx, (int)ny));
    int cx = (int)x, cy = (int)y, tx = (int)nx, ty = (int)ny;
    if (cx == tx && cy == ty) {
        return;
    }
    assert((cx == tx) != (cy == ty));
    int d = tx > cx ? 0 : ty > cy ? 1 : tx < cx ? 2 : 3;
    assert(maze_passage(m, cx, cy, d));
}

static void face_object(maze_t *m)
{
    int ox = (int)m->object_x, oy = (int)m->object_y;
    for (int d = 0; d < 4; ++d) {
        if (!maze_passage(m, ox, oy, d)) {
            continue;
        }
        m->x = ox + dx[d] + .5f;
        m->y = oy + dy[d] + .5f;
        m->angle = atan2f(m->object_y - m->y, m->object_x - m->x);
        m->mode = MAZE_WALK;
        m->mode_ms = 1000;
        return;
    }
    assert(0 && "object has no visible approach");
}

int main(int argc, char **argv)
{
    maze_t a, b;
    /* Reciprocal walls, a connected tree, and useful branch density across
     * seeds. A tree guarantees the right-wall follower eventually exits. */
    for (uint32_t seed = 1; seed <= 64; ++seed) {
        maze_init(&a, seed);
        maze_init(&b, seed);
        assert(maze_connected(&a));
        int junctions = 0, passages = 0, turns = 0, reversals = 0;
        for (int y = 0; y < MAZE_GRID; ++y) {
            for (int x = 0; x < MAZE_GRID; ++x) {
                int neighbors = 0;
                for (int d = 0; d < 4; ++d) {
                    bool open = maze_passage(&a, x, y, d);
                    if (maze_open(&a, x + dx[d], y + dy[d])) {
                        assert(open == maze_passage(&a, x + dx[d], y + dy[d], (d + 2) & 3));
                    } else {
                        assert(a.walls[y][x] & (1u << d));
                    }
                    neighbors += open;
                }
                junctions += neighbors >= 3;
                passages += neighbors;
            }
        }
        assert(junctions >= 15 && passages == 2 * (MAZE_GRID * MAZE_GRID - 1));
        bool exit_seen = false;
        unsigned steps = 0;
        while (a.generation == 1 && steps++ < 7000) {
            maze_t before = a;
            maze_step(&a, 25);
            maze_step(&b, 25);
            assert(memcmp(&a, &b, sizeof(a)) == 0);
            if (a.generation != before.generation) {
                break;
            }
            check_crossing(&a, before.x, before.y, a.x, a.y);
            if (a.object_kind == 1) {
                check_crossing(&a, before.object_x, before.object_y, a.object_x, a.object_y);
            }
            int change = (a.heading - before.heading + 4) & 3;
            turns += change != 0;
            reversals += change == 2;
            exit_seen |= a.mode == MAZE_EXIT;
            if (seed <= 4 && steps % 37 == 0) {
                paint_checked(&a);
            }
        }
        assert(exit_seen && a.generation == 2 && maze_connected(&a));
        assert(turns >= 10 && reversals >= 1 && a.object_kind == 0);
    }
    /* Timing must not introduce a per-cell pause at a different frame rate. */
    maze_init(&a, 12345);
    a.mode = MAZE_WALK;
    a.mode_ms = 1000;
    b = a;
    for (int i = 0; i < 1000; ++i) {
        maze_step(&a, 10);
    }
    for (int i = 0; i < 400; ++i) {
        maze_step(&b, 25);
    }
    assert(a.cell_x == b.cell_x && a.cell_y == b.cell_y);
    assert(fabsf(a.x - b.x) < .002f && fabsf(a.y - b.y) < .002f &&
           fabsf(a.angle - b.angle) < .002f);
    /* Thin-wall occlusion: a hidden rat must leave the image untouched. */
    maze_init(&a, 12345);
    a.mode = MAZE_WALK;
    a.mode_ms = 1000;
    bool hidden_tested = false;
    static uint16_t baseline[MAZE_W * MAZE_H];
    for (int y = 0; y < MAZE_GRID && !hidden_tested; ++y) {
        for (int x = 0; x < MAZE_GRID - 1; ++x) {
            if (maze_passage(&a, x, y, 0)) {
                continue;
            }
            a.x = x + .5f;
            a.y = y + .5f;
            a.angle = 0;
            a.object_x = x + 1.5f;
            a.object_y = y + .5f;
            a.object_kind = 0;
            maze_paint(&a, baseline);
            a.object_kind = 1;
            paint_checked(&a);
            assert(memcmp(baseline, guarded + 1, sizeof(baseline)) == 0);
            a.object_kind = 2;
            paint_checked(&a);
            assert(memcmp(baseline, guarded + 1, sizeof(baseline)) == 0);
            hidden_tested = true;
            break;
        }
    }
    assert(hidden_tested);
    uint16_t strip[DISPLAY_W * 40 + 2];
    strip[0] = strip[DISPLAY_W * 40 + 1] = 0xbeef;
    assert(maze_expand_strip(guarded + 1, 0, 40, strip + 1));
    assert(!maze_expand_strip(guarded + 1, 1, 40, strip + 1));
    assert(!maze_expand_strip(guarded + 1, 220, 40, strip + 1));
    assert(strip[0] == 0xbeef && strip[DISPLAY_W * 40 + 1] == 0xbeef);
    if (argc > 1) {
        maze_init(&a, 12345);
        for (int i = 0; i < 330; ++i) {
            maze_step(&a, 25);
        }
        paint_checked(&a);
        write_ppm(argv[1], guarded + 1);
        maze_init(&a, 12345);
        for (int y = 1; y < MAZE_GRID - 1; ++y) {
            for (int x = 1; x < MAZE_GRID - 1; ++x) {
                int neighbors = 0;
                for (int d = 0; d < 4; ++d) {
                    neighbors += maze_passage(&a, x, y, d);
                }
                if (neighbors < 3 || !maze_passage(&a, x, y, 1)) {
                    continue;
                }
                a.x = x + .5f;
                a.y = y + 1.5f;
                a.angle = -1.5707963f;
                a.mode = MAZE_WALK;
                a.mode_ms = 1000;
                a.object_kind = 0;
                goto found_junction;
            }
        }
    found_junction:
        paint_checked(&a);
        write_ppm("firmwares/maze-pin/preview-junction.ppm", guarded + 1);
        maze_init(&a, 12345);
        paint_checked(&a);
        write_ppm("firmwares/maze-pin/preview-title.ppm", guarded + 1);
        face_object(&a);
        paint_checked(&a);
        write_ppm("firmwares/maze-pin/preview-encounter.ppm", guarded + 1);
        a.object_kind = 2;
        a.elapsed_ms = 375;
        paint_checked(&a);
        write_ppm("firmwares/maze-pin/preview-shape.ppm", guarded + 1);
        a.object_x = a.exit_x + .5f;
        a.object_y = a.exit_y + .5f;
        a.object_kind = 0;
        face_object(&a);
        paint_checked(&a);
        write_ppm("firmwares/maze-pin/preview-exit.ppm", guarded + 1);
    }
    if (argc > 2) {
        maze_init(&a, 12345);
        for (int i = 0; i < 600; ++i) {
            char path[512];
            snprintf(path, sizeof(path), "%s/%04d.ppm", argv[2], i);
            paint_checked(&a);
            write_ppm(path, guarded + 1);
            maze_step(&a, 25);
            maze_step(&a, 25);
        }
    }
    puts("maze tests passed: 64 seeds, wall-following exits, collision, timing, occlusion and "
         "bounds");
    return 0;
}
