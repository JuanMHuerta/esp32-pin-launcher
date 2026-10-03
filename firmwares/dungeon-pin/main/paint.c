// SPDX-License-Identifier: GPL-3.0-only
#include "paint.h"
#include "assets_generated.h"

#include <math.h>
#include <string.h>

enum {
    VIEW_H = DUNGEON_H,
    HORIZON = 48,
    PROJECTION = 156,
    DEEP = 2,
    STONE = 4,
    WHITE = 61,
    RED = 13,
    MAGIC = 29,
    GOLD = 36
};
typedef struct {
    uint16_t *pixels;
} canvas_t;
typedef struct {
    float x, y, height, z, depth;
    unsigned node, kind;
    enum dungeon_sprite_id id;
} item_t;
static uint16_t shaded[4][DUNGEON_PALETTE_SIZE];
static float wall_depth[DUNGEON_W];
static item_t items[DUNGEON_NODES * 5];
static bool palette_ready;

static uint16_t dim(uint16_t p, unsigned amount)
{
    return (uint16_t)((((p >> 11) * amount / 256u) << 11) |
                      ((((p >> 5) & 63u) * amount / 256u) << 5) | ((p & 31u) * amount / 256u));
}

static void prepare_palette(void)
{
    if (palette_ready) {
        return;
    }
    static const unsigned light[] = {256, 214, 174, 128};
    for (unsigned n = 0; n < 4; ++n) {
        for (unsigned i = 0; i < DUNGEON_PALETTE_SIZE; ++i) {
            shaded[n][i] = dim(dungeon_palette[i], light[n]);
        }
    }
    palette_ready = true;
}

static void fill(canvas_t *c, int x, int y, int w, int h, unsigned color)
{
    int right = x + w, bottom = y + h;
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (right > DUNGEON_W) {
        right = DUNGEON_W;
    }
    if (bottom > DUNGEON_H) {
        bottom = DUNGEON_H;
    }
    for (int yy = y; yy < bottom; ++yy) {
        for (int xx = x; xx < right; ++xx) {
            c->pixels[yy * DUNGEON_W + xx] = dungeon_palette[color];
        }
    }
}

static void plot(canvas_t *c, int x, int y, unsigned color)
{
    if ((unsigned)x < DUNGEON_W && (unsigned)y < VIEW_H) {
        c->pixels[y * DUNGEON_W + x] = dungeon_palette[color];
    }
}

static void line(canvas_t *c, int x0, int y0, int x1, int y1, unsigned color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1, error = dx + dy;
    for (;;) {
        plot(c, x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int twice = error * 2;
        if (twice >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

static void crop(canvas_t *c, enum dungeon_sprite_id id, int x, int y, int w, int h, unsigned shade,
                 int sx0, int sy0, int sw, int sh, float depth)
{
    if ((unsigned)id >= DSP_COUNT || w <= 0 || h <= 0) {
        return;
    }
    const dungeon_sprite_t *s = &dungeon_sprites[id];
    int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y, right = x + w > DUNGEON_W ? DUNGEON_W : x + w;
    int bottom = y + h > VIEW_H ? VIEW_H : y + h;
    for (int yy = top; yy < bottom; ++yy) {
        int sy = sy0 + (yy - y) * sh / h;
        for (int xx = left; xx < right; ++xx) {
            if (depth > 0 && depth > wall_depth[xx] + .03f) {
                continue;
            }
            unsigned p = s->pixels[sy * s->width + sx0 + (xx - x) * sw / w];
            /* An open arch really shows the raycast passage behind it. */
            if (p && (p != 1 || id != DSP_ARCH)) {
                c->pixels[yy * DUNGEON_W + xx] = shaded[shade][p];
            }
        }
    }
}

static void sprite(canvas_t *c, enum dungeon_sprite_id id, int x, int y, int w, int h,
                   unsigned shade)
{
    crop(c, id, x, y, w, h, shade, 0, 0, 64, 64, 0);
}

static enum dungeon_sprite_id wall_material(unsigned b)
{
    static const enum dungeon_sprite_id ids[] = {DSP_WALL_NEAR, DSP_MOSS_WALL, DSP_EMBER_WALL,
                                                 DSP_FROST_WALL};
    return ids[b % DUNGEON_BIOME_COUNT];
}

static enum dungeon_sprite_id floor_material(unsigned b)
{
    static const enum dungeon_sprite_id ids[] = {DSP_FLOOR_NEAR, DSP_MOSS_FLOOR, DSP_EMBER_FLOOR,
                                                 DSP_FROST_FLOOR};
    return ids[b % DUNGEON_BIOME_COUNT];
}

static void architecture(canvas_t *c, const dungeon_t *d)
{
    const uint8_t *wall = dungeon_sprites[wall_material(d->biome)].pixels;
    const uint8_t *floor = dungeon_sprites[floor_material(d->biome)].pixels;
    float plane_x = -d->dir_y * .9f, plane_y = d->dir_x * .9f;
    /* The two floor axes, ceiling and walls use the same moving/turning camera.
     * Texture coordinates are world coordinates, never phase-dependent scroll. */
    for (int y = 0; y < VIEW_H; ++y) {
        int delta = y > HORIZON ? y - HORIZON : HORIZON - y;
        if (!delta) {
            fill(c, 0, y, DUNGEON_W, 1, DEEP);
            continue;
        }
        float distance = (PROJECTION * .5f) / (float)delta;
        int u = (int)((d->camera_x + distance * (d->dir_x - plane_x)) * 65536.0f);
        int v = (int)((d->camera_y + distance * (d->dir_y - plane_y)) * 65536.0f);
        int du = (int)(distance * 2 * plane_x * 65536.0f / DUNGEON_W);
        int dv = (int)(distance * 2 * plane_y * 65536.0f / DUNGEON_W);
        unsigned shade = delta < 13 ? 3 : y < HORIZON ? 2 : 1;
        const uint8_t *tex = y > HORIZON ? floor : wall;
        for (int x = 0; x < DUNGEON_W; ++x, u += du, v += dv) {
            unsigned p = tex[((unsigned)(v >> 10) & 63u) * 64u + ((unsigned)(u >> 10) & 63u)];
            c->pixels[y * DUNGEON_W + x] = shaded[shade][p];
        }
    }
    for (int x = 0; x < DUNGEON_W; ++x) {
        float camera = 2.0f * x / DUNGEON_W - 1.0f;
        float rx = d->dir_x + plane_x * camera, ry = d->dir_y + plane_y * camera;
        float dx = fabsf(rx) < .0001f ? 10000.0f : 1.0f / fabsf(rx);
        float dy = fabsf(ry) < .0001f ? 10000.0f : 1.0f / fabsf(ry);
        int mx = (int)d->camera_x, my = (int)d->camera_y, sx = rx < 0 ? -1 : 1,
            sy = ry < 0 ? -1 : 1, side = 1;
        float side_x = (rx < 0 ? d->camera_x - mx : mx + 1.0f - d->camera_x) * dx;
        float side_y = (ry < 0 ? d->camera_y - my : my + 1.0f - d->camera_y) * dy;
        for (unsigned n = 0; n < 64; ++n) {
            if (side_x < side_y) {
                side_x += dx;
                mx += sx;
                side = 0;
            } else {
                side_y += dy;
                my += sy;
                side = 1;
            }
            if (mx < 0 || mx >= DUNGEON_MAP_SIZE || my < 0 || my >= DUNGEON_MAP_SIZE ||
                d->tiles[my][mx]) {
                break;
            }
        }
        float distance = side ? side_y - dy : side_x - dx;
        if (distance < .06f) {
            distance = .06f;
        }
        wall_depth[x] = distance;
        float hit = side ? d->camera_x + distance * rx : d->camera_y + distance * ry;
        int tx = (int)((hit - floorf(hit)) * 64);
        if (tx < 0) {
            tx = 0;
        }
        if (tx > 63) {
            tx = 63;
        }
        if ((!side && rx > 0) || (side && ry < 0)) {
            tx = 63 - tx;
        }
        int height = (int)(PROJECTION / distance), top = HORIZON - height / 2,
            bottom = top + height;
        int start = top < 0 ? 0 : top, end = bottom > VIEW_H ? VIEW_H : bottom;
        unsigned shade = distance < 2 ? 0 : distance < 3.5f ? 1 : distance < 5 ? 2 : 3;
        if (!side && shade < 3) {
            shade++;
        }
        int step = 65536 / height, tv = (start - top) * step;
        for (int y = start; y < end; ++y, tv += step) {
            c->pixels[y * DUNGEON_W + x] =
                shaded[shade][wall[((unsigned)(tv >> 10) & 63u) * 64u + (unsigned)tx]];
        }
    }
}

static bool projection(const dungeon_t *d, float wx, float wy, float height, float z, float *depth,
                       int *x, int *feet, int *size)
{
    float dx = wx - d->camera_x, dy = wy - d->camera_y;
    *depth = dx * d->dir_x + dy * d->dir_y;
    if (*depth < .2f || *depth > 24) {
        return false;
    }
    float lateral = dx * (-d->dir_y) + dy * d->dir_x;
    *x = 134 + (int)(lateral * (DUNGEON_W * .5f) / (.9f * (*depth)));
    *feet = HORIZON + (int)((.5f - z) * PROJECTION / (*depth));
    *size = (int)(height * PROJECTION / (*depth));
    return *size > 2 && *x + *size / 2 > 0 && *x - *size / 2 < DUNGEON_W;
}

static void shadow(canvas_t *c, int x, int y, int radius, float depth)
{
    if (radius > 150) {
        radius = 150;
    }
    for (int yy = -3; yy <= 3; ++yy) {
        for (int xx = -radius; xx <= radius; ++xx) {
            if (xx * xx * 9 + yy * yy * radius * radius > radius * radius * 9) {
                continue;
            }
            int px = x + xx, py = y + yy;
            if ((unsigned)px < DUNGEON_W && (unsigned)py < VIEW_H &&
                depth <= wall_depth[px] + .03f) {
                c->pixels[py * DUNGEON_W + px] = dim(c->pixels[py * DUNGEON_W + px], 110);
            }
        }
    }
}

static void light(canvas_t *c, int cx, int cy, int radius, unsigned biome, unsigned pulse)
{
    if (radius > 28) {
        radius = 28;
    }
    int limit = radius * radius;
    for (int y = cy - radius; y <= cy + radius; ++y) {
        for (int x = cx - radius; x <= cx + radius; ++x) {
            if ((unsigned)x >= DUNGEON_W || (unsigned)y >= VIEW_H) {
                continue;
            }
            int dx = x - cx, dy = y - cy, power = limit - dx * dx - dy * dy;
            if (power <= 0) {
                continue;
            }
            uint16_t p = c->pixels[y * DUNGEON_W + x];
            unsigned r =
                (p >> 11) + (unsigned)power * (biome == 3 ? 1u : 4u + pulse) / (unsigned)limit;
            unsigned g = ((p >> 5) & 63u) + (unsigned)power * 4u / (unsigned)limit;
            unsigned b = (p & 31u) + (biome == 3 ? (unsigned)power * 5u / (unsigned)limit : 0);
            if (r > 31) {
                r = 31;
            }
            if (g > 63) {
                g = 63;
            }
            if (b > 31) {
                b = 31;
            }
            c->pixels[y * DUNGEON_W + x] = (uint16_t)((r << 11) | (g << 5) | b);
        }
    }
}

static bool node_combat(const dungeon_node_t *n)
{
    return n->boss || n->room == DUNGEON_AMBUSH || n->room == DUNGEON_ELITE ||
           n->room == DUNGEON_CRYPT;
}

static enum dungeon_sprite_id objective_id(const dungeon_t *d, unsigned node)
{
    const dungeon_node_t *n = &d->nodes[node];
    bool open = n->resolved || (node == d->current_node && d->interaction);
    switch (n->room) {
    case DUNGEON_ARRIVAL:
        return open ? DSP_ARCH : DSP_DOOR;
    case DUNGEON_TREASURE:
        return open ? DSP_CHEST_OPEN : DSP_CHEST;
    case DUNGEON_SHRINE:
        return DSP_SHRINE;
    case DUNGEON_GROVE:
        return DSP_MUSHROOMS;
    case DUNGEON_LIBRARY:
        return DSP_BOOKS;
    case DUNGEON_ARMORY:
        return DSP_RACK;
    case DUNGEON_EXIT:
        return DSP_STAIRS;
    default:
        return DSP_BRAZIER;
    }
}

static void add_item(const dungeon_t *d, unsigned *count, unsigned node, unsigned kind,
                     enum dungeon_sprite_id id, float x, float y, float height, float z)
{
    if (*count >= DUNGEON_NODES * 5) {
        return;
    }
    float depth = (x - d->camera_x) * d->dir_x + (y - d->camera_y) * d->dir_y;
    if (depth < .2f || depth > 24) {
        return;
    }
    items[(*count)++] = (item_t){x, y, height, z, depth, node, kind, id};
}

static void creature(canvas_t *c, const dungeon_t *d, const item_t *item, int cx, int feet,
                     int size)
{
    const dungeon_node_t *n = &d->nodes[item->node];
    bool active = item->node == d->current_node && dungeon_is_combat(d);
    unsigned kind =
        n->boss ? (unsigned)d->boss % DUNGEON_BOSS_COUNT : n->enemy % DUNGEON_ENEMY_COUNT;
    unsigned contact = dungeon_contact_ms(d->weapon);
    bool impact = active && d->phase == 3 && d->phase_ms >= contact && d->phase_ms < contact + 75;
    unsigned frame = !n->hp                                ? 3
                     : active && d->phase == 1             ? 1
                     : active && (d->phase == 2 || impact) ? 2
                                                           : 0;
    if (active && d->phase == 2 && d->phase_ms < 200) {
        unsigned thrust = d->phase_ms < 100 ? d->phase_ms : 200u - d->phase_ms;
        size += (int)thrust / 14;
    }
    shadow(c, cx, feet, size / 3, item->depth);
    enum dungeon_sprite_id first = n->boss ? DSP_WARDEN_0 : DSP_SKELETON_0;
    crop(c, (enum dungeon_sprite_id)(first + kind * 4u + frame), cx - size / 2, feet - size, size,
         size, 0, 0, 0, 64, 64, item->depth);
    if (active && d->phase == 1) {
        int top = feet - size + 2;
        if (top < 5) {
            top = 5;
        }
        line(c, cx - 9, top, cx - 4, top + 4, GOLD);
        line(c, cx + 9, top, cx + 4, top + 4, GOLD);
    }
    if (active && d->phase == 2 && d->phase_ms < 240) {
        bool magic = n->boss ? (kind == 1 || kind == 2 || kind == 3 || kind == 4)
                             : (kind == 1 || kind == 3 || kind == 6);
        unsigned t = d->phase_ms;
        if (magic) {
            int s = 12 + (int)t / 18;
            sprite(c, n->boss && (kind == 1 || kind == 4) ? DSP_FIRE : DSP_BOLT, cx - s / 2,
                   feet - size / 2 + (int)t / 9, s, s, 0);
        } else if (t >= 90 && t < 165) {
            sprite(c, kind == 2 || kind == 5 ? DSP_CLAW : DSP_SLASH, cx - 23, feet - size / 2, 46,
                   37, 0);
        }
    }
    if (impact) {
        sprite(c, DSP_IMPACT, cx - 14, feet - size / 2 - 10, 28, 28, 0);
    }
    if (n->hp && (n->hits || n->boss) && item->depth < 3) {
        int width = n->boss ? 48 : 34;
        fill(c, cx - width / 2, feet + 2, width, 4, DEEP);
        fill(c, cx - width / 2 + 1, feet + 3, (width - 2) * (int)n->hp / (int)n->max_hp, 2, RED);
    }
}

static void world(canvas_t *c, const dungeon_t *d)
{
    unsigned count = 0;
    for (unsigned i = 0; i < DUNGEON_NODES; ++i) {
        const dungeon_node_t *n = &d->nodes[i];
        float x = n->x + .5f, y = n->y + .5f;
        bool enemy = node_combat(n);
        add_item(d, &count, i, enemy ? 1 : 0, objective_id(d, i), x, y,
                 n->boss                    ? 1.02f
                 : n->room == DUNGEON_ELITE ? .88f
                                            : .81f,
                 0);
        add_item(d, &count, i, 2, d->biome == 3 ? DSP_CRYSTALS : DSP_TORCH, x - 1.65f, y - 1.8f,
                 .53f, .36f);
        add_item(d, &count, i, 2, d->biome == 3 ? DSP_CRYSTALS : DSP_TORCH, x + 1.65f, y + 1.8f,
                 .53f, .36f);
        enum dungeon_sprite_id decor = d->biome == 1              ? DSP_MUSHROOMS
                                       : d->biome == 3            ? DSP_CRYSTALS
                                       : n->room == DUNGEON_CRYPT ? DSP_COFFIN
                                       : i % 2                    ? DSP_SKULLS
                                                                  : DSP_RUBBLE;
        add_item(d, &count, i, 0, decor, x - 1.65f, y + .7f, .57f, 0);
        if (n->room == DUNGEON_LIBRARY || n->room == DUNGEON_ARMORY) {
            add_item(d, &count, i, 0, n->room == DUNGEON_LIBRARY ? DSP_BOOKS : DSP_RACK, x + 1.8f,
                     y - 1.0f, .86f, 0);
        }
    }
    for (unsigned i = 1; i < count; ++i) {
        item_t value = items[i];
        unsigned j = i;
        while (j && items[j - 1].depth < value.depth) {
            items[j] = items[j - 1];
            j--;
        }
        items[j] = value;
    }
    for (unsigned i = 0; i < count; ++i) {
        item_t *item = &items[i];
        float depth;
        int cx, feet, size;
        if (!projection(d, item->x, item->y, item->height, item->z, &depth, &cx, &feet, &size)) {
            continue;
        }
        if (item->kind == 1) {
            creature(c, d, item, cx, feet, size);
        } else {
            unsigned shade = depth < 3 ? 0 : depth < 5 ? 1 : 2;
            if (item->kind == 2) {
                int column = cx < 0 ? 0 : cx >= DUNGEON_W ? DUNGEON_W - 1 : cx;
                if (depth < wall_depth[column] + .1f) {
                    light(c, cx, feet - size * 2 / 3, size / 2, d->biome,
                          (d->uptime_ms / 140u) % 3u);
                }
            } else if (!item->z) {
                shadow(c, cx, feet, size / 3, depth);
            }
            crop(c, item->id, cx - size / 2, feet - size, size, size, shade, 0, 0, 64, 64, depth);
            const dungeon_node_t *n = &d->nodes[item->node];
            if (item->node == d->current_node && d->state == DUNGEON_ROOM && d->phase == 3) {
                if (n->room == DUNGEON_TREASURE || n->room == DUNGEON_LIBRARY) {
                    sprite(c, DSP_HP_POTION, cx - 28, feet - size - 8, 27, 35, 0);
                    sprite(c, DSP_MP_POTION, cx + 2, feet - size - 8, 27, 35, 0);
                } else if (n->room == DUNGEON_SHRINE || n->room == DUNGEON_GROVE) {
                    sprite(c, DSP_RUNE, cx - 28, feet - 22, 56, 20, 0);
                }
            }
        }
    }
}

static void weapon(canvas_t *c, const dungeon_t *d)
{
    static const enum dungeon_sprite_id ready[] = {DSP_SWORD, DSP_STAFF,  DSP_AXE,
                                                   DSP_MACE,  DSP_DAGGER, DSP_CROSSBOW};
    static const enum dungeon_sprite_id attack[] = {DSP_SWORD_SWING,  DSP_STAFF_CAST,
                                                    DSP_AXE_SWING,    DSP_MACE_SWING,
                                                    DSP_DAGGER_SWING, DSP_CROSSBOW_FIRE};
    unsigned kind = d->weapon % DUNGEON_WEAPON_COUNT, t = d->phase_ms;
    int size = kind == 4 ? 96 : kind == 5 ? 108 : 103, x = kind == 5 ? 154 : 160, y = VIEW_H - size;
    enum dungeon_sprite_id pose = ready[kind];
    bool counter = dungeon_is_combat(d) && d->phase == 3;
    if (counter) {
        if (t < 100) {
            x += (int)t / 25;
            y += (int)t / 20;
        } else if (t < 180) {
            pose = attack[kind];
            x = kind == 1 || kind == 5 ? x - 9 : 155 - (int)(t - 100) * 25 / 80;
            if (kind == 3) {
                size += 5;
                y -= 3;
            }
            if (kind == 4) {
                x += 8;
                size += 5;
                y -= 2;
            }
        } else if (t < 320) {
            pose = attack[kind];
            x = kind == 1 || kind == 5 ? x - 5 : 130 + (int)(t - 180) * 30 / 140;
            y += (int)(320 - t) / 40;
        }
    } else if (d->state == DUNGEON_MOVE) {
        y += (int)((d->state_ms / 160u) % 3u);
        x += (int)((d->state_ms / 320u) & 1u);
    }
    if (d->state == DUNGEON_REST) {
        y += 48;
    }
    if (d->state == DUNGEON_FALL) {
        y += (int)d->state_ms / 25;
    }
    sprite(c, pose, x, y, size, size, 0);
    if (counter && t >= 105 && t < 170) {
        if (kind == 1 || kind == 5) {
            int progress = (int)t - 105, launch_x = kind == 1 ? 182 : 179,
                launch_y = kind == 1 ? 29 : 47;
            int px = launch_x - progress * (launch_x - 134) / 65,
                py = launch_y + progress * (63 - launch_y) / 65;
            if (kind == 1) {
                sprite(c, DSP_BOLT, px - 10, py - 9, 20, 18, 0);
            } else {
                line(c, px + 10, py + 5, px - 4, py - 1, WHITE);
                line(c, px + 12, py + 1, px + 8, py + 7, 33);
            }
        } else {
            sprite(c, kind == 3 ? DSP_IMPACT : DSP_SLASH, kind == 3 ? 117 : 109,
                   kind == 3 ? 47 : 39, kind == 3 ? 34 : 56, kind == 3 ? 34 : 46, 0);
        }
    }
    if (dungeon_is_combat(d) && d->phase == 2 && d->phase_ms < 240) {
        crop(c, DSP_HANDS, 43, 79, 36, 44, 0, 0, 0, 32, 64, 0);
    }
    if (d->state == DUNGEON_REST) {
        bool blue = d->rest_drunk ? d->restore_mask == 2 : !(d->hp < 60 && d->hp_potions);
        int rise = d->state_ms < 300    ? (int)(300 - d->state_ms) / 12
                   : d->state_ms > 1050 ? (int)(d->state_ms - 1050) / 10
                                        : 0;
        sprite(c, blue ? DSP_MP_POTION : DSP_HP_POTION, 86, 39 + rise, 47, 62, 0);
        crop(c, DSP_HANDS, 95, 80 + rise, 29, 39, 0, 0, 0, 32, 64, 0);
    }
}

static void particles(canvas_t *c, const dungeon_t *d)
{
    static const unsigned colors[] = {43, MAGIC, WHITE};
    for (unsigned i = 0; i < DUNGEON_PARTICLES; ++i) {
        const dungeon_particle_t *p = &d->particles[i];
        if (p->life) {
            plot(c, p->x * 2, p->y * 2, colors[(p->color - 1u) % 3u]);
        }
    }
}

static void hud(canvas_t *c, const dungeon_t *d)
{
    for (int n = 0; n < 2; ++n) {
        int x = 6 + n * 64;
        fill(c, x - 4, 107, 61, 13, STONE);
        fill(c, x - 2, 105, 57, 15, STONE);
        line(c, x - 1, 105, x + 53, 105, 7);
        line(c, x - 3, 107, x - 3, 119, 6);
        line(c, x - 1, 118, x + 54, 118, DEEP);
        fill(c, x + 12, 109, 38, 7, DEEP);
        int amount = 35 * (int)(n ? d->mp : d->hp) / 100;
        fill(c, x + 13, 110, amount, 4, n ? 27 : 12);
        fill(c, x + 13, 110, amount, 1, n ? 30 : 14);
        unsigned color = n ? MAGIC : RED;
        if (d->last_restore_ms && d->uptime_ms - d->last_restore_ms < 650 &&
            (d->restore_mask & (1u << n))) {
            color = WHITE;
        }
        line(c, x + 3, 108, x + 8, 112, color);
        line(c, x + 8, 112, x + 3, 116, color);
        line(c, x + 3, 116, x, 112, color);
        line(c, x, 112, x + 3, 108, color);
        plot(c, x + 3, 110, WHITE);
    }
    /* Safe inset: every progress diamond is wholly above y=115, even after 2x. */
    unsigned total = d->target_rooms > 8 ? 8 : d->target_rooms;
    for (unsigned n = 0; n < total; ++n) {
        int x = 132 + (int)n * 5;
        unsigned color = n < d->normal_rooms ? GOLD : 5;
        plot(c, x, 111, color);
        fill(c, x - 1, 112, 3, 2, color);
        plot(c, x, 114, color);
    }
    line(c, 0, 119, 267, 119, 3);
}

static void wipe(canvas_t *c, const dungeon_t *d)
{
    /* Shutters occur ONLY at entry/stairs, never between ordinary rooms. */
    unsigned time = d->state_ms > DUNGEON_DOOR_MS ? DUNGEON_DOOR_MS : d->state_ms;
    int w = d->state == DUNGEON_DOOR_OPEN ? 134 - (int)(time * 134u / DUNGEON_DOOR_MS)
                                          : (int)(time * 134u / DUNGEON_DOOR_MS);
    for (int side = 0; side < 2; ++side) {
        int offset = side ? DUNGEON_W - w : w - 134;
        for (int col = 0; col < 3; ++col) {
            sprite(c, wall_material(d->biome), offset + col * 45, 0, col == 2 ? 44 : 45, VIEW_H, 2);
        }
        for (int y = 18; y < VIEW_H; y += 37) {
            fill(c, offset, y, 134, 3, DEEP);
            line(c, offset, y, offset + 133, y, 6);
        }
        line(c, side ? offset : offset + 133, 0, side ? offset : offset + 133, VIEW_H - 1, GOLD);
    }
}

void dungeon_paint(const dungeon_t *d, uint16_t pixels[DUNGEON_W * DUNGEON_H])
{
    canvas_t c = {pixels};
    prepare_palette();
    architecture(&c, d);
    world(&c, d);
    if (d->state == DUNGEON_TITLE) {
        for (unsigned i = 0; i < DUNGEON_W * DUNGEON_H; ++i) {
            pixels[i] = dim(pixels[i], 150);
        }
        sprite(&c, DSP_TITLE, 104, 24, 60, 72, 0);
        return;
    }
    weapon(&c, d);
    particles(&c, d);
    hud(&c, d);
    if (d->last_hurt_ms && d->uptime_ms - d->last_hurt_ms < 110) {
        fill(&c, 0, 0, 2, VIEW_H, 11);
        fill(&c, DUNGEON_W - 2, 0, 2, VIEW_H, 11);
    }
    if (d->state == DUNGEON_DOOR_OPEN || d->state == DUNGEON_DOOR_CLOSE) {
        wipe(&c, d);
    }
    if (d->state == DUNGEON_FALL) {
        unsigned amount = d->state_ms > 1400 ? 35u : 256u - d->state_ms * 220u / 1400u;
        for (unsigned i = 0; i < DUNGEON_W * DUNGEON_H; ++i) {
            pixels[i] = dim(pixels[i], amount);
        }
        sprite(&c, DSP_SKULLS, 99, 33, 70, 50, 1);
    }
}

bool dungeon_expand_strip(const uint16_t *px, int y, int rows, uint16_t *out)
{
    if (!px || !out || y < 0 || rows <= 0 || y > DISPLAY_H - rows || y % DUNGEON_SCALE ||
        rows % DUNGEON_SCALE) {
        return false;
    }
    for (int row = 0; row < rows; row += DUNGEON_SCALE) {
        uint16_t *dst = out + row * DISPLAY_W;
        const uint16_t *src = px + ((y + row) / DUNGEON_SCALE) * DUNGEON_W;
        for (int x = 0; x < DUNGEON_W; ++x) {
            for (int scale = 0; scale < DUNGEON_SCALE; ++scale) {
                dst[x * DUNGEON_SCALE + scale] = (uint16_t)((src[x] << 8) | (src[x] >> 8));
            }
        }
        for (int scale = 1; scale < DUNGEON_SCALE; ++scale) {
            memcpy(dst + scale * DISPLAY_W, dst, DISPLAY_W * sizeof(*dst));
        }
    }
    return true;
}
