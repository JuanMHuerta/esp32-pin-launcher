#pragma once

#include <stdint.h>

enum { DUNGEON_W = 134, DUNGEON_H = 60, DUNGEON_SCALE = 4,
       DISPLAY_W = DUNGEON_W * DUNGEON_SCALE, DISPLAY_H = DUNGEON_H * DUNGEON_SCALE,
       DUNGEON_PARTICLES = 24 };

typedef enum {
    DUNGEON_TITLE, DUNGEON_DOOR_OPEN, DUNGEON_ROOM, DUNGEON_BOSS,
    DUNGEON_DOOR_CLOSE, DUNGEON_STATE_COUNT
} dungeon_state_t;

typedef enum {
    DUNGEON_ARRIVAL, DUNGEON_CORRIDOR, DUNGEON_TREASURE, DUNGEON_AMBUSH,
    DUNGEON_ELITE, DUNGEON_SHRINE, DUNGEON_EXIT, DUNGEON_ROOM_COUNT
} dungeon_room_t;

typedef enum { DUNGEON_WARDEN, DUNGEON_WYRM, DUNGEON_EYE, DUNGEON_BOSS_COUNT } dungeon_boss_t;

typedef struct {
    int8_t x, y, dx, dy;
    uint8_t life, color;
} dungeon_particle_t;

typedef struct {
    uint32_t rng, uptime_ms, state_ms, room_ms, run, normal_rooms, target_rooms;
    dungeon_state_t state;
    dungeon_room_t room;
    dungeon_boss_t boss;
    /* Named visual selections keep simulation independent from renderer art. */
    uint8_t layout, entry, catacomb_set, enemy, weapon, phase, particle_count;
    int8_t hero_x, hero_y, focal_x, focal_y;
    dungeon_particle_t particles[DUNGEON_PARTICLES];
} dungeon_t;

void dungeon_init(dungeon_t *dungeon, uint32_t seed);
void dungeon_step(dungeon_t *dungeon, uint32_t dt_ms);
const char *dungeon_state_name(dungeon_state_t state);
const char *dungeon_room_name(dungeon_room_t room);
const char *dungeon_boss_name(dungeon_boss_t boss);
