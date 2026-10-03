// SPDX-License-Identifier: GPL-3.0-only
#include "../main/dungeon.h"
#include "../main/paint.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void advance(dungeon_t *d, unsigned ms)
{
    while (ms) {
        unsigned step = ms > 17 ? 17 : ms;
        dungeon_step(d, step);
        ms -= step;
    }
}

static uint32_t hash(const uint16_t *frame)
{
    uint32_t h = 2166136261u;
    for (unsigned i = 0; i < DUNGEON_W * DUNGEON_H; ++i) {
        h = (h ^ frame[i]) * 16777619u;
    }
    return h;
}

static void encounter(dungeon_t *d, unsigned kind, int boss, unsigned weapon)
{
    dungeon_init(d, 17);
    dungeon_preview_scene(d, DUNGEON_AMBUSH, kind, boss);
    d->weapon = weapon;
    d->hp = 100;
    d->mp = 100;
    d->target_rooms = 8;
    dungeon_node_t *n = &d->nodes[0];
    n->max_hp = n->hp = (uint16_t)(boss ? 190 + kind * 6 : 62 + kind * 3);
}

int main(void)
{
    dungeon_t a, b;
    dungeon_init(&a, 0x12345678u);
    dungeon_init(&b, 0x12345678u);
    unsigned enemies = 0, bosses = 0, weapons = 0, biomes = 0, walks = 0, turns = 0, backtracks = 0,
             cleared_revisits = 0;
    uint8_t seen[DUNGEON_NODES] = {0};
    for (unsigned elapsed = 0; elapsed < 2400000; elapsed += 17) {
        dungeon_state_t old = a.state;
        float yaw = a.yaw;
        unsigned run = a.run, hp = a.hp, mp = a.mp;
        dungeon_step(&a, 17);
        dungeon_step(&b, 17);
        assert(memcmp(&a, &b, sizeof(a)) == 0);
        assert(dungeon_walkable(&a, a.camera_x, a.camera_y));
        assert(fabsf(a.dir_x * a.dir_x + a.dir_y * a.dir_y - 1) < .001f);
        assert(a.hp <= 100 && a.mp <= 100 && a.hp_potions <= 4 && a.mp_potions <= 4);
        assert(a.current_node < DUNGEON_NODES && a.goal_node < DUNGEON_NODES && a.phase < 5);
        assert(a.particle_count <= DUNGEON_PARTICLES);
        for (unsigned i = 0; i < DUNGEON_PARTICLES; ++i) {
            assert(a.particles[i].life <= 14);
        }
        if (run != a.run) {
            memset(seen, 0, sizeof(seen));
        }
        if (old != a.state && a.state == DUNGEON_MOVE) {
            walks++;
        }
        if (old != a.state && a.state == DUNGEON_TURN) {
            turns++;
            if (fabsf(a.to_yaw - yaw) > 2) {
                backtracks++;
            }
        }
        if (a.state == DUNGEON_MOVE || a.state == DUNGEON_TURN) {
            /* No resource refill at a room change, movement, or a turn. */
            assert(a.hp == hp && a.mp == mp);
        }
        if (a.hp > hp || a.mp > mp) {
            assert(a.state == DUNGEON_REST || old == DUNGEON_FALL ||
                   (a.state == DUNGEON_ROOM && a.phase == 3 &&
                    (a.room == DUNGEON_SHRINE || a.room == DUNGEON_GROVE)));
        }
        for (unsigned i = 0; i < DUNGEON_NODES; ++i) {
            const dungeon_node_t *n = &a.nodes[i];
            assert(n->hp <= n->max_hp);
            if (n->max_hp && !n->hp) {
                assert(n->hits >= 3);
            }
            if (n->resolved && n->max_hp) {
                assert(!n->hp);
            }
            if (n->resolved && !seen[i]) {
                seen[i] = 1;
            }
        }
        if (a.state == DUNGEON_TURN && a.nodes[a.current_node].resolved && a.route_cursor > 3) {
            cleared_revisits++;
        }
        biomes |= 1u << a.biome;
        weapons |= 1u << a.weapon;
        if (dungeon_is_combat(&a)) {
            if (a.scene_boss) {
                bosses |= 1u << a.boss;
            } else {
                enemies |= 1u << a.enemy;
            }
        }
    }
    assert(enemies == (1u << DUNGEON_ENEMY_COUNT) - 1u &&
           bosses == (1u << DUNGEON_BOSS_COUNT) - 1u);
    assert(weapons == (1u << DUNGEON_WEAPON_COUNT) - 1u &&
           biomes == (1u << DUNGEON_BIOME_COUNT) - 1u);
    assert(walks > 200 && turns > 150 && backtracks > 60 && cleared_revisits > 100);

    dungeon_init(&a, 91);
    b = a;
    dungeon_step(&a, 45000);
    advance(&b, 45000);
    assert(a.state == b.state && a.state_ms == b.state_ms && a.phase_ms == b.phase_ms &&
           a.rng == b.rng);
    assert(a.hp == b.hp && a.mp == b.mp && a.route_cursor == b.route_cursor);
    assert(memcmp(a.nodes, b.nodes, sizeof(a.nodes)) == 0);
    assert(fabsf(a.camera_x - b.camera_x) < .0001f && fabsf(a.camera_y - b.camera_y) < .0001f);

    /* Every weapon must land several actual damage events, not one timer-based
     * kill. HP persists through telegraph, hit, recovery and next exchange. */
    for (unsigned boss = 0; boss < 2; ++boss) {
        for (unsigned kind = 0; kind < (boss ? DUNGEON_BOSS_COUNT : DUNGEON_ENEMY_COUNT); ++kind) {
            for (unsigned gear = 0; gear < DUNGEON_WEAPON_COUNT; ++gear) {
                encounter(&a, kind, boss, gear);
                unsigned last_hp = 100, last_mp = 100, last_hits = 0;
                for (unsigned ms = 0; ms < 20000 && a.phase != 4; ms += 17) {
                    unsigned enemy_hp = a.nodes[0].hp;
                    dungeon_step(&a, 17);
                    assert(a.hp <= last_hp && a.mp <= last_mp);
                    if (a.nodes[0].hp < enemy_hp) {
                        assert(a.phase == 3 && a.player_contact &&
                               a.nodes[0].hits == last_hits + 1);
                        last_hits = a.nodes[0].hits;
                    }
                    last_hp = a.hp;
                    last_mp = a.mp;
                }
                assert(a.phase == 4 && !a.nodes[0].hp);
                assert(a.nodes[0].hits >= (boss ? 7u : 3u));
                assert(a.mp == (gear == 1 ? 100 - a.nodes[0].hits * 8u : 100));
                unsigned hits = a.nodes[0].hits;
                advance(&a, 500);
                assert(a.phase == 4 && !a.nodes[0].hp && a.nodes[0].hits == hits);
            }
        }
    }

    /* Potion recovery occurs once at the visible drink contact; movement does
     * not reset resources. An exhausted staff falls back to steel. */
    dungeon_init(&a, 12);
    a.state = DUNGEON_REST;
    a.hp = 38;
    a.mp = 75;
    a.hp_potions = 2;
    advance(&a, 649);
    assert(a.hp == 38 && a.hp_potions == 2);
    dungeon_step(&a, 1);
    assert(a.hp == 73 && a.mp == 75 && a.hp_potions == 1);
    advance(&a, 700);
    assert(a.hp == 73 && a.hp_potions == 1);
    encounter(&a, 0, 0, 1);
    a.mp = 0;
    advance(&a, 1050);
    assert(a.weapon == 0 && a.mp == 0);

    /* An opened/used shrine cannot heal again on a later visit. */
    dungeon_init(&a, 22);
    dungeon_preview_scene(&a, DUNGEON_SHRINE, 0, 0);
    a.hp = 50;
    a.mp = 40;
    advance(&a, 850);
    assert(a.hp == 72 && a.mp == 74);
    advance(&a, 1200);
    assert(a.nodes[0].resolved);
    unsigned saved_hp = a.hp, saved_mp = a.mp;
    a.state = DUNGEON_ROOM;
    a.phase = 3;
    a.phase_ms = 0;
    a.interaction = 0;
    advance(&a, 600);
    assert(a.hp == saved_hp && a.mp == saved_mp);

    static uint16_t frame[DUNGEON_W * DUNGEON_H], other[DUNGEON_W * DUNGEON_H],
        strip[DISPLAY_W * 40];
    /* A living multi-hit enemy remains distinct from its floor corpse. The
     * corpse is attached to its node, not resurrected by walking/turning. */
    encounter(&a, 0, 0, 0);
    dungeon_paint(&a, frame);
    advance(&a, 5000);
    assert(!a.nodes[0].hp);
    dungeon_paint(&a, other);
    assert(hash(frame) != hash(other));
    unsigned dead_hits = a.nodes[0].hits;
    advance(&a, 17000);
    assert(!a.nodes[0].hp && a.nodes[0].hits == dead_hits);

    /* Identical camera, different combat clock: walls/floor do not shear.
     * Walking changes the real projection; 90-degree turns change it again. */
    dungeon_init(&a, 17);
    dungeon_preview_scene(&a, DUNGEON_CORRIDOR, 0, 0);
    a.uptime_ms = 4200;
    a.phase = 0;
    dungeon_paint(&a, frame);
    a.phase = 1;
    a.scene_ms = 530;
    dungeon_paint(&a, other);
    assert(memcmp(frame, other, sizeof(frame)) == 0);
    a.camera_x += .3f;
    dungeon_paint(&a, other);
    assert(hash(frame) != hash(other));
    a.dir_x = 0;
    a.dir_y = -1;
    dungeon_paint(&a, frame);
    assert(hash(frame) != hash(other));

    uint32_t variants[8];
    for (unsigned biome = 0; biome < DUNGEON_BIOME_COUNT; ++biome) {
        dungeon_init(&a, 17);
        a.state = DUNGEON_MOVE;
        a.biome = biome;
        dungeon_paint(&a, frame);
        variants[biome] = hash(frame);
        for (unsigned n = 0; n < biome; ++n) {
            assert(variants[n] != variants[biome]);
        }
    }
    for (unsigned gear = 0; gear < DUNGEON_WEAPON_COUNT; ++gear) {
        encounter(&a, 0, 0, gear);
        a.phase = 3;
        unsigned times[] = {80, 140, 250, 370};
        for (unsigned pose = 0; pose < 4; ++pose) {
            a.phase_ms = times[pose];
            dungeon_paint(&a, frame);
            variants[pose] = hash(frame);
            for (unsigned n = 0; n < pose; ++n) {
                assert(variants[n] != variants[pose]);
            }
        }
    }
    /* Progress diamonds have six physical pixels of safe bottom padding. */
    dungeon_init(&a, 17);
    a.state = DUNGEON_MOVE;
    a.target_rooms = 8;
    a.normal_rooms = 8;
    dungeon_paint(&a, frame);
    for (unsigned n = 0; n < 8; ++n) {
        int x = 132 + (int)n * 5;
        assert(frame[111 * DUNGEON_W + x] == frame[114 * DUNGEON_W + x]);
        assert(frame[114 * DUNGEON_W + x] != frame[117 * DUNGEON_W + x]);
    }

    static uint16_t guarded[DUNGEON_W * DUNGEON_H + 32];
    for (unsigned i = 0; i < sizeof(guarded) / sizeof(guarded[0]); ++i) {
        guarded[i] = 0xa55a;
    }
    unsigned cases = 0;
    for (unsigned state = 0; state < DUNGEON_STATE_COUNT; ++state) {
        for (unsigned room = 0; room < DUNGEON_ROOM_COUNT; ++room) {
            for (unsigned phase = 0; phase < 5; ++phase) {
                for (unsigned gear = 0; gear < DUNGEON_WEAPON_COUNT; ++gear) {
                    for (unsigned actor = 0; actor < DUNGEON_ENEMY_COUNT; ++actor) {
                        encounter(&a, actor, state == DUNGEON_BOSS, gear);
                        a.state = (dungeon_state_t)state;
                        a.room = (dungeon_room_t)room;
                        a.nodes[0].room = room;
                        a.phase = phase;
                        a.phase_ms = 140;
                        a.state_ms = phase * 107;
                        a.uptime_ms = 4200;
                        a.biome = actor % DUNGEON_BIOME_COUNT;
                        a.hp = actor * 13;
                        a.mp = 100 - actor * 11;
                        if (phase == 4) {
                            a.nodes[0].hp = 0;
                        }
                        dungeon_paint(&a, guarded + 16);
                        for (unsigned i = 0; i < 16; ++i) {
                            assert(guarded[i] == 0xa55a);
                            assert(guarded[16 + DUNGEON_W * DUNGEON_H + i] == 0xa55a);
                        }
                        cases++;
                    }
                }
            }
        }
    }
    assert(cases == 25920);
    assert(!dungeon_expand_strip(frame, 1, 40, strip));
    assert(!dungeon_expand_strip(frame, 0, 39, strip));
    assert(!dungeon_expand_strip(NULL, 0, 40, strip));
    assert(!dungeon_expand_strip(frame, 0, 40, NULL));
    assert(!dungeon_expand_strip(frame, -2, 40, strip));
    assert(!dungeon_expand_strip(frame, DISPLAY_H - 20, 40, strip));
    for (unsigned i = 0; i < DUNGEON_W * DUNGEON_H; ++i) {
        frame[i] = (uint16_t)(i * 37u);
    }
    for (int y = 0; y < DISPLAY_H; y += 40) {
        assert(dungeon_expand_strip(frame, y, 40, strip));
        for (int yy = 0; yy < 40; ++yy) {
            for (int x = 0; x < DISPLAY_W; ++x) {
                uint16_t src = frame[((y + yy) / DUNGEON_SCALE) * DUNGEON_W + x / DUNGEON_SCALE];
                assert(strip[yy * DISPLAY_W + x] == (uint16_t)((src << 8) | (src >> 8)));
            }
        }
    }
    printf("connected exploration, backtracking, persistent resources/corpses, multi-hit combat "
           "and %u guarded renders passed\n",
           cases);
    return 0;
}
