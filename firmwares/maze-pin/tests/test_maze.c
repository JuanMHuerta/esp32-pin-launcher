#include "maze.h"
#include "paint.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void write_ppm(const char *path, const uint16_t *pixels)
{
    FILE *f = fopen(path, "wb"); assert(f);
    fprintf(f, "P6\n%d %d\n255\n", MAZE_W, MAZE_H);
    for (int i = 0; i < MAZE_W * MAZE_H; ++i) {
        uint16_t c = pixels[i];
        fputc(((c >> 11) & 31) * 255 / 31, f);
        fputc(((c >> 5) & 63) * 255 / 63, f);
        fputc((c & 31) * 255 / 31, f);
    }
    fclose(f);
}

static void face_object(maze_t *m)
{
    static const int dx[4] = {1,0,-1,0}, dy[4] = {0,1,0,-1};
    int ox = (int)m->object_x, oy = (int)m->object_y;
    for (int d = 0; d < 4; ++d) {
        int x = ox + dx[d], y = oy + dy[d];
        if (!maze_open(m, x, y)) continue;
        m->x = x + 0.5f; m->y = y + 0.5f;
        m->angle = atan2f(m->object_y - m->y, m->object_x - m->x);
        m->mode = MAZE_WALK;
        return;
    }
    assert(0 && "object has no visible approach");
}

int main(int argc, char **argv)
{
    maze_t a, b;
    maze_init(&a, 12345); maze_init(&b, 12345);
    assert(maze_connected(&a));
    for (uint32_t seed = 1; seed <= 64; ++seed) {
        maze_t sample; maze_init(&sample, seed);
        assert(maze_connected(&sample));
        int junctions = 0;
        for (int y = 1; y < MAZE_GRID - 1; ++y) for (int x = 1; x < MAZE_GRID - 1; ++x)
            if (maze_open(&sample, x, y)) {
                int neighbors = maze_open(&sample, x + 1, y) + maze_open(&sample, x - 1, y) +
                                maze_open(&sample, x, y + 1) + maze_open(&sample, x, y - 1);
                junctions += neighbors >= 3;
            }
        assert(junctions >= 40);
    }
    assert(memcmp(&a, &b, sizeof(a)) == 0);
    assert(a.exit_x != 1 || a.exit_y != 1);
    uint16_t guarded[MAZE_W * MAZE_H + 2];
    guarded[0] = guarded[MAZE_W * MAZE_H + 1] = 0xbeef;
    maze_paint(&a, guarded + 1);
    assert(guarded[0] == 0xbeef && guarded[MAZE_W * MAZE_H + 1] == 0xbeef);
    uint16_t strip[DISPLAY_W * 40 + 2];
    strip[0] = strip[DISPLAY_W * 40 + 1] = 0xbeef;
    assert(maze_expand_strip(guarded + 1, 0, 40, strip + 1));
    assert(!maze_expand_strip(guarded + 1, 1, 40, strip + 1));
    assert(strip[0] == 0xbeef && strip[DISPLAY_W * 40 + 1] == 0xbeef);
    maze_step(&a, 100); maze_step(&b, 100);
    assert(memcmp(&a, &b, sizeof(a)) == 0);
    uint32_t prior = a.generation;
    bool reached_exit = false;
    for (int i = 0; i < 30000 && a.generation == prior; ++i) {
        maze_step(&a, 33); maze_step(&b, 33);
        assert(memcmp(&a, &b, sizeof(a)) == 0);
        assert(maze_open(&a, (int)a.x, (int)a.y));
        assert(a.x > 1.0f && a.x < MAZE_GRID - 1.0f);
        assert(a.y > 1.0f && a.y < MAZE_GRID - 1.0f);
        if (a.mode == MAZE_EXIT) reached_exit = true;
        if (i % 59 == 0) {
            maze_paint(&a, guarded + 1);
            assert(guarded[0] == 0xbeef && guarded[MAZE_W * MAZE_H + 1] == 0xbeef);
        }
    }
    assert(reached_exit && a.generation == prior + 1 && maze_connected(&a));
    assert(a.elapsed_ms < 30000);
    assert(a.object_kind == 0);
    if (argc > 1) {
        maze_init(&a, 12345); maze_step(&a, 1800);
        for (int i = 0; i < 250; ++i) maze_step(&a, 33);
        maze_paint(&a, guarded + 1);
        write_ppm(argv[1], guarded + 1);
        maze_init(&a, 12345);
        for (int y = 2; y < MAZE_GRID - 2; ++y) for (int x = 2; x < MAZE_GRID - 2; ++x) {
            if (!maze_open(&a, x, y)) continue;
            int neighbors = maze_open(&a, x + 1, y) + maze_open(&a, x - 1, y) +
                            maze_open(&a, x, y + 1) + maze_open(&a, x, y - 1);
            if (neighbors < 3 || !maze_open(&a, x, y + 1)) continue;
            a.x = x + 0.5f; a.y = y + 1.5f; a.angle = -1.5707963f;
            a.mode = MAZE_WALK; a.object_kind = 0;
            goto found_junction;
        }
found_junction:
        maze_paint(&a, guarded + 1);
        write_ppm("firmwares/maze-pin/preview-junction.ppm", guarded + 1);
        maze_init(&a, 12345);
        maze_paint(&a, guarded + 1);
        write_ppm("firmwares/maze-pin/preview-title.ppm", guarded + 1);
        assert(a.object_kind == 1);
        face_object(&a);
        maze_paint(&a, guarded + 1);
        write_ppm("firmwares/maze-pin/preview-encounter.ppm", guarded + 1);
        for (int generation = 1; generation < 3; ++generation) {
            a.mode = MAZE_WALK;
            a.cell_x = a.exit_x; a.cell_y = a.exit_y;
            maze_step(&a, 33);
            for (int i = 0; i < 12; ++i) maze_step(&a, 100);
        }
        assert(a.object_kind == 2);
        face_object(&a);
        maze_paint(&a, guarded + 1);
        write_ppm("firmwares/maze-pin/preview-shape.ppm", guarded + 1);
        maze_init(&a, 12345);
        const int dx[4] = {1,0,-1,0}, dy[4] = {0,1,0,-1};
        for (int d = 0; d < 4; ++d) {
            int nx = a.exit_x + dx[d], ny = a.exit_y + dy[d];
            if (!maze_open(&a, nx, ny)) continue;
            a.x = nx + 0.5f; a.y = ny + 0.5f;
            a.angle = atan2f((float)-dy[d], (float)-dx[d]);
            a.mode = MAZE_WALK;
            break;
        }
        maze_paint(&a, guarded + 1);
        write_ppm("firmwares/maze-pin/preview-exit.ppm", guarded + 1);
    }
    puts("maze tests passed");
    return 0;
}
