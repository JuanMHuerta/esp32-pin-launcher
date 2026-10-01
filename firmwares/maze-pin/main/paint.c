#include "paint.h"
#include <math.h>
#include <string.h>

static uint16_t rgb(int r, int g, int b)
{ return (uint16_t)(((r & 248) << 8) | ((g & 252) << 3) | (b >> 3)); }
static int clamp(int n, int lo, int hi) { return n < lo ? lo : n > hi ? hi : n; }
static uint32_t hash(int x, int y)
{ uint32_t n = (uint32_t)x * 0x45d9f3bu ^ (uint32_t)y * 0x119de1f3u; n ^= n >> 16; n *= 0x45d9f3bu; return n ^ (n >> 16); }

static uint16_t wall_texture[64 * 64], floor_texture[64 * 64], ceiling_texture[64 * 64];
static bool textures_ready;

/* Procedural art is made once; frame rendering only reads RGB565 texels. */
static void init_textures(void)
{
    if (textures_ready) return;
    for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x) {
        int i = y * 64 + x;
        int noise = (int)(hash(x / 3, y / 3) & 15) - 7;
        int brick_row = y / 16;
        bool mortar = y % 16 < 2 || ((x + (brick_row & 1 ? 16 : 0)) & 31) < 2;
        int r = 207 + noise * 2 - (y % 16 < 4 ? 13 : 0);
        int g = 69 + noise - (y % 16 < 4 ? 7 : 0);
        int b = 59 + noise;
        wall_texture[i] = mortar ? rgb(63, 42, 42) : rgb(clamp(r, 0, 255), clamp(g, 0, 255), clamp(b, 0, 255));

        bool floor_seam = y % 16 < 2 || ((x + ((y / 16) & 1 ? 32 : 0)) & 63) < 2;
        int grain = (int)(hash(x / 7, y / 3) & 15) - 7;
        floor_texture[i] = floor_seam ? rgb(66, 42, 27) : rgb(139 + grain, 104 + grain, 65 + grain / 2);

        bool ceiling_joint = x < 2 || y < 2 || x > 61 || y > 61;
        int stone = 103 + ((int)(hash(x / 5, y / 5) & 7) - 3);
        ceiling_texture[i] = ceiling_joint ? rgb(48, 49, 52) : rgb(stone, stone + 1, stone + 4);
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

static uint16_t shade_plane(uint16_t c, int level)
{
    if (!level) return c;
    uint16_t half = (uint16_t)((c & 0xf7deu) >> 1);
    if (level == 2) return half;
    return (uint16_t)(half + ((half & 0xf7deu) >> 1));
}

static void rect(uint16_t *p, int x, int y, int w, int h, uint16_t c)
{
    int l = clamp(x, 0, MAZE_W), t = clamp(y, 0, MAZE_H);
    int r = clamp(x + w, 0, MAZE_W), b = clamp(y + h, 0, MAZE_H);
    for (int yy = t; yy < b; ++yy) for (int xx = l; xx < r; ++xx) p[yy * MAZE_W + xx] = c;
}

static const uint8_t *glyph(char ch)
{
    static const uint8_t blank[7] = {0};
    static const uint8_t alpha[26][7] = {
        {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
        {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
        {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
        {1,1,1,1,17,17,14},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
        {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
        {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},{17,17,10,4,4,4,4},
        {31,1,2,4,8,16,31}};
    static const uint8_t digits[10][7] = {
        {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
        {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
        {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
        {14,17,17,15,1,1,14}};
    if (ch >= 'A' && ch <= 'Z') return alpha[ch - 'A'];
    if (ch >= '0' && ch <= '9') return digits[ch - '0'];
    return blank;
}

static void label(uint16_t *p, int x, int y, const char *s, int scale, uint16_t c)
{
    for (; *s; ++s, x += 6 * scale) {
        const uint8_t *g = glyph(*s);
        for (int gy = 0; gy < 7; ++gy) for (int gx = 0; gx < 5; ++gx)
            if (g[gy] & (1u << (4 - gx))) rect(p, x + gx * scale, y + gy * scale, scale, scale, c);
    }
}

/* Flat sprite primitives are occluded by the per-column wall depth. */
static void sprite(const maze_t *m, uint16_t *p, const float *depth, float wx, float wy, int kind)
{
    float dx = wx - m->x, dy = wy - m->y;
    float forward = dx * cosf(m->angle) + dy * sinf(m->angle);
    float sideways = -dx * sinf(m->angle) + dy * cosf(m->angle);
    if (forward < 0.18f || (kind != 0 && forward > 3.2f)) return;
    float scale = MAZE_H / forward;
    float center_z = kind == 0 ? 0.49f : kind == 1 ? 0.14f :
                     0.57f + 0.055f * sinf(m->elapsed_ms * 0.004f);
    int cx = (int)(MAZE_W / 2 + sideways * (MAZE_W / 1.5f) / forward);
    int cy = (int)(MAZE_H / 2 + (0.5f - center_z) * scale);
    int width = clamp((int)((kind == 0 ? 0.62f : kind == 1 ? 0.48f : 0.43f) * scale), 1, MAZE_W * 2);
    int height = clamp((int)((kind == 0 ? 0.98f : kind == 1 ? 0.28f : 0.43f) * scale), 1, MAZE_H * 4);
    int left = cx - width / 2, top = cy - height / 2;
    if (left >= MAZE_W || left + width < 0 || top >= MAZE_H || top + height < 0) return;
    float inv_width = 2.0f / width, inv_height = 2.0f / height;
    float wobble = kind == 2 ? sinf(m->elapsed_ms * 0.009f) * 0.20f : 0;
    for (int y = clamp(top, 0, MAZE_H); y < clamp(top + height, 0, MAZE_H); ++y)
        for (int x = clamp(left, 0, MAZE_W); x < clamp(left + width, 0, MAZE_W); ++x) {
            if (forward > depth[x] + 0.03f) continue;
            float u = (x - cx) * inv_width, v = (y - cy) * inv_height;
            uint16_t color = 0;
            if (kind == 0) { /* heavy green exit door with inset panels and gold knob */
                if (fabsf(u) > 0.93f || fabsf(v) > 0.96f) color = rgb(48, 33, 20);
                else if (fabsf(u) > 0.82f || fabsf(v) > 0.88f) color = rgb(32, 91, 53);
                else if (u > 0.48f && u < 0.65f && v > 0.05f && v < 0.2f) color = rgb(238, 194, 61);
                else if (fabsf(u) < 0.65f && (fabsf(v + 0.43f) < 0.30f || fabsf(v - 0.48f) < 0.28f)) color = rgb(27, 92, 50);
                else color = rgb(18, 61, 39);
                float face_x = u / 0.48f, face_y = (v + 0.43f) / 0.29f;
                if (face_x * face_x + face_y * face_y < 1.0f) {
                    color = rgb(249, 211, 52);
                    if (fabsf(face_x - 0.33f) < 0.10f && fabsf(face_y + 0.28f) < 0.13f) color = rgb(36, 29, 18);
                    if (fabsf(face_x + 0.33f) < 0.10f && fabsf(face_y + 0.28f) < 0.13f) color = rgb(36, 29, 18);
                    if (fabsf(face_x) < 0.55f && fabsf(face_y - (0.43f - face_x * face_x * 0.55f)) < 0.08f) color = rgb(36, 29, 18);
                }
            } else if (kind == 1) { /* gray rat: body, muzzle, pink ears and tail */
                if (u * u / 0.85f + v * v / 0.75f < 1.0f) color = rgb(93, 86, 82);
                if (u > 0.23f && u < 0.82f && v > -0.25f && v < 0.25f) color = rgb(146, 131, 119);
                if (u > 0.66f && u < 0.85f && fabsf(v) < 0.1f) color = rgb(213, 119, 118);
                if (u > 0.35f && u < 0.49f && v < -0.48f) color = rgb(207, 127, 129);
                if (u > 0.39f && u < 0.48f && v < -0.05f && v > -0.18f) color = rgb(10, 9, 8);
                if (u < -0.72f && fabsf(v + 0.32f + (u + 0.7f) * 0.5f) < 0.09f) color = rgb(213, 119, 118);
                if (v > 0.63f && v < 0.86f && ((u > -0.55f && u < -0.25f) || (u > 0.2f && u < 0.5f)))
                    color = rgb(69, 58, 55);
            } else { /* rotating faceted floating diamond */
                float au = fabsf(u + wobble), av = fabsf(v);
                if (au + av < 0.86f) {
                    if (v < 0) color = u < 0 ? rgb(250, 216, 66) : rgb(253, 113, 54);
                    else color = u < 0 ? rgb(46, 195, 220) : rgb(76, 91, 223);
                    if (au < 0.08f) color = rgb(255, 243, 170);
                }
            }
            if (color) p[y * MAZE_W + x] = color;
        }
}

void maze_paint(const maze_t *m, uint16_t p[MAZE_W * MAZE_H])
{
    init_textures();
    float depth[MAZE_W];
    float ca = cosf(m->angle), sa = sinf(m->angle);
    /* Row-wise floor casting replaces tens of thousands of per-pixel floorf,
     * division, and hash calls with fixed-point texture increments. */
    for (int y = 0; y < MAZE_H; ++y) {
        int row = y < MAZE_H / 2 ? MAZE_H / 2 - y : y - MAZE_H / 2;
        float distance = (MAZE_H / 2.0f) / (row ? row : 1);
        float left_x = m->x + (ca + sa * 0.75f) * distance;
        float left_y = m->y + (sa - ca * 0.75f) * distance;
        int32_t fx = (int32_t)(left_x * 65536.0f);
        int32_t fy = (int32_t)(left_y * 65536.0f);
        int32_t step_x = (int32_t)(-sa * 1.5f * distance * 65536.0f / MAZE_W);
        int32_t step_y = (int32_t)( ca * 1.5f * distance * 65536.0f / MAZE_W);
        const uint16_t *texture = y < MAZE_H / 2 ? ceiling_texture : floor_texture;
        int level = distance > 8.0f ? 2 : distance > 4.0f ? 1 : 0;
        uint16_t *dest = p + y * MAZE_W;
        for (int x = 0; x < MAZE_W; ++x) {
            dest[x] = shade_plane(texture[((fy >> 10) & 63) * 64 + ((fx >> 10) & 63)], level);
            fx += step_x; fy += step_y;
        }
    }
    for (int x = 0; x < MAZE_W; ++x) {
        float camera = (2.0f * x / MAZE_W - 1.0f) * 0.75f;
        float rx = ca - sa * camera, ry = sa + ca * camera;
        int map_x = (int)m->x, map_y = (int)m->y;
        float delta_x = fabsf(1.0f / rx), delta_y = fabsf(1.0f / ry);
        int step_x = rx < 0 ? -1 : 1, step_y = ry < 0 ? -1 : 1;
        float sx = (rx < 0 ? m->x - map_x : map_x + 1 - m->x) * delta_x;
        float sy = (ry < 0 ? m->y - map_y : map_y + 1 - m->y) * delta_y;
        int side = 0;
        for (int count = 0; count < MAZE_GRID * 2; ++count) {
            if (sx < sy) { sx += delta_x; map_x += step_x; side = 0; }
            else { sy += delta_y; map_y += step_y; side = 1; }
            if (!maze_open(m, map_x, map_y)) break;
        }
        float distance = side ? sy - delta_y : sx - delta_x;
        if (distance < 0.08f) distance = 0.08f;
        depth[x] = distance;
        int wall_h = (int)(MAZE_H / distance);
        int top = clamp((MAZE_H - wall_h) / 2, 0, MAZE_H);
        int bottom = clamp((MAZE_H + wall_h) / 2, 0, MAZE_H);
        float hit = side ? m->x + distance * rx : m->y + distance * ry;
        float u = hit - floorf(hit);
        if ((side == 0 && rx > 0) || (side == 1 && ry < 0)) u = 1.0f - u;
        int texture_x = ((int)(u * 64.0f)) & 63;
        int texture_step = (64 << 16) / wall_h;
        int texture_y = (32 << 16) + (top - MAZE_H / 2) * texture_step;
        int shade = clamp(256 - (int)(distance * 20.0f) - side * 23 +
                          ((map_x * 7 + map_y * 11) & 7), 82, 256);
        for (int y = top; y < bottom; ++y) {
            p[y * MAZE_W + x] = shade_wall(wall_texture[((texture_y >> 16) & 63) * 64 + texture_x], shade);
            texture_y += texture_step;
        }
        if (top > 0 && top < MAZE_H) p[top * MAZE_W + x] = rgb(31, 27, 24);
        if (bottom > 0 && bottom < MAZE_H) p[(bottom - 1) * MAZE_W + x] = rgb(36, 25, 18);
    }
    sprite(m, p, depth, m->exit_x + 0.5f, m->exit_y + 0.5f, 0);
    if (m->mode == MAZE_WALK && m->object_kind)
        sprite(m, p, depth, m->object_x, m->object_y, m->object_kind);
    if (m->mode == MAZE_TITLE) {
        rect(p, 48, 31, 172, 56, rgb(15, 18, 18));
        rect(p, 51, 34, 166, 50, rgb(101, 77, 48));
        rect(p, 54, 37, 160, 44, rgb(12, 18, 16));
        label(p, 69, 44, "3D MAZE", 3, rgb(255, 226, 95));
        label(p, 85, 70, "AUTOPILOT", 1, rgb(216, 216, 205));
    } else if (m->mode == MAZE_EXIT) {
        rect(p, 64, 46, 140, 28, rgb(16, 28, 22));
        label(p, 75, 53, "EXIT FOUND", 2, rgb(249, 226, 116));
    }
}

bool maze_expand_strip(const uint16_t *px, int y, int rows, uint16_t *out)
{
    if (!px || !out || y < 0 || rows <= 0 || y > DISPLAY_H - rows || y % MAZE_SCALE || rows % MAZE_SCALE) return false;
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
