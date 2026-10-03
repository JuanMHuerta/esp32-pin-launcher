// SPDX-License-Identifier: GPL-3.0-only
#include "maze.h"
#include <math.h>
#include <string.h>

static const int dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};
static const float pi = 3.14159265358979323846f;

static uint32_t random_next(maze_t *m)
{
    uint32_t x = m->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return m->rng = x;
}

bool maze_open(const maze_t *m, int x, int y)
{
    (void)m;
    return x >= 0 && x < MAZE_GRID && y >= 0 && y < MAZE_GRID;
}

bool maze_passage(const maze_t *m, int x, int y, int d)
{
    return d >= 0 && d < 4 && maze_open(m, x, y) && maze_open(m, x + dx[d], y + dy[d]) &&
           !(m->walls[y][x] & (1u << d));
}

/* A perfect maze of thin partitions. Prim gives short branches, T junctions,
 * and dead ends; the right-wall follower visits every cell without a solver. */
static void generate(maze_t *m)
{
    bool visited[MAZE_GRID * MAZE_GRID] = {false};
    uint8_t frontier[MAZE_GRID * MAZE_GRID];
    int count = 1;
    memset(m->walls, 15, sizeof(m->walls));
    frontier[0] = 0;
    visited[0] = true;
    while (count) {
        int chosen = (int)(random_next(m) % (unsigned)count);
        int cell = frontier[chosen], x = cell % MAZE_GRID, y = cell / MAZE_GRID;
        int directions[4], n = 0;
        for (int d = 0; d < 4; ++d) {
            int nx = x + dx[d], ny = y + dy[d];
            if (maze_open(m, nx, ny) && !visited[ny * MAZE_GRID + nx]) {
                directions[n++] = d;
            }
        }
        if (!n) {
            frontier[chosen] = frontier[--count];
            continue;
        }
        int d = directions[random_next(m) % (unsigned)n];
        int nx = x + dx[d], ny = y + dy[d];
        m->walls[y][x] &= ~(1u << d);
        m->walls[ny][nx] &= ~(1u << ((d + 2) & 3));
        visited[ny * MAZE_GRID + nx] = true;
        frontier[count++] = (uint8_t)(ny * MAZE_GRID + nx);
    }
    /* Place the finish far from the entrance. All scratch storage is small;
     * no grid-sized int queues on the ESP32 task stack. */
    uint8_t queue[MAZE_GRID * MAZE_GRID];
    memset(visited, 0, sizeof(visited));
    int head = 0, tail = 1;
    queue[0] = 0;
    visited[0] = true;
    while (head < tail) {
        int cell = queue[head++], x = cell % MAZE_GRID, y = cell / MAZE_GRID;
        m->exit_x = x;
        m->exit_y = y;
        for (int d = 0; d < 4; ++d) {
            int nx = x + dx[d], ny = y + dy[d];
            if (maze_passage(m, x, y, d) && !visited[ny * MAZE_GRID + nx]) {
                visited[ny * MAZE_GRID + nx] = true;
                queue[tail++] = (uint8_t)(ny * MAZE_GRID + nx);
            }
        }
    }
    ++m->generation;
    m->object_kind = m->generation % 4u == 1u ? 1u : m->generation % 4u == 3u ? 2u : 0u;
    int object = queue[tail * 2 / 3];
    m->rat_x = m->rat_next_x = object % MAZE_GRID;
    m->rat_y = m->rat_next_y = object / MAZE_GRID;
    m->rat_heading = 0;
    m->object_x = m->rat_x + 0.5f;
    m->object_y = m->rat_y + 0.5f;
    m->cell_x = m->cell_y = m->next_x = m->next_y = 0;
    m->x = m->y = 0.5f;
    m->heading = maze_passage(m, 0, 0, 0) ? 0 : 1;
    m->angle = m->heading * pi / 2;
}

bool maze_connected(const maze_t *m)
{
    bool seen[MAZE_GRID * MAZE_GRID] = {false};
    uint8_t queue[MAZE_GRID * MAZE_GRID];
    int head = 0, tail = 1;
    queue[0] = 0;
    seen[0] = true;
    while (head < tail) {
        int cell = queue[head++], x = cell % MAZE_GRID, y = cell / MAZE_GRID;
        for (int d = 0; d < 4; ++d) {
            int nx = x + dx[d], ny = y + dy[d];
            if (maze_passage(m, x, y, d) && !seen[ny * MAZE_GRID + nx]) {
                seen[ny * MAZE_GRID + nx] = true;
                queue[tail++] = (uint8_t)(ny * MAZE_GRID + nx);
            }
        }
    }
    return tail == MAZE_GRID * MAZE_GRID;
}

static int right_hand_direction(const maze_t *m, int x, int y, int heading)
{
    const int order[4] = {1, 0, 3, 2};
    for (int i = 0; i < 4; ++i) {
        int d = (heading + order[i]) & 3;
        if (maze_passage(m, x, y, d)) {
            return d;
        }
    }
    return heading;
}

static void move_rat(maze_t *m, float seconds)
{
    if (m->object_kind != 1) {
        return;
    }
    float travel = seconds * 1.6f;
    while (travel > 0) {
        if (m->rat_x == m->rat_next_x && m->rat_y == m->rat_next_y) {
            m->rat_heading = right_hand_direction(m, m->rat_x, m->rat_y, m->rat_heading);
            m->rat_next_x += dx[m->rat_heading];
            m->rat_next_y += dy[m->rat_heading];
        }
        float remaining =
            fabsf(m->rat_next_x + 0.5f - m->object_x) + fabsf(m->rat_next_y + 0.5f - m->object_y);
        float step = fminf(travel, remaining);
        m->object_x += dx[m->rat_heading] * step;
        m->object_y += dy[m->rat_heading] * step;
        travel -= step;
        if (step < remaining) {
            break;
        }
        m->rat_x = m->rat_next_x;
        m->rat_y = m->rat_next_y;
        m->object_x = m->rat_x + 0.5f;
        m->object_y = m->rat_y + 0.5f;
    }
}

void maze_init(maze_t *m, uint32_t seed)
{
    memset(m, 0, sizeof(*m));
    m->rng = seed ? seed : 0x4d415a45u;
    generate(m);
    m->mode = MAZE_TITLE;
}

void maze_step(maze_t *m, uint32_t dt_ms)
{
    if (dt_ms > 100) {
        dt_ms = 100;
    }
    m->elapsed_ms += dt_ms;
    m->mode_ms += dt_ms;
    if (m->mode == MAZE_TITLE) {
        if (m->mode_ms >= 900) {
            m->mode = MAZE_WALK;
            m->mode_ms = 0;
        }
        return;
    }
    if (m->mode == MAZE_EXIT) {
        if (m->mode_ms >= 650) {
            generate(m);
            m->mode = MAZE_WALK;
            m->mode_ms = 0;
        }
        return;
    }
    float seconds = dt_ms * 0.001f;
    move_rat(m, seconds);
    /* Consume leftover time at cell/turn boundaries: constant speed, no pause
     * every tile, and the same trajectory at different display frame rates. */
    while (seconds > 0.000001f) {
        if (m->cell_x == m->exit_x && m->cell_y == m->exit_y) {
            m->mode = MAZE_EXIT;
            m->mode_ms = 0;
            return;
        }
        if (m->next_x == m->cell_x && m->next_y == m->cell_y) {
            m->heading = right_hand_direction(m, m->cell_x, m->cell_y, m->heading);
            m->next_x = m->cell_x + dx[m->heading];
            m->next_y = m->cell_y + dy[m->heading];
        }
        float target = m->heading * pi / 2;
        float diff = target - m->angle;
        while (diff > pi) {
            diff -= 2 * pi;
        }
        while (diff < -pi) {
            diff += 2 * pi;
        }
        /* Dead ends always turn clockwise, matching the wall-following rule. */
        if (fabsf(fabsf(diff) - pi) < 0.001f) {
            diff = pi;
        }
        float turn_time = fabsf(diff) / 6.5f;
        if (turn_time > seconds) {
            m->angle += copysignf(6.5f * seconds, diff);
            break;
        }
        m->angle = target;
        seconds -= turn_time;
        float remaining = fabsf(m->next_x + 0.5f - m->x) + fabsf(m->next_y + 0.5f - m->y);
        float step = fminf(3.0f * seconds, remaining);
        m->x += dx[m->heading] * step;
        m->y += dy[m->heading] * step;
        seconds -= step / 3.0f;
        if (step < remaining) {
            break;
        }
        m->cell_x = m->next_x;
        m->cell_y = m->next_y;
        m->x = m->cell_x + 0.5f;
        m->y = m->cell_y + 0.5f;
    }
    if (m->object_kind == 2 && fabsf(m->x - m->object_x) + fabsf(m->y - m->object_y) < 0.32f) {
        m->object_kind = 0;
    }
}
