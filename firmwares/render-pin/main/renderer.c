// SPDX-License-Identifier: GPL-3.0-only
#include "renderer.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FAR_DUST 390
#define PI 3.141592654f

typedef struct {
    uint8_t r, g, b;
} rgb_t;
typedef struct {
    rgb_t star, line;
} palette_t;
typedef struct {
    float x, y, z;
    uint32_t seed;
} star_seed_t;
typedef struct {
    float cy, sy, cp, sp, cr, sr, tx, ty;
} camera_t;
static const palette_t palettes[3] = {
    {{230, 242, 255}, {112, 185, 245}}, /* white stars, ice-blue lines */
    {{255, 241, 221}, {225, 189, 133}}, /* warm white */
    {{245, 245, 245}, {175, 185, 195}}, /* silver */
};
static const rgb_t rare_stars[2] = {{255, 214, 142}, {255, 151, 151}};
/* Precomputed subpixel star disks keep larger stars inexpensive to draw. */
static uint8_t star_mask[4][64][81];
static star_seed_t seeds[STAR_COUNT];
static int16_t sine[1024];
/* Six points and five edges: a crooked path with a short branch. */
static const uint8_t edge_a[5] = {0, 1, 2, 3, 2};
static const uint8_t edge_b[5] = {1, 2, 3, 4, 5};

static int clip(int n, int lo, int hi)
{
    return n < lo ? lo : n > hi ? hi : n;
}
static float limit(float n, float lo, float hi)
{
    return n < lo ? lo : n > hi ? hi : n;
}
static uint32_t hash32(uint32_t n)
{
    n ^= n >> 16;
    n *= 0x7feb352dU;
    n ^= n >> 15;
    n *= 0x846ca68bU;
    return n ^ (n >> 16);
}
static float random_signed(uint32_t h)
{
    return (h & 65535) / 32767.5f - 1.0f;
}
static int star_hue(uint32_t seed)
{
    /* Roughly one in eighty stars gets a warm or red accent. */
    if (seed % 211 == 0) {
        return 2;
    }
    if (seed % 131 == 0) {
        return 1;
    }
    return 0;
}
static uint16_t wire565(int r, int g, int b)
{
    return __builtin_bswap16((uint16_t)(((clip(r, 0, 255) >> 3) << 11) |
                                        ((clip(g, 0, 255) >> 2) << 5) | (clip(b, 0, 255) >> 3)));
}
static void add_pixel(uint16_t *pixel, rgb_t ink, int alpha)
{
    uint16_t c = __builtin_bswap16(*pixel);
    *pixel = wire565(((c >> 11) & 31) * 255 / 31 + ink.r * alpha / 255,
                     ((c >> 5) & 63) * 255 / 63 + ink.g * alpha / 255,
                     (c & 31) * 255 / 31 + ink.b * alpha / 255);
}
void renderer_init(void)
{
    for (int i = 0; i < 1024; ++i) {
        sine[i] = (int16_t)lrintf(32767 * sinf(2 * PI * i / 1024));
    }
    for (int size = 0; size < 4; ++size) {
        float radius = (float[]){1.05f, 1.45f, 1.98f, 2.2f}[size];
        for (int phase = 0; phase < 64; ++phase) {
            for (int y = -4; y <= 4; ++y) {
                for (int x = -4; x <= 4; ++x) {
                    float dx = x - (phase % 8) / 8.0f;
                    float dy = y - (phase / 8) / 8.0f;
                    float d = dx * dx + dy * dy;
                    float core = limit(1.4f - d / (radius * radius), 0, 1);
                    float halo = size ? 0.10f * expf(-d / 5.0f) : 0;
                    star_mask[size][phase][(y + 4) * 9 + x + 4] =
                        (uint8_t)(255 * limit(core + halo, 0, 1));
                }
            }
        }
    }
    for (int i = 0; i < STAR_COUNT; ++i) {
        uint32_t h = hash32(i + 0x912f387aU), k = hash32(h);
        seeds[i] = (star_seed_t){random_signed(h) * 1450, random_signed(k) * 850,
                                 180 + (k >> 16) % 1620, h};
        if (i < FAR_DUST) {
            /* A spherical distant sky stays populated at wide camera angles. */
            float height = random_signed(k);
            float radius = sqrtf(1 - height * height) * 2400;
            int angle = h & 1023;
            seeds[i].x = radius * sine[angle] / 32768.0f;
            seeds[i].y = height * 2400;
            seeds[i].z = radius * sine[(angle + 256) & 1023] / 32768.0f;
        }
    }
}

/* Camera rotations change both the position and the perspective denominator. */
static void project(projected_star_t *p, float x, float y, float z, const camera_t *c, int alpha,
                    int size, int hue)
{
    x -= c->tx;
    y -= c->ty;
    float xx = c->cy * x - c->sy * z;
    float zz = c->sy * x + c->cy * z;
    float yy = c->cp * y - c->sp * zz;
    zz = c->sp * y + c->cp * zz;
    *p = (projected_star_t){.hue = hue};
    if (zz < 95) {
        return;
    }
    float scale = 240 / zz;
    float sx = 268 + (c->cr * xx - c->sr * yy) * scale;
    float sy = 120 + (c->sr * xx + c->cr * yy) * scale;
    if (sx < -500 || sx > 1036 || sy < -400 || sy > 640) {
        return;
    }
    int x8 = (int)floorf(sx * 8), y8 = (int)floorf(sy * 8);
    p->x = (int16_t)floorf(sx);
    p->y = (int16_t)floorf(sy);
    p->frac_x = x8 - p->x * 8;
    p->frac_y = y8 - p->y * 8;
    p->alpha = clip(alpha, 0, 255);
    p->size = size;
    p->hue = hue;
}

void renderer_prepare(renderer_t *state, int32_t time_ms, float view_x, float view_y, float roll,
                      float energy, uint8_t mood, int pulse_x, int pulse_y, int32_t pulse_age_ms)
{
    state->mood = mood % 3;
    float t = time_ms * 0.001f;
    float yaw = limit(view_x, -1.5f, 1.5f) * 0.75f + 0.025f * sinf(t * 0.13f);
    float pitch = limit(view_y, -1.5f, 1.5f) * 0.60f + 0.02f * sinf(t * 0.17f);
    float bank = roll * (2 * PI / 1024) + 0.035f * sinf(t * 0.11f);
    camera_t cam = {cosf(yaw),  sinf(yaw),  cosf(pitch),  sinf(pitch),
                    cosf(bank), sinf(bank), view_x * 110, view_y * 85};
    (void)energy; /* Movement changes the camera, never the brightness. */
    for (int i = 0; i < DUST_COUNT; ++i) {
        const star_seed_t *s = &seeds[i];
        if (i < FAR_DUST) {
            int brightness = 170 + (int)(s->seed % 70);
            brightness += sine[(i * 97 + time_ms / 19) & 1023] / 4000;
            project(&state->stars[i], s->x, s->y, s->z, &cam, brightness, 0, star_hue(s->seed));
            continue;
        }
        float travel = t * 67;
        int cycle = (int)floorf((travel + 1800 - s->z) / 1620);
        float z = s->z - travel + cycle * 1620;
        uint32_t h = hash32(s->seed + (uint32_t)cycle * 0x9e3779b9U);
        float x = random_signed(h) * 1450;
        float y = random_signed(hash32(h)) * 850;
        float fade = limit((z - 180) / 150, 0, 1) * limit((1800 - z) / 220, 0, 1);
        int brightness = (int)((285 - z * 0.045f) * fade);
        brightness += sine[(i * 97 + time_ms / (13 + i % 9)) & 1023] / 2100;
        int size = z < 650 ? 2 : 1;
        if (i >= FAR_DUST && h % 29 == 0 && z < 1100) {
            size = 3;
        }
        project(&state->stars[i], x, y, z, &cam, brightness, size, star_hue(h));
    }
    for (int group = 0; group < CONSTELLATIONS; ++group) {
        float initial_z = 390 + group * 113;
        float travel = t * 58;
        int cycle = (int)floorf((travel + 1700 - initial_z) / 1480);
        float z = initial_z - travel + cycle * 1480;
        uint32_t h = hash32(0x5e2a319U + group * 491U + (uint32_t)cycle * 9719U);
        /* Stratified anchors prevent a single clump and leave open sky. */
        float anchor_x = ((group % 4) - 1.5f) * 430 + random_signed(h) * 95;
        float anchor_y = ((group / 4) - 1.0f) * 320 + random_signed(hash32(h)) * 90;
        int angle = (h >> 16) & 1023;
        float cs = sine[(angle + 256) & 1023] / 32768.0f;
        float sn = sine[angle] / 32768.0f;
        float fade = limit((z - 220) / 180, 0, 1) * limit((1700 - z) / 260, 0, 1);
        int base = DUST_COUNT + group * NODES_PER_CONSTELLATION;
        for (int n = 0; n < NODES_PER_CONSTELLATION; ++n) {
            uint32_t k = hash32(h + n * 739);
            float nx = n == 5 ? 5 : (n - 2) * 47;
            float ny = n == 5 ? 85 : random_signed(k) * 43;
            float nz = random_signed(hash32(k)) * 24;
            int twinkle = sine[(time_ms / 16 + n * 155 + group * 87) & 1023] / 3000;
            project(&state->stars[base + n], anchor_x + nx * cs - ny * sn,
                    anchor_y + nx * sn + ny * cs, z + nz, &cam, (int)((255 + twinkle) * fade),
                    n == 2 ? 3 : 2, star_hue(k));
        }
        for (int e = 0; e < 5; ++e) {
            projected_edge_t *edge = &state->edges[group * 5 + e];
            edge->a = base + edge_a[e];
            edge->b = base + edge_b[e];
            edge->alpha = (int)(85 * fade);
            edge->phase = (time_ms / 24 + group * 39 + e * 51) & 255;
        }
    }
    /* A touch sends a light wave through the existing stars and connections. */
    if (pulse_age_ms >= 0 && pulse_age_ms < 1900) {
        int radius = pulse_age_ms * 2 / 5;
        for (int i = 0; i < STAR_COUNT; ++i) {
            projected_star_t *p = &state->stars[i];
            if (!p->alpha) {
                continue;
            }
            float dx = p->x - pulse_x, dy = p->y - pulse_y;
            int distance = (int)sqrtf(dx * dx + dy * dy);
            int wave = clip(48 - abs(distance - radius), 0, 48);
            p->alpha = clip(p->alpha + wave * 2, 0, 255);
            if (wave > 28) {
                p->size = 3;
            }
        }
    }
}

static void stamp(uint16_t *pixels, int y0, int rows, int x, int y, rgb_t color, int alpha)
{
    if (alpha > 0 && x >= 0 && x < PIN_WIDTH && y >= y0 && y < y0 + rows) {
        add_pixel(&pixels[(y - y0) * PIN_WIDTH + x], color, alpha);
    }
}
static void soft(uint16_t *pixels, int y0, int rows, int x8, int y8, rgb_t color, int alpha)
{
    int x = x8 >= 0 ? x8 / 8 : -((-x8 + 7) / 8);
    int y = y8 >= 0 ? y8 / 8 : -((-y8 + 7) / 8);
    int fx = x8 - x * 8, fy = y8 - y * 8;
    stamp(pixels, y0, rows, x, y, color, alpha * (8 - fx) * (8 - fy) / 64);
    stamp(pixels, y0, rows, x + 1, y, color, alpha * fx * (8 - fy) / 64);
    stamp(pixels, y0, rows, x, y + 1, color, alpha * (8 - fx) * fy / 64);
    stamp(pixels, y0, rows, x + 1, y + 1, color, alpha * fx * fy / 64);
}
static void draw_edge(uint16_t *pixels, int y0, int rows, const projected_star_t *a,
                      const projected_star_t *b, const projected_edge_t *edge, rgb_t ink)
{
    if (!a->alpha || !b->alpha || !edge->alpha) {
        return;
    }
    if ((a->y < y0 - 2 && b->y < y0 - 2) || (a->y > y0 + rows && b->y > y0 + rows) ||
        (a->x < -2 && b->x < -2) || (a->x > PIN_WIDTH && b->x > PIN_WIDTH)) {
        return;
    }
    int ax = a->x * 8 + a->frac_x, ay = a->y * 8 + a->frac_y;
    int dx = b->x * 8 + b->frac_x - ax, dy = b->y * 8 + b->frac_y - ay;
    int steps = (abs(dx) > abs(dy) ? abs(dx) : abs(dy)) / 8 + 1;
    if (steps > 450) {
        return;
    }
    for (int s = 1; s < steps; ++s) {
        int x8 = ax + dx * s / steps, y8 = ay + dy * s / steps;
        /* A quiet traveling highlight; the whole line remains visible. */
        int glint = clip(18 - abs(s * 255 / steps - edge->phase), 0, 18);
        soft(pixels, y0, rows, x8, y8, ink, edge->alpha + glint * edge->alpha / 30);
    }
}
void renderer_strip(const renderer_t *state, int y0, int rows, uint16_t *pixels)
{
    memset(pixels, 0, (size_t)rows * PIN_WIDTH * sizeof(*pixels));
    const palette_t *palette = &palettes[state->mood];
    for (int i = 0; i < EDGE_COUNT; ++i) {
        const projected_edge_t *e = &state->edges[i];
        draw_edge(pixels, y0, rows, &state->stars[e->a], &state->stars[e->b], e, palette->line);
    }
    for (int i = 0; i < STAR_COUNT; ++i) {
        const projected_star_t *p = &state->stars[i];
        if (!p->alpha || p->y < y0 - 10 || p->y > y0 + rows + 10 || p->x < -10 ||
            p->x > PIN_WIDTH + 10) {
            continue;
        }
        rgb_t ink = p->hue ? rare_stars[p->hue - 1] : palette->star;
        int x8 = p->x * 8 + p->frac_x, y8 = p->y * 8 + p->frac_y;
        const uint8_t *mask = star_mask[p->size][p->frac_y * 8 + p->frac_x];
        for (int dy = -4; dy <= 4; ++dy) {
            for (int dx = -4; dx <= 4; ++dx) {
                int alpha = mask[(dy + 4) * 9 + dx + 4] * p->alpha / 255;
                if (alpha) {
                    stamp(pixels, y0, rows, p->x + dx, p->y + dy, ink, alpha);
                }
            }
        }
        if (p->size == 3) {
            for (int d = 1; d <= (p->size == 3 ? 8 : 2); ++d) {
                int ray = p->alpha * (9 - d) / (p->size == 3 ? 25 : 45);
                soft(pixels, y0, rows, x8 - d * 8, y8, ink, ray);
                soft(pixels, y0, rows, x8 + d * 8, y8, ink, ray);
                soft(pixels, y0, rows, x8, y8 - d * 8, ink, ray);
                soft(pixels, y0, rows, x8, y8 + d * 8, ink, ray);
            }
        }
    }
}
