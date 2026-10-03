// SPDX-License-Identifier: GPL-3.0-only
#include "../main/dungeon.h"
#include "../main/paint.h"

#include <stdio.h>
#include <string.h>

enum {
    SCALE = DUNGEON_SCALE,
    COLUMNS = 5,
    CARDS = 185,
    ROWS = CARDS / COLUMNS,
    ANIMATION_FRAMES = 1536
};

static void advance(dungeon_t *d, unsigned ms)
{
    while (ms) {
        unsigned step = ms > 17 ? 17 : ms;
        dungeon_step(d, step);
        ms -= step;
    }
}

static void rgb_pixel(uint16_t p)
{
    unsigned char rgb[3] = {(unsigned char)((p >> 8) & 0xf8u), (unsigned char)((p >> 3) & 0xfcu),
                            (unsigned char)((p << 3) & 0xf8u)};
    fwrite(rgb, sizeof(rgb), 1, stdout);
}

int main(int argc, char **argv)
{
    static uint16_t frames[CARDS][DUNGEON_W * DUNGEON_H];
    if (argc > 1 && (strcmp(argv[1], "--animate") == 0 || strcmp(argv[1], "--combat") == 0)) {
        dungeon_t d;
        dungeon_init(&d, 0x51ed270b);
        bool combat = strcmp(argv[1], "--combat") == 0;
        if (combat) {
            dungeon_preview_scene(&d, DUNGEON_ELITE, 4, 0);
            d.weapon = 0;
            d.hp = 76;
            d.mp = 58;
            d.nodes[0].hp = d.nodes[0].max_hp = 98;
        }
        unsigned total = combat ? 80u : ANIMATION_FRAMES;
        printf("P6\n%d %d\n255\n", DUNGEON_W, total * DUNGEON_H);
        for (unsigned n = 0; n < total; ++n) {
            fprintf(stderr, "%u,%s,%s,%s,%u,%u,%u,%u,%u,%u,%u\n", n, dungeon_state_name(d.state),
                    dungeon_room_name(d.room), dungeon_boss_name(d.boss), d.phase, d.biome,
                    d.scene_boss, d.hp, d.mp, d.nodes[d.current_node].hp,
                    d.nodes[d.current_node].hits);
            dungeon_paint(&d, frames[0]);
            for (unsigned i = 0; i < DUNGEON_W * DUNGEON_H; ++i) {
                rgb_pixel(frames[0][i]);
            }
            advance(&d, 125);
        }
        return 0;
    }
    for (unsigned card = 0; card < CARDS; ++card) {
        dungeon_t d;
        dungeon_init(&d, card + 1);
        d.state = DUNGEON_ROOM;
        d.room_ms = 3400;
        d.run = 1;
        d.target_rooms = 6;
        d.normal_rooms = 3;
        unsigned group = card / 5u;
        d.room = card < 60 ? (dungeon_room_t)group : DUNGEON_AMBUSH;
        d.enemy = (uint8_t)((card < 60 ? group : (card - 60) / 5u) % DUNGEON_ENEMY_COUNT);
        d.layout = (uint8_t)(group % DUNGEON_LAYOUT_COUNT);
        d.biome = (uint8_t)(group % DUNGEON_BIOME_COUNT);
        d.phase = card % 5u;
        d.phase_ms = 140;
        d.scene_ms = 510 + d.phase * 620;
        d.state_ms = d.scene_ms;
        d.uptime_ms = 4200;
        d.catacomb_set = group % 3u;
        d.weapon = group % DUNGEON_WEAPON_COUNT;
        if (card >= 100 && card < 130) {
            d.state = DUNGEON_BOSS;
            d.scene_boss = 1;
            d.boss = (dungeon_boss_t)((card - 100) / 5u);
            d.biome = d.boss % DUNGEON_BIOME_COUNT;
            d.layout = d.boss & 1u ? 5 : 2;
            d.strike = d.phase >= 3 ? 2 : 0;
        } else if (card >= 130 && card < 154) {
            unsigned variant = card - 130;
            d.biome = variant / 6u;
            d.layout = variant % 6u;
            d.room = DUNGEON_CORRIDOR;
            d.phase = 0;
        } else if (card >= 154 && card < 178) {
            static const unsigned times[] = {80, 140, 250, 330};
            d.weapon = (card - 154) / 4u;
            d.phase = 3;
            d.phase_ms = times[(card - 154) % 4u];
            d.biome = d.weapon % 4u;
            d.enemy = 0;
            d.layout = 2;
        } else if (card == 178) {
            d.state = DUNGEON_TITLE;
        } else if (card >= 179) {
            d.state = card < 182 ? DUNGEON_DOOR_OPEN : DUNGEON_DOOR_CLOSE;
            d.state_ms = ((card - 179) % 3u) * 215u;
            d.room = DUNGEON_AMBUSH;
            d.enemy = 0;
            d.biome = 0;
            d.layout = 0;
            d.phase = card < 182 ? 0 : 4;
            d.scene_ms = card < 182 ? 0 : 3400;
            d.phase_ms = card < 182 ? 0 : 700;
            d.weapon = 0;
        }
        dungeon_state_t state = d.state;
        dungeon_preview_scene(&d, d.room, d.scene_boss ? d.boss : d.enemy, d.scene_boss);
        d.state = state;
        if (d.phase == 4) {
            d.nodes[0].hp = 0;
        }
        if (d.phase == 3) {
            d.nodes[0].hits = 2;
            d.nodes[0].hp = d.nodes[0].max_hp / 2;
            d.hp = 72;
            d.mp = d.weapon == 1 ? 68 : 91;
        }
        if (card >= 130 && card < 154) {
            static const uint8_t nodes[] = {0, 1, 3, 2, 4, 8};
            unsigned view = (card - 130) % 6u, node = nodes[view];
            d.room = DUNGEON_CORRIDOR;
            d.nodes[0].room = DUNGEON_ARRIVAL;
            d.current_node = node;
            d.state = DUNGEON_MOVE;
            d.camera_x = d.nodes[node].x + .5f;
            d.camera_y = d.nodes[node].y + .5f + 1.5f;
            d.dir_x = 0;
            d.dir_y = -1;
            if (view == 3) {
                d.dir_x = -1;
                d.dir_y = 0;
                d.camera_x = d.nodes[node].x + .5f;
                d.camera_y = d.nodes[node].y + .5f;
            }
        }
        dungeon_paint(&d, frames[card]);
    }
    printf("P6\n%d %d\n255\n", COLUMNS * DUNGEON_W * SCALE, ROWS * DUNGEON_H * SCALE);
    for (int y = 0; y < ROWS * DUNGEON_H * SCALE; ++y) {
        for (int x = 0; x < COLUMNS * DUNGEON_W * SCALE; ++x) {
            unsigned panel =
                (unsigned)(x / (DUNGEON_W * SCALE)) + COLUMNS * (unsigned)(y / (DUNGEON_H * SCALE));
            uint16_t p =
                frames[panel][((y / SCALE) % DUNGEON_H) * DUNGEON_W + ((x / SCALE) % DUNGEON_W)];
            rgb_pixel(p);
        }
    }
    return 0;
}
