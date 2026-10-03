// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <stdint.h>

enum {
    DUNGEON_W = 268,
    DUNGEON_H = 120,
    DUNGEON_SCALE = 2,
    DISPLAY_W = DUNGEON_W * DUNGEON_SCALE,
    DISPLAY_H = DUNGEON_H * DUNGEON_SCALE,
    DUNGEON_PARTICLES = 24,
    DUNGEON_LAYOUT_COUNT = 6,
    DUNGEON_BIOME_COUNT = 4,
    DUNGEON_ENEMY_COUNT = 8,
    DUNGEON_WEAPON_COUNT = 6,
    DUNGEON_DOOR_MS = 430,
    DUNGEON_BOSS_MS = 6500,
    DUNGEON_MAP_SIZE = 19,
    DUNGEON_NODES = 9
};

typedef enum {
    DUNGEON_TITLE,
    DUNGEON_DOOR_OPEN,
    DUNGEON_ROOM,
    DUNGEON_BOSS,
    DUNGEON_DOOR_CLOSE,
    DUNGEON_MOVE,
    DUNGEON_TURN,
    DUNGEON_REST,
    DUNGEON_FALL,
    DUNGEON_STATE_COUNT
} dungeon_state_t;

typedef enum {
    DUNGEON_ARRIVAL,
    DUNGEON_CORRIDOR,
    DUNGEON_TREASURE,
    DUNGEON_AMBUSH,
    DUNGEON_ELITE,
    DUNGEON_SHRINE,
    DUNGEON_EXIT,
    DUNGEON_JUNCTION,
    DUNGEON_CRYPT,
    DUNGEON_LIBRARY,
    DUNGEON_ARMORY,
    DUNGEON_GROVE,
    DUNGEON_ROOM_COUNT
} dungeon_room_t;

typedef enum {
    DUNGEON_WARDEN,
    DUNGEON_WYRM,
    DUNGEON_EYE,
    DUNGEON_LICH,
    DUNGEON_HYDRA,
    DUNGEON_MINOTAUR,
    DUNGEON_BOSS_COUNT
} dungeon_boss_t;

typedef struct {
    int8_t x, y, dx, dy;
    uint8_t life, color;
} dungeon_particle_t;

typedef struct {
    uint8_t x, y, room, enemy, boss, visited, resolved, hits;
    uint16_t hp, max_hp;
} dungeon_node_t;

typedef struct {
    uint32_t rng, uptime_ms, state_ms, room_ms, run, normal_rooms, target_rooms;
    dungeon_state_t state;
    dungeon_room_t room;
    dungeon_boss_t boss;
    /* Named visual selections keep simulation independent from renderer art. */
    uint8_t layout, entry, catacomb_set, enemy, weapon, phase, particle_count;
    uint8_t biome, scene_boss, strike;
    /* Freeze the completed scene through the closing shutter. */
    uint32_t scene_ms, phase_ms;
    /* One connected world persists until the descent reaches its stairs. */
    uint8_t tiles[DUNGEON_MAP_SIZE][DUNGEON_MAP_SIZE];
    dungeon_node_t nodes[DUNGEON_NODES];
    float camera_x, camera_y, dir_x, dir_y, yaw, from_x, from_y, to_x, to_y, from_yaw, to_yaw;
    uint32_t motion_ms, last_hit_ms, last_hurt_ms, last_restore_ms;
    uint8_t current_node, goal_node, route_cursor, move_to_center;
    uint8_t hp, mp, hp_potions, mp_potions, round, enemy_contact, player_contact, launched;
    uint8_t rest_return, rest_drunk, restore_mask, interaction;
    int8_t hero_x, hero_y, focal_x, focal_y;
    dungeon_particle_t particles[DUNGEON_PARTICLES];
    uint32_t particle_ms;
} dungeon_t;

void dungeon_init(dungeon_t *dungeon, uint32_t seed);
void dungeon_step(dungeon_t *dungeon, uint32_t dt_ms);
const char *dungeon_state_name(dungeon_state_t state);
const char *dungeon_room_name(dungeon_room_t room);
const char *dungeon_boss_name(dungeon_boss_t boss);
const char *dungeon_biome_name(unsigned biome);
int dungeon_is_combat(const dungeon_t *dungeon);
unsigned dungeon_contact_ms(unsigned weapon);
int dungeon_walkable(const dungeon_t *dungeon, float x, float y);
void dungeon_preview_scene(dungeon_t *dungeon, dungeon_room_t room, unsigned kind, int boss);
