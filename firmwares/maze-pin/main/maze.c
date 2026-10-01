#include "maze.h"
#include <math.h>
#include <string.h>

static uint32_t random_next(maze_t *m)
{
    uint32_t x = m->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return m->rng = x;
}

bool maze_open(const maze_t *m, int x, int y)
{ return x >= 0 && x < MAZE_GRID && y >= 0 && y < MAZE_GRID && m->cells[y][x] != 0; }

static void add_frontier(const maze_t *m, uint16_t *edges, int *count, int x, int y)
{
    static const int dx[4] = {0, 2, 0, -2}, dy[4] = {-2, 0, 2, 0};
    for (int d = 0; d < 4; ++d) {
        int nx = x + dx[d], ny = y + dy[d];
        if (nx > 0 && nx < MAZE_GRID - 1 && ny > 0 && ny < MAZE_GRID - 1 && !m->cells[ny][nx])
            edges[(*count)++] = (uint16_t)(x | (y << 5) | (d << 10));
    }
}

/* Randomized Prim makes short corridors and many branches. A few extra
 * openings turn some branches into visible intersections and loops. */
static void generate(maze_t *m)
{
    static const int dx[4] = {0, 2, 0, -2}, dy[4] = {-2, 0, 2, 0};
    static uint16_t edges[4 * 121];
    int count = 0;
    memset(m->cells, 0, sizeof(m->cells));
    m->cells[1][1] = 1;
    add_frontier(m, edges, &count, 1, 1);
    while (count) {
        int chosen = (int)(random_next(m) % (unsigned)count);
        uint16_t edge = edges[chosen];
        edges[chosen] = edges[--count];
        int x = edge & 31, y = (edge >> 5) & 31, d = edge >> 10;
        int nx = x + dx[d], ny = y + dy[d];
        if (m->cells[ny][nx]) continue;
        m->cells[y + dy[d] / 2][x + dx[d] / 2] = 1;
        m->cells[ny][nx] = 1;
        add_frontier(m, edges, &count, nx, ny);
    }
    for (int y = 1; y < MAZE_GRID - 1; y += 2) for (int x = 1; x < MAZE_GRID - 1; x += 2) {
        if (x + 2 < MAZE_GRID - 1 && !m->cells[y][x + 1] && random_next(m) % 100u < 34u)
            m->cells[y][x + 1] = 1;
        if (y + 2 < MAZE_GRID - 1 && !m->cells[y + 1][x] && random_next(m) % 100u < 34u)
            m->cells[y + 1][x] = 1;
    }
    /* Select a distant destination with turns on its shortest route. */
    static int qx[MAZE_GRID * MAZE_GRID], qy[MAZE_GRID * MAZE_GRID];
    static int16_t dist[MAZE_GRID][MAZE_GRID];
    static uint8_t turns[MAZE_GRID][MAZE_GRID];
    static int8_t parent[MAZE_GRID][MAZE_GRID];
    memset(dist, 0xff, sizeof(dist));
    memset(turns, 0, sizeof(turns));
    memset(parent, -1, sizeof(parent));
    int head = 0, tail = 1;
    qx[0] = qy[0] = 1; dist[1][1] = 0;
    m->exit_x = m->exit_y = 1;
    int best = -1;
    while (head < tail) {
        int x = qx[head], y = qy[head++];
        int score = dist[y][x] * 3 + turns[y][x] * 2;
        if ((x & 1) && (y & 1) && score > best) { best = score; m->exit_x = x; m->exit_y = y; }
        for (int d = 0; d < 4; ++d) {
            int nx = x + dx[d] / 2, ny = y + dy[d] / 2;
            if (maze_open(m, nx, ny) && dist[ny][nx] < 0) {
                dist[ny][nx] = dist[y][x] + 1;
                parent[ny][nx] = (int8_t)d;
                turns[ny][nx] = turns[y][x] + (parent[y][x] >= 0 && parent[y][x] != d);
                qx[tail] = nx; qy[tail++] = ny;
            }
        }
    }
    ++m->generation;
    m->object_kind = m->generation % 4u == 1u ? 1u : m->generation % 4u == 3u ? 2u : 0u;
    if (m->object_kind) {
        static int path_x[MAZE_GRID * MAZE_GRID], path_y[MAZE_GRID * MAZE_GRID];
        int length = 0;
        int x = m->exit_x, y = m->exit_y;
        while (x != 1 || y != 1) {
            path_x[length] = x; path_y[length++] = y;
            int d = parent[y][x];
            x -= dx[d] / 2; y -= dy[d] / 2;
        }
        if (length > 8) {
            int index = length * (m->object_kind == 1 ? 2 : 1) / 3;
            int ox = path_x[index], oy = path_y[index];
            int from_x = path_x[index + 1], from_y = path_y[index + 1];
            m->object_x = ox + 0.5f - (oy - from_y) * 0.22f;
            m->object_y = oy + 0.5f + (ox - from_x) * 0.22f;
        } else m->object_kind = 0;
    }
    m->cell_x = m->cell_y = 1;
    m->next_x = m->next_y = 1;
    m->x = m->y = 1.5f;
    m->angle = 0;
}

bool maze_connected(const maze_t *m)
{
    bool seen[MAZE_GRID][MAZE_GRID] = {{false}};
    int qx[MAZE_GRID * MAZE_GRID], qy[MAZE_GRID * MAZE_GRID], head = 0, tail = 1;
    qx[0] = qy[0] = 1; seen[1][1] = true;
    while (head < tail) {
        int x = qx[head], y = qy[head++];
        const int dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};
        for (int d = 0; d < 4; ++d) {
            int nx = x + dx[d], ny = y + dy[d];
            if (maze_open(m, nx, ny) && !seen[ny][nx]) {
                seen[ny][nx] = true; qx[tail] = nx; qy[tail++] = ny;
            }
        }
    }
    for (int y = 0; y < MAZE_GRID; ++y) for (int x = 0; x < MAZE_GRID; ++x)
        if (maze_open(m, x, y) && !seen[y][x]) return false;
    return seen[m->exit_y][m->exit_x];
}

/* Breadth-first search still gives an exit route when extra openings make loops. */
static void choose_next(maze_t *m)
{
    static int qx[MAZE_GRID * MAZE_GRID], qy[MAZE_GRID * MAZE_GRID];
    static int8_t parent[MAZE_GRID][MAZE_GRID];
    static const int dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};
    memset(parent, -1, sizeof(parent));
    int head = 0, tail = 1;
    qx[0] = m->cell_x; qy[0] = m->cell_y;
    parent[m->cell_y][m->cell_x] = 4;
    while (head < tail && parent[m->exit_y][m->exit_x] < 0) {
        int x = qx[head], y = qy[head++];
        for (int d = 0; d < 4; ++d) {
            int nx = x + dx[d], ny = y + dy[d];
            if (maze_open(m, nx, ny) && parent[ny][nx] < 0) {
                parent[ny][nx] = (int8_t)d; qx[tail] = nx; qy[tail++] = ny;
            }
        }
    }
    int x = m->exit_x, y = m->exit_y;
    while (parent[y][x] >= 0 && parent[y][x] != 4) {
        int d = parent[y][x], px = x - dx[d], py = y - dy[d];
        if (px == m->cell_x && py == m->cell_y) { m->next_x = x; m->next_y = y; return; }
        x = px; y = py;
    }
    m->next_x = m->cell_x; m->next_y = m->cell_y;
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
    if (dt_ms > 100) dt_ms = 100;
    m->elapsed_ms += dt_ms;
    m->mode_ms += dt_ms;
    if (m->mode == MAZE_TITLE) {
        if (m->mode_ms >= 900) { m->mode = MAZE_WALK; m->mode_ms = 0; }
        return;
    }
    if (m->mode == MAZE_EXIT) {
        if (m->mode_ms >= 600) { generate(m); m->mode = MAZE_WALK; m->mode_ms = 0; }
        return;
    }
    if (m->cell_x == m->exit_x && m->cell_y == m->exit_y) {
        m->mode = MAZE_EXIT; m->mode_ms = 0; return;
    }
    if (m->next_x == m->cell_x && m->next_y == m->cell_y) choose_next(m);
    float target = atan2f((float)(m->next_y - m->cell_y), (float)(m->next_x - m->cell_x));
    float diff = atan2f(sinf(target - m->angle), cosf(target - m->angle));
    float turn = 5.4f * dt_ms / 1000.0f;
    if (fabsf(diff) > turn) { m->angle += diff > 0 ? turn : -turn; return; }
    m->angle = target;
    float tx = m->next_x + 0.5f, ty = m->next_y + 0.5f;
    float remaining = hypotf(tx - m->x, ty - m->y);
    float travel = 3.0f * dt_ms / 1000.0f;
    if (travel >= remaining) {
        m->x = tx; m->y = ty; m->cell_x = m->next_x; m->cell_y = m->next_y;
    } else {
        m->x += cosf(target) * travel; m->y += sinf(target) * travel;
    }
}
