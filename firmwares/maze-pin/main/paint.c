// SPDX-License-Identifier: GPL-3.0-only
#include "paint.h"
#include <math.h>
#include <string.h>

static uint16_t rgb(int r, int g, int b)
{
    return (uint16_t)(((r & 248) << 8) | ((g & 252) << 3) | (b >> 3));
}
static int clamp(int n, int lo, int hi)
{
    return n < lo ? lo : n > hi ? hi : n;
}
static uint32_t hash(int x, int y)
{
    uint32_t n = (uint32_t)x * 0x45d9f3bu ^ (uint32_t)y * 0x119de1f3u;
    n ^= n >> 16;
    n *= 0x45d9f3bu;
    return n ^ (n >> 16);
}

static uint16_t wall_texture[64 * 64], floor_texture[64 * 64], ceiling_texture[64 * 64];
static bool textures_ready;

/* Redrawn textures: red clay and pale mortar, continuous wood grain, and
 * the small stippled ceiling tiles of the original. No distance fog. */
static void init_textures(void)
{
    if (textures_ready) {
        return;
    }
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            int i = y * 64 + x;
            int bx = (x + ((y / 16) & 1) * 16) & 31, by = y & 15;
            int noise = (int)(hash(x, y) & 31) - 16;
            int brick = (int)(hash((x + ((y / 16) & 1) * 16) / 32, y / 16) & 31);
            bool mortar = by < 2 || bx < 2;
            int bevel = by == 2 || bx == 2 ? 22 : by == 15 || bx == 31 ? -28 : 0;
            wall_texture[i] = mortar ? rgb(190 + noise, 182 + noise, 171 + noise)
                                     : rgb(clamp(153 + brick + noise + bevel, 0, 255),
                                           clamp(15 + noise / 3 + bevel / 3, 0, 255),
                                           clamp(9 + noise / 4 + bevel / 4, 0, 255));

            /* Periodic wandering grain rather than wide horizontal floorboards. */
            float bend =
                2.6f * sinf(x * 0.09817477f) + 1.3f * sinf(x * 0.29452431f + y * 0.19634954f);
            float grain = sinf(y * 1.57079633f + bend);
            int wood = (int)(grain * 15) + (int)(hash(x, y) & 7);
            floor_texture[i] = rgb(168 + wood, 113 + wood, 48 + wood / 2);

            int tx = x & 15, ty = y & 15;
            int speckle = (int)(hash(x, y) & 63);
            int stone = 145 + speckle;
            if (!tx || !ty) {
                stone = 235;
            } else if (tx == 15 || ty == 15) {
                stone = 100;
            }
            ceiling_texture[i] = rgb(stone, stone, stone);
        }
    }
    textures_ready = true;
}

static uint16_t shade_wall(uint16_t c, int shade)
{
    int r = ((c >> 11) & 31) * shade >> 8;
    int g = ((c >> 5) & 63) * shade >> 8;
    int b = (c & 31) * shade >> 8;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

/* One focal length for both axes keeps bricks, spheres and floor cells in
 * proportion on the wide display. The eye is below the wall midpoint. */
#define FOCAL 100.0f
#define EYE 0.36f

static void rect(uint16_t *p, int x, int y, int w, int h, uint16_t c)
{
    int l = clamp(x, 0, MAZE_W), t = clamp(y, 0, MAZE_H);
    int r = clamp(x + w, 0, MAZE_W), b = clamp(y + h, 0, MAZE_H);
    for (int yy = t; yy < b; ++yy) {
        for (int xx = l; xx < r; ++xx) {
            p[yy * MAZE_W + xx] = c;
        }
    }
}

static const uint8_t *glyph(char ch)
{
    static const uint8_t blank[7] = {0};
    static const uint8_t alpha[26][7] = {
        {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30}, {14, 17, 16, 16, 16, 17, 14},
        {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
        {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17}, {14, 4, 4, 4, 4, 4, 14},
        {1, 1, 1, 1, 17, 17, 14},     {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
        {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17}, {14, 17, 17, 17, 17, 17, 14},
        {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
        {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},       {17, 17, 17, 17, 17, 17, 14},
        {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
        {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31}};
    static const uint8_t digits[10][7] = {{14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},
                                          {14, 17, 1, 2, 4, 8, 31},     {30, 1, 1, 14, 1, 1, 30},
                                          {2, 6, 10, 18, 31, 2, 2},     {31, 16, 16, 30, 1, 1, 30},
                                          {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},
                                          {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14}};
    if (ch >= 'A' && ch <= 'Z') {
        return alpha[ch - 'A'];
    }
    if (ch >= '0' && ch <= '9') {
        return digits[ch - '0'];
    }
    return blank;
}

static void label(uint16_t *p, int x, int y, const char *s, int scale, uint16_t c)
{
    for (; *s; ++s, x += 6 * scale) {
        const uint8_t *g = glyph(*s);
        for (int gy = 0; gy < 7; ++gy) {
            for (int gx = 0; gx < 5; ++gx) {
                if (g[gy] & (1u << (4 - gx))) {
                    rect(p, x + gx * scale, y + gy * scale, scale, scale, c);
                }
            }
        }
    }
}

/* Billboards use world coordinates, floor contact and wall occlusion. */
static void sprite(const maze_t *m, uint16_t *p, const float *depth, float wx, float wy, int kind)
{
    float ca = cosf(m->angle), sa = sinf(m->angle);
    float dx = wx - m->x, dy = wy - m->y;
    float forward = dx * ca + dy * sa, sideways = -dx * sa + dy * ca;
    if (forward < 0.12f) {
        return;
    }
    float scale = FOCAL / forward;
    float center_z = kind == 0 ? 0.40f : 0.10f;
    int cx = (int)(MAZE_W / 2 + sideways * scale);
    int cy = (int)(MAZE_H / 2 + (EYE - center_z) * scale);
    int width = clamp((int)((kind == 0 ? 0.58f : 0.52f) * scale), 1, MAZE_W * 4);
    int height = clamp((int)((kind == 0 ? 0.58f : 0.20f) * scale), 1, MAZE_H * 4);
    int left = cx - width / 2, top = cy - height / 2;
    if (left >= MAZE_W || left + width < 0 || top >= MAZE_H || top + height < 0) {
        return;
    }
    float inv_width = 2.0f / width, inv_height = 2.0f / height;
    static const float dir_x[4] = {1, 0, -1, 0}, dir_y[4] = {0, 1, 0, -1};
    bool flip = -dir_x[m->rat_heading] * sa + dir_y[m->rat_heading] * ca < 0;
    int gait = (m->elapsed_ms / 75) & 1;
    for (int y = clamp(top, 0, MAZE_H); y < clamp(top + height, 0, MAZE_H); ++y) {
        for (int x = clamp(left, 0, MAZE_W); x < clamp(left + width, 0, MAZE_W); ++x) {
            if (forward > depth[x]) {
                continue;
            }
            float u = (x - cx) * inv_width, v = (y - cy) * inv_height;
            uint16_t color = 0;
            if (kind == 0) { /* Yellow smiley with blue eyes and smile. */
                float radius = u * u + v * v;
                if (radius < 0.94f) {
                    int light = clamp((int)(250 - 80 * radius - 28 * (u + v)), 115, 255);
                    color = rgb(light, light, 0);
                    if ((u - 0.36f) * (u - 0.36f) + (v + 0.28f) * (v + 0.28f) < 0.014f ||
                        (u + 0.36f) * (u + 0.36f) + (v + 0.28f) * (v + 0.28f) < 0.014f ||
                        (fabsf(u) < 0.57f && fabsf(v - (0.47f - u * u * 0.72f)) < 0.085f)) {
                        color = rgb(20, 30, 230);
                    }
                }
            } else {
                if (flip) {
                    u = -u;
                }
                int fur = (int)(hash((int)((u + 1) * 40), (int)((v + 1) * 18)) & 31);
                /* Tail, arched brown body, pointed snout, ear, and running feet. */
                if (u < -0.40f && fabsf(v - (0.60f + 0.12f * sinf(u * 8))) < 0.055f) {
                    color = rgb(157, 119, 83);
                }
                if ((u + 0.12f) * (u + 0.12f) / 0.37f + (v + 0.06f) * (v + 0.06f) / 0.53f < 1) {
                    color = rgb(91 + fur, 65 + fur, 37 + fur / 2);
                }
                if (u > 0.24f && u < 0.90f && fabsf(v - 0.09f) < (0.94f - u) * 0.65f) {
                    color = rgb(119 + fur, 85 + fur, 51 + fur / 2);
                }
                if ((u - 0.36f) * (u - 0.36f) / 0.018f + (v + 0.43f) * (v + 0.43f) / 0.09f < 1) {
                    color = rgb(167, 120, 83);
                }
                if (fabsf(u - 0.57f) < 0.035f && fabsf(v + 0.10f) < 0.07f) {
                    color = rgb(12, 9, 6);
                }
                if (v > 0.58f && v < 0.91f &&
                    (fabsf(u + 0.24f + gait * 0.11f) < 0.10f ||
                     fabsf(u - 0.38f + gait * 0.09f) < 0.09f)) {
                    color = rgb(164, 126, 92);
                }
            }
            if (color) {
                p[y * MAZE_W + x] = color;
            }
        }
    }
}

typedef struct {
    float x, y, z;
} vertex_t;

static float edge(vertex_t a, vertex_t b, float x, float y)
{
    return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
}

/* Eight real triangles, rotating in world space, rather than a wobbling icon.
 * Per-pixel reciprocal depth clips faces correctly against maze partitions. */
static void polyhedron(const maze_t *m, uint16_t *p, const float *depth)
{
    float ca = cosf(m->angle), sa = sinf(m->angle);
    float spin = m->elapsed_ms * 0.0018f, cs = cosf(spin), ss = sinf(spin);
    const vertex_t local[6] = {{0, 0, .22f}, {0, 0, -.22f}, {.19f, 0, 0},
                               {0, .19f, 0}, {-.19f, 0, 0}, {0, -.19f, 0}};
    static const uint8_t faces[8][3] = {{0, 2, 3}, {0, 3, 4}, {0, 4, 5}, {0, 5, 2},
                                        {1, 3, 2}, {1, 4, 3}, {1, 5, 4}, {1, 2, 5}};
    vertex_t v[6];
    float center_depth = (m->object_x - m->x) * ca + (m->object_y - m->y) * sa;
    if (center_depth < 0.32f) {
        return;
    }
    for (int i = 0; i < 6; ++i) {
        float wx = m->object_x + local[i].x * cs - local[i].y * ss - m->x;
        float wy = m->object_y + local[i].x * ss + local[i].y * cs - m->y;
        float z = wx * ca + wy * sa;
        v[i].x = MAZE_W / 2 + (-wx * sa + wy * ca) * FOCAL / z;
        v[i].y = MAZE_H / 2 + (EYE - (0.48f + local[i].z)) * FOCAL / z;
        v[i].z = 1.0f / z;
    }
    for (int f = 0; f < 8; ++f) {
        vertex_t a = v[faces[f][0]], b = v[faces[f][1]], c = v[faces[f][2]];
        float area = edge(a, b, c.x, c.y);
        if (area <= 0.001f) {
            continue;
        }
        int left = clamp((int)floorf(fminf(a.x, fminf(b.x, c.x))), 0, MAZE_W);
        int right = clamp((int)ceilf(fmaxf(a.x, fmaxf(b.x, c.x))), 0, MAZE_W);
        int top = clamp((int)floorf(fminf(a.y, fminf(b.y, c.y))), 0, MAZE_H);
        int bottom = clamp((int)ceilf(fmaxf(a.y, fmaxf(b.y, c.y))), 0, MAZE_H);
        int gray = 95 + (f % 4) * 37 + (f < 4 ? 18 : 0);
        uint16_t color = rgb(gray, gray, gray);
        float inverse = 1.0f / area;
        for (int y = top; y < bottom; ++y) {
            for (int x = left; x < right; ++x) {
                float wa = edge(b, c, x + .5f, y + .5f) * inverse;
                float wb = edge(c, a, x + .5f, y + .5f) * inverse;
                float wc = 1 - wa - wb;
                if (wa >= 0 && wb >= 0 && wc >= 0 &&
                    wa * a.z + wb * b.z + wc * c.z > 1.0f / depth[x]) {
                    p[y * MAZE_W + x] = color;
                }
            }
        }
    }
}

void maze_paint(const maze_t *m, uint16_t p[MAZE_W * MAZE_H])
{
    init_textures();
    float depth[MAZE_W];
    float ca = cosf(m->angle), sa = sinf(m->angle);
    const float half_view = MAZE_W / (2 * FOCAL);
    for (int y = 0; y < MAZE_H; ++y) {
        float row = y + 0.5f - MAZE_H / 2;
        float distance = FOCAL * (row < 0 ? 1 - EYE : EYE) / fabsf(row);
        float left_x = m->x + (ca + sa * half_view) * distance;
        float left_y = m->y + (sa - ca * half_view) * distance;
        int32_t fx = (int32_t)(left_x * 65536.0f), fy = (int32_t)(left_y * 65536.0f);
        int32_t step_x = (int32_t)(-sa * distance * 65536.0f / FOCAL);
        int32_t step_y = (int32_t)(ca * distance * 65536.0f / FOCAL);
        const uint16_t *texture = row < 0 ? ceiling_texture : floor_texture;
        uint16_t *dest = p + y * MAZE_W;
        for (int x = 0; x < MAZE_W; ++x) {
            dest[x] = texture[((fy >> 10) & 63) * 64 + ((fx >> 10) & 63)];
            fx += step_x;
            fy += step_y;
        }
    }
    float wall_height = m->mode == MAZE_EXIT ? fmaxf(0, 1 - m->mode_ms / 650.0f) : 1;
    if (m->mode == MAZE_WALK && m->mode_ms < 400) {
        wall_height = m->mode_ms / 400.0f;
    }
    for (int x = 0; x < MAZE_W; ++x) {
        float camera = (x + 0.5f - MAZE_W / 2) / FOCAL;
        float rx = ca - sa * camera, ry = sa + ca * camera;
        int map_x = (int)m->x, map_y = (int)m->y;
        float delta_x = fabsf(1.0f / rx), delta_y = fabsf(1.0f / ry);
        int step_x = rx < 0 ? -1 : 1, step_y = ry < 0 ? -1 : 1;
        float sx = (rx < 0 ? m->x - map_x : map_x + 1 - m->x) * delta_x;
        float sy = (ry < 0 ? m->y - map_y : map_y + 1 - m->y) * delta_y;
        int side = 0;
        float distance = 0;
        /* Intersect an edge before entering the next cell: walls have no
         * block-sized thickness and every maze cell is a usable room. */
        for (int count = 0; count < MAZE_GRID * 2; ++count) {
            if (sx < sy) {
                side = 0;
                distance = sx;
                if (!maze_passage(m, map_x, map_y, step_x > 0 ? 0 : 2)) {
                    break;
                }
                sx += delta_x;
                map_x += step_x;
            } else {
                side = 1;
                distance = sy;
                if (!maze_passage(m, map_x, map_y, step_y > 0 ? 1 : 3)) {
                    break;
                }
                sy += delta_y;
                map_y += step_y;
            }
        }
        distance = fmaxf(distance, 0.025f);
        depth[x] = distance;
        float scale = FOCAL / distance;
        float wall_top = MAZE_H / 2 - (wall_height - EYE) * scale;
        int top = clamp((int)ceilf(wall_top - .5f), 0, MAZE_H);
        int bottom = clamp((int)ceilf(MAZE_H / 2 + EYE * scale - .5f), 0, MAZE_H);
        float hit = side ? m->x + distance * rx : m->y + distance * ry;
        int texture_x = ((int)(hit * 64.0f)) & 63;
        int texture_step = (int)(65536.0f * 64 / scale);
        int texture_y = (int)((top + .5f - wall_top) * texture_step);
        int shade = side ? 230 : 256;
        for (int y = top; y < bottom; ++y) {
            p[y * MAZE_W + x] =
                shade_wall(wall_texture[((texture_y >> 16) & 63) * 64 + texture_x], shade);
            texture_y += texture_step;
        }
    }
    if (m->mode != MAZE_EXIT) {
        /* Draw the farther billboard first if both share a line of sight. */
        float exit_depth = (m->exit_x + .5f - m->x) * ca + (m->exit_y + .5f - m->y) * sa;
        float object_depth = (m->object_x - m->x) * ca + (m->object_y - m->y) * sa;
        if (exit_depth > object_depth) {
            sprite(m, p, depth, m->exit_x + .5f, m->exit_y + .5f, 0);
        }
        if (m->mode == MAZE_WALK && m->object_kind == 1) {
            sprite(m, p, depth, m->object_x, m->object_y, 1);
        }
        if (m->mode == MAZE_WALK && m->object_kind == 2) {
            polyhedron(m, p, depth);
        }
        if (exit_depth <= object_depth) {
            sprite(m, p, depth, m->exit_x + .5f, m->exit_y + .5f, 0);
        }
    }
    if (m->mode == MAZE_TITLE) {
        rect(p, 57, 36, 154, 47, rgb(192, 192, 192));
        rect(p, 57, 36, 154, 1, rgb(255, 255, 255));
        rect(p, 57, 36, 1, 47, rgb(255, 255, 255));
        rect(p, 58, 82, 153, 1, rgb(64, 64, 64));
        rect(p, 210, 37, 1, 46, rgb(64, 64, 64));
        rect(p, 60, 39, 148, 12, rgb(0, 0, 128));
        label(p, 64, 42, "3D MAZE", 1, rgb(255, 255, 255));
        label(p, 75, 59, "3D MAZE", 3, rgb(0, 0, 0));
    }
}

bool maze_expand_strip(const uint16_t *px, int y, int rows, uint16_t *out)
{
    if (!px || !out || y < 0 || rows <= 0 || y > DISPLAY_H - rows || y % MAZE_SCALE ||
        rows % MAZE_SCALE) {
        return false;
    }
    for (int row = 0; row < rows; row += MAZE_SCALE) {
        uint16_t *dst = out + row * DISPLAY_W;
        const uint16_t *src = px + ((y + row) / MAZE_SCALE) * MAZE_W;
        for (int x = 0; x < MAZE_W; ++x) {
            uint16_t sw = (uint16_t)((src[x] << 8) | (src[x] >> 8));
            dst[x * MAZE_SCALE] = dst[x * MAZE_SCALE + 1] = sw;
        }
        memcpy(dst + DISPLAY_W, dst, DISPLAY_W * sizeof(*dst));
    }
    return true;
}
