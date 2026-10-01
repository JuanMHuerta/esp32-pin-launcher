#include "paint.h"
#include "assets_generated.h"

#include <string.h>

/* World art is packed 4-bit source-sheet data. This file only composes it. */
typedef struct { uint16_t *pixels; } canvas_t;
enum { VIEW_H = 51, P_VOID = 1, P_DEEP = 2, P_STONE = 3, P_LIT = 4,
       P_BONE = 6, P_AMBER = 7, P_FIRE = 8, P_BLOOD = 9, P_VIOLET = 10,
       P_BLUE = 11, P_MAGIC = 12, P_LIGHT = 13, P_MOSS = 14 };

static void fill(canvas_t *c, int x, int y, int w, int h, unsigned color)
{
    int right = x + w, bottom = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (right > DUNGEON_W) right = DUNGEON_W;
    if (bottom > DUNGEON_H) bottom = DUNGEON_H;
    for (int yy = y; yy < bottom; ++yy) for (int xx = x; xx < right; ++xx)
        c->pixels[yy * DUNGEON_W + xx] = dungeon_palette[color & 15u];
}

static unsigned sprite_pixel(const dungeon_sprite_t *s, int x, int y)
{
    unsigned offset = (unsigned)y * s->width + (unsigned)x;
    uint8_t packed = s->pixels[offset >> 1];
    return offset & 1u ? packed & 15u : packed >> 4;
}

/* Clipped nearest-neighbour sprite blit at logical resolution. */
static void sprite(canvas_t *c, enum dungeon_sprite_id id, int x, int y, int w, int h)
{
    const dungeon_sprite_t *s;
    if ((unsigned)id >= DSP_COUNT || w <= 0 || h <= 0) return;
    s = &dungeon_sprites[id];
    int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y;
    int right = x + w > DUNGEON_W ? DUNGEON_W : x + w;
    int bottom = y + h > DUNGEON_H ? DUNGEON_H : y + h;
    for (int yy = top; yy < bottom; ++yy) {
        int sy = (yy - y) * s->height / h;
        for (int xx = left; xx < right; ++xx) {
            unsigned p = sprite_pixel(s, (xx - x) * s->width / w, sy);
            if (p) c->pixels[yy * DUNGEON_W + xx] = dungeon_palette[p];
        }
    }
}

static const uint8_t *glyph(char z)
{
    static const uint8_t blank[7] = {0};
    static const uint8_t letters[26][7] = {
        {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},{1,1,1,1,17,17,14},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},{17,17,10,4,4,4,4},{31,1,2,4,8,16,31}};
    return z >= 'A' && z <= 'Z' ? letters[z - 'A'] : blank;
}

static void text(canvas_t *c, int x, int y, const char *s, int scale, unsigned color)
{
    for (; *s; ++s, x += 6 * scale) {
        const uint8_t *g = glyph(*s);
        for (int yy = 0; yy < 7; ++yy) for (int xx = 0; xx < 5; ++xx)
            if (g[yy] & (1u << (4 - xx))) fill(c, x + xx * scale, y + yy * scale, scale, scale, color);
    }
}

static void plot(canvas_t *c, int x, int y, unsigned color)
{
    if ((unsigned)x < DUNGEON_W && (unsigned)y < VIEW_H) c->pixels[y * DUNGEON_W + x] = dungeon_palette[color & 15u];
}

static void line(canvas_t *c, int x0, int y0, int x1, int y1, unsigned color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        plot(c, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int twice = error * 2;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
    }
}

/* Map one packed stone tile into a true trapezoid.  This keeps source art
 * static and allocation-free while its near edge expands toward the viewer. */
static void trapezoid_sprite(canvas_t *c, enum dungeon_sprite_id id,
                             int left_top, int right_top, int top,
                             int left_bottom, int right_bottom, int bottom)
{
    const dungeon_sprite_t *s;
    if ((unsigned)id >= DSP_COUNT || bottom <= top || right_top <= left_top || right_bottom <= left_bottom) return;
    s = &dungeon_sprites[id];
    for (int y = top < 0 ? 0 : top; y <= bottom && y < VIEW_H; ++y) {
        int t = (y - top) * 256 / (bottom - top);
        int left = left_top + (left_bottom - left_top) * t / 256;
        int right = right_top + (right_bottom - right_top) * t / 256;
        int width = right - left;
        if (width <= 0) continue;
        int sy = (y - top) * s->height / (bottom - top + 1);
        for (int x = left < 0 ? 0 : left; x <= right && x < DUNGEON_W; ++x) {
            unsigned p = sprite_pixel(s, (x - left) * s->width / (width + 1), sy);
            if (p) c->pixels[y * DUNGEON_W + x] = dungeon_palette[p];
        }
    }
}

static int edge(int ax, int ay, int bx, int by, int px, int py)
{
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static void triangle(canvas_t *c, int ax, int ay, int bx, int by, int cx, int cy, unsigned color)
{
    int left = ax < bx ? (ax < cx ? ax : cx) : (bx < cx ? bx : cx);
    int right = ax > bx ? (ax > cx ? ax : cx) : (bx > cx ? bx : cx);
    int top = ay < by ? (ay < cy ? ay : cy) : (by < cy ? by : cy);
    int bottom = ay > by ? (ay > cy ? ay : cy) : (by > cy ? by : cy);
    int winding = edge(ax, ay, bx, by, cx, cy);
    if (left < 0) left = 0;
    if (right >= DUNGEON_W) right = DUNGEON_W - 1;
    if (top < 0) top = 0;
    if (bottom >= VIEW_H) bottom = VIEW_H - 1;
    for (int y = top; y <= bottom; ++y) for (int x = left; x <= right; ++x) {
        int a = edge(ax, ay, bx, by, x, y), b = edge(bx, by, cx, cy, x, y), d = edge(cx, cy, ax, ay, x, y);
        if ((winding >= 0 && a >= 0 && b >= 0 && d >= 0) || (winding < 0 && a <= 0 && b <= 0 && d <= 0)) plot(c, x, y, color);
    }
}

/* Side walls are actual receding quadrilaterals.  The seams run from their
 * broad near edge to the narrow end-wall edge, giving the room its depth. */
static void wall_plane(canvas_t *c, bool left_side, int inner_top_x, int inner_top_y,
                       int inner_bottom_x, int inner_bottom_y)
{
    int outer = left_side ? 0 : DUNGEON_W - 1;
    triangle(c, outer, 0, inner_top_x, inner_top_y, inner_bottom_x, inner_bottom_y, P_STONE);
    triangle(c, outer, 0, inner_bottom_x, inner_bottom_y, outer, VIEW_H - 1, P_STONE);
    for (int n = 1; n < 6; ++n) {
        int y_outer = n * (VIEW_H - 1) / 6;
        int y_inner = inner_top_y + n * (inner_bottom_y - inner_top_y) / 6;
        line(c, outer, y_outer, inner_top_x + n * (inner_bottom_x - inner_top_x) / 6, y_inner, P_DEEP);
    }
    for (int n = 1; n < 5; ++n) {
        int x_top = outer + n * (inner_top_x - outer) / 5;
        int x_bottom = outer + n * (inner_bottom_x - outer) / 5;
        line(c, x_top, n * inner_top_y / 5, x_bottom,
             (VIEW_H - 1) + n * (inner_bottom_y - (VIEW_H - 1)) / 5, P_LIT);
    }
    line(c, inner_top_x, inner_top_y, inner_bottom_x, inner_bottom_y, P_LIT);
}

/* Centred vanishing point, with ceiling and floor planes expanding from the
 * rear arch to the display edges. */
static void deep_corridor(canvas_t *c, const dungeon_t *d)
{
    int shift = (int)(d->layout % 3u) - 1;
    fill(c, 0, 0, DUNGEON_W, VIEW_H, P_DEEP);

    trapezoid_sprite(c, DSP_CEILING_FAR, 60 + shift, 74 + shift, 3, 54 + shift, 80 + shift, 8);
    trapezoid_sprite(c, DSP_CEILING_MID, 54 + shift, 80 + shift, 8, 36, 98, 16);
    trapezoid_sprite(c, DSP_CEILING_NEAR, 36, 98, 16, 0, DUNGEON_W - 1, 26);
    trapezoid_sprite(c, DSP_FLOOR_FAR, 57 + shift, 77 + shift, 29, 53 + shift, 81 + shift, 33);
    trapezoid_sprite(c, DSP_FLOOR_MID, 53 + shift, 81 + shift, 33, 34, 100, 39);
    trapezoid_sprite(c, DSP_FLOOR_NEAR, 34, 100, 39, 0, DUNGEON_W - 1, VIEW_H - 1);
    wall_plane(c, true, 51 + shift, 9, 51 + shift, 33);
    wall_plane(c, false, 82 + shift, 9, 82 + shift, 33);
    sprite(c, DSP_WALL_FAR, 42 + shift, 13, 12, 17);
    sprite(c, DSP_WALL_MID, 23, 20, 13, 20);
    sprite(c, DSP_WALL_NEAR, 2, 31, 16, 16);
    sprite(c, DSP_WALL_FAR, 80 + shift, 13, 12, 17);
    sprite(c, DSP_WALL_MID, 98, 20, 13, 20);
    sprite(c, DSP_WALL_NEAR, 116, 31, 16, 16);
    sprite(c, DSP_ARCH, 50 + shift, 7, 34, 31);
    sprite(c, DSP_TORCH, 29, 18, 13, 18);
    sprite(c, DSP_TORCH, 92, 18, 13, 18);
}

/* A shallower, wider room: broad rear courses hold the set piece while the
 * floor remains readable enough to frame chests, shrines, and bosses. */
static void chamber(canvas_t *c, const dungeon_t *d)
{
    int shift = (int)(d->layout % 3u) - 1;
    fill(c, 0, 0, DUNGEON_W, VIEW_H, P_DEEP);

    trapezoid_sprite(c, DSP_CEILING_FAR, 56 + shift, 78 + shift, 3, 51 + shift, 83 + shift, 8);
    trapezoid_sprite(c, DSP_CEILING_MID, 51 + shift, 83 + shift, 8, 34, 100, 15);
    trapezoid_sprite(c, DSP_CEILING_NEAR, 34, 100, 15, 0, DUNGEON_W - 1, 23);
    trapezoid_sprite(c, DSP_FLOOR_FAR, 53 + shift, 81 + shift, 29, 48 + shift, 86 + shift, 34);
    trapezoid_sprite(c, DSP_FLOOR_MID, 48 + shift, 86 + shift, 34, 30, 104, 40);
    trapezoid_sprite(c, DSP_FLOOR_NEAR, 30, 104, 40, 0, DUNGEON_W - 1, VIEW_H - 1);
    wall_plane(c, true, 45 + shift, 11, 45 + shift, 34);
    wall_plane(c, false, 88 + shift, 11, 88 + shift, 34);
    sprite(c, DSP_WALL_FAR, 34, 10, 12, 18);
    sprite(c, DSP_WALL_MID, 14, 22, 14, 19);
    sprite(c, DSP_WALL_NEAR, 1, 34, 16, 14);
    sprite(c, DSP_WALL_FAR, 88, 10, 12, 18);
    sprite(c, DSP_WALL_MID, 106, 22, 14, 19);
    sprite(c, DSP_WALL_NEAR, 117, 34, 16, 14);
    trapezoid_sprite(c, DSP_WALL_FAR, 45 + shift, 88 + shift, 11, 45 + shift, 88 + shift, 29);
    sprite(c, DSP_ARCH, 49 + shift, 6, 36, 32);
    sprite(c, DSP_TORCH, 24, 17, 13, 18);
    sprite(c, DSP_TORCH, 97, 17, 13, 18);
}

static bool uses_chamber(const dungeon_t *d)
{
    if (d->state == DUNGEON_BOSS) return true;
    return d->room == DUNGEON_ARRIVAL || d->room == DUNGEON_TREASURE || d->room == DUNGEON_SHRINE;
}

static void scenery(canvas_t *c, const dungeon_t *d)
{
    switch (d->catacomb_set % 3u) {
    case 0: sprite(c, DSP_BANNER, 7, 3, 21, 31); sprite(c, DSP_BANNER, 106, 3, 21, 31); sprite(c, DSP_RUBBLE, 18, 39, 28, 12); sprite(c, DSP_RUBBLE, 88, 39, 28, 12); break;
    case 1: sprite(c, DSP_CHAIN, 5, 0, 22, 33); sprite(c, DSP_CHAIN, 108, 0, 22, 33); sprite(c, DSP_SKULLS, 13, 40, 33, 11); sprite(c, DSP_SKULLS, 88, 40, 33, 11); break;
    default: sprite(c, DSP_ROOTS, 0, 0, 30, 32); sprite(c, DSP_ROOTS, 104, 0, 30, 32); sprite(c, DSP_RUBBLE, 7, 40, 31, 11); sprite(c, DSP_RUBBLE, 96, 40, 31, 11); break;
    }
}

static enum dungeon_sprite_id actor_id(const dungeon_t *d)
{
    unsigned frame = d->phase == 0 ? 0 : d->phase == 1 ? 1 : d->phase == 2 ? 2 : 3;
    return (enum dungeon_sprite_id)(DSP_SKELETON_0 + (d->enemy % 4u) * 4u + frame);
}

static void enemy(canvas_t *c, const dungeon_t *d)
{
    if (d->phase == 0) sprite(c, actor_id(d), 57, 24, 20, 20); else sprite(c, actor_id(d), 45, 3, 44, 44);
    if (d->phase == 1) sprite(c, DSP_IMPACT, 51, 5, 32, 18);
    if (d->phase == 2) sprite(c, DSP_BOLT, 50, 10, 35, 27);
}

static void objective(canvas_t *c, const dungeon_t *d)
{
    switch (d->room) {
    case DUNGEON_ARRIVAL: sprite(c, DSP_DOOR, 52, 12, 30, 35); break;
    case DUNGEON_CORRIDOR: sprite(c, DSP_ARCH, 54, 16, 27, 25); break;
    case DUNGEON_TREASURE: sprite(c, DSP_CHEST, 45, d->phase >= 3 ? 18 : 25, 45, 27); if (d->phase >= 3) sprite(c, DSP_BURST, 51, 8, 32, 24); break;
    case DUNGEON_AMBUSH: case DUNGEON_ELITE: enemy(c, d); break;
    case DUNGEON_SHRINE: sprite(c, DSP_SHRINE, 45, 12, 45, 35); sprite(c, d->phase & 1u ? DSP_BOLT : DSP_IMPACT, 54, 4, 28, 24); break;
    case DUNGEON_EXIT: sprite(c, DSP_GATE, 43, 7, 48, 42); break;
    default: break;
    }
}

static void boss(canvas_t *c, const dungeon_t *d)
{
    unsigned frame = d->phase == 0 ? 0 : d->phase == 1 ? 1 : d->phase == 2 ? 2 : 3;
    enum dungeon_sprite_id id = (enum dungeon_sprite_id)(DSP_WARDEN_0 + d->boss * 4u + frame);
    sprite(c, id, 41, 0, 52, 51);
    if (d->phase == 1) sprite(c, DSP_IMPACT, 50, 2, 34, 22);
    if (d->phase == 2) sprite(c, d->boss == DUNGEON_WYRM ? DSP_BOLT : DSP_SLASH, 48, 8, 40, 31);
    if (d->phase >= 4) sprite(c, DSP_BURST, 48, 8, 40, 31);
}

static void weapon(canvas_t *c, const dungeon_t *d)
{
    if (d->phase == 3) { sprite(c, d->weapon ? DSP_STAFF : DSP_SWORD, d->weapon ? 83 : 18, 23, 42, 29); sprite(c, d->weapon ? DSP_BOLT : DSP_SLASH, 48, 16, 43, 30); }
    else if (d->phase >= 4) sprite(c, DSP_HANDS, 92, 31, 31, 22);
}

static void particles(canvas_t *c, const dungeon_t *d)
{
    static const uint8_t colors[] = { P_FIRE, P_MAGIC, P_LIGHT };
    for (unsigned i = 0; i < DUNGEON_PARTICLES; ++i) { const dungeon_particle_t *p = &d->particles[i]; if (p->life) fill(c, p->x, p->y, p->life > 8 ? 2 : 1, p->life > 8 ? 2 : 1, colors[(p->color - 1u) % 3u]); }
}

static void hud(canvas_t *c, const dungeon_t *d)
{
    fill(c, 0, 51, 134, 9, P_VOID); fill(c, 0, 51, 134, 1, P_LIT); text(c, 3, 53, "HP", 1, P_BONE); fill(c, 16, 54, 20, 3, P_BLOOD); fill(c, 17, 55, 16, 1, P_FIRE); text(c, 47, 53, "RUN", 1, P_LIGHT); text(c, 87, 53, "MP", 1, P_MAGIC); fill(c, 101, 54, 22, 3, P_MAGIC); fill(c, 125, 54, 5, 3, d->normal_rooms >= d->target_rooms ? P_FIRE : P_BLUE);
}

static void wipe(canvas_t *c, const dungeon_t *d)
{
    int w = d->state == DUNGEON_DOOR_OPEN ? 67 - (int)(d->state_ms * 67u / 430u) : (int)(d->state_ms * 67u / 430u);
    if (w < 0) w = 0;
    if (w > 67) w = 67;
    fill(c, 0, 0, w, VIEW_H, P_VOID); fill(c, 134 - w, 0, w, VIEW_H, P_VOID); sprite(c, DSP_GATE, w - 16, 15, 18, 25); sprite(c, DSP_GATE, 132 - w, 15, 18, 25);
}

static void title(canvas_t *c, const dungeon_t *d)
{
    fill(c, 0, 0, 134, 60, P_VOID); sprite(c, DSP_ARCH, 5, 4, 124, 48); sprite(c, DSP_TITLE, 48, 5, 39, 30); sprite(c, DSP_TORCH, 18, 31, 20, 20); sprite(c, DSP_TORCH, 96, 31, 20, 20); text(c, 22, 37, "DUNGEON", 2, d->uptime_ms / 180u & 1u ? P_LIGHT : P_FIRE); text(c, 51, 51, "SEED", 1, P_MAGIC);
}

void dungeon_paint(const dungeon_t *d, uint16_t pixels[DUNGEON_W * DUNGEON_H])
{
    canvas_t c = { pixels };
    if (d->state == DUNGEON_TITLE) { title(&c, d); return; }
    if (uses_chamber(d)) chamber(&c, d); else deep_corridor(&c, d);
    scenery(&c, d); if (d->state == DUNGEON_BOSS) boss(&c, d); else objective(&c, d); weapon(&c, d); particles(&c, d); if (d->state == DUNGEON_DOOR_OPEN || d->state == DUNGEON_DOOR_CLOSE) wipe(&c, d); hud(&c, d);
}

bool dungeon_expand_strip(const uint16_t *px, int y, int rows, uint16_t *out)
{
    if (!px || !out || y < 0 || rows <= 0 || y > DISPLAY_H - rows || y % DUNGEON_SCALE || rows % DUNGEON_SCALE) return false;
    for (int row = 0; row < rows; row += DUNGEON_SCALE) {
        uint16_t *dst = out + row * DISPLAY_W; const uint16_t *src = px + ((y + row) / DUNGEON_SCALE) * DUNGEON_W;
        for (int x = 0; x < DUNGEON_W; ++x) for (int scale = 0; scale < DUNGEON_SCALE; ++scale) dst[x * DUNGEON_SCALE + scale] = (uint16_t)((src[x] << 8) | (src[x] >> 8));
        for (int scale = 1; scale < DUNGEON_SCALE; ++scale) memcpy(dst + scale * DISPLAY_W, dst, DISPLAY_W * sizeof(*dst));
    }
    return true;
}
