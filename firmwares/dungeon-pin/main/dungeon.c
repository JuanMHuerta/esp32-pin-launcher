#include "dungeon.h"

#include <string.h>

enum { TITLE_MS = 1900, DOOR_MS = 430, BOSS_MS = 6500 };

static uint32_t random_next(dungeon_t *d)
{
    uint32_t x = d->rng ? d->rng : 0x6d2b79f5u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    d->rng = x;
    return x;
}

static void particles_clear(dungeon_t *d)
{
    memset(d->particles, 0, sizeof(d->particles));
    d->particle_count = 0;
}

static void particles_burst(dungeon_t *d, int x, int y, unsigned colors)
{
    particles_clear(d);
    for (unsigned i = 0; i < DUNGEON_PARTICLES; ++i) {
        uint32_t r = random_next(d);
        dungeon_particle_t *p = &d->particles[i];
        p->x = (int8_t)x;
        p->y = (int8_t)y;
        p->dx = (int8_t)((int)(r & 7u) - 3);
        p->dy = (int8_t)(-1 - (int)((r >> 3) & 3u));
        p->life = (uint8_t)(7 + ((r >> 6) & 7u));
        p->color = (uint8_t)(1 + ((r >> 9) % colors));
        d->particle_count++;
    }
}

static void particles_step(dungeon_t *d, uint32_t dt_ms)
{
    if (!d->particle_count) return;
    unsigned ticks = dt_ms / 33u;
    if (!ticks) return;
    d->particle_count = 0;
    for (unsigned i = 0; i < DUNGEON_PARTICLES; ++i) {
        dungeon_particle_t *p = &d->particles[i];
        if (!p->life) continue;
        for (unsigned n = 0; n < ticks && p->life; ++n) {
            p->x = (int8_t)(p->x + p->dx);
            p->y = (int8_t)(p->y + p->dy);
            p->dy++;
            p->life--;
        }
        if (p->life) d->particle_count++;
    }
}

static void enter(dungeon_t *d, dungeon_state_t state)
{
    d->state = state;
    d->state_ms = 0;
    d->phase = 0;
}

static void choose_room(dungeon_t *d)
{
    dungeon_room_t previous = d->room;
    d->room = (dungeon_room_t)(random_next(d) % DUNGEON_ROOM_COUNT);
    if (d->normal_rooms && d->room == previous)
        d->room = (dungeon_room_t)((d->room + 1 + random_next(d) % (DUNGEON_ROOM_COUNT - 1)) % DUNGEON_ROOM_COUNT);
    d->layout = (uint8_t)(random_next(d) % 3u);
    d->entry = (uint8_t)(random_next(d) & 1u);
    d->catacomb_set = (uint8_t)(random_next(d) % 3u);
    d->enemy = (uint8_t)(random_next(d) % 4u);
    d->weapon = (uint8_t)(random_next(d) & 1u);
    d->room_ms = 2500u + random_next(d) % 1100u;
    d->focal_x = (int8_t)(d->layout == 0 ? 98 : d->layout == 1 ? 67 : 35);
    d->focal_y = 39;
    d->hero_x = d->entry ? 8 : 118;
    d->hero_y = 42;
    d->normal_rooms++;
}

static void begin_run(dungeon_t *d)
{
    d->run++;
    d->normal_rooms = 0;
    d->target_rooms = 4u + random_next(d) % 3u;
    choose_room(d);
    enter(d, DUNGEON_DOOR_OPEN);
}

static void choose_boss(dungeon_t *d)
{
    d->boss = (dungeon_boss_t)(random_next(d) % DUNGEON_BOSS_COUNT);
    d->layout = (uint8_t)(random_next(d) % 3u);
    d->focal_x = 89;
    d->focal_y = 35;
    d->hero_x = 38;
    d->hero_y = 43;
    particles_clear(d);
    enter(d, DUNGEON_BOSS);
}

void dungeon_init(dungeon_t *d, uint32_t seed)
{
    memset(d, 0, sizeof(*d));
    d->rng = seed ? seed : 0x51ed270bu;
    d->state = DUNGEON_TITLE;
    d->room = DUNGEON_ARRIVAL;
    d->boss = DUNGEON_WARDEN;
}

void dungeon_step(dungeon_t *d, uint32_t dt_ms)
{
    d->uptime_ms += dt_ms;
    d->state_ms += dt_ms;
    particles_step(d, dt_ms);
    for (;;) {
        uint32_t duration = 0;
        switch (d->state) {
        case DUNGEON_TITLE: duration = TITLE_MS; break;
        case DUNGEON_DOOR_OPEN: case DUNGEON_DOOR_CLOSE: duration = DOOR_MS; break;
        case DUNGEON_ROOM: duration = d->room_ms; break;
        case DUNGEON_BOSS: duration = BOSS_MS; break;
        default: return;
        }
        if (d->state_ms < duration) break;
        d->state_ms -= duration;
        switch (d->state) {
        case DUNGEON_TITLE: begin_run(d); break;
        case DUNGEON_DOOR_OPEN: enter(d, DUNGEON_ROOM); break;
        case DUNGEON_ROOM:
            if (d->room == DUNGEON_TREASURE || d->room == DUNGEON_SHRINE)
                particles_burst(d, d->focal_x, d->focal_y - 8, 2);
            enter(d, DUNGEON_DOOR_CLOSE);
            break;
        case DUNGEON_DOOR_CLOSE:
            if (!d->normal_rooms) begin_run(d);
            else if (d->normal_rooms >= d->target_rooms) choose_boss(d);
            else { choose_room(d); enter(d, DUNGEON_DOOR_OPEN); }
            break;
        case DUNGEON_BOSS:
            particles_burst(d, d->focal_x, d->focal_y - 10, 3);
            /* Zero marks a completed run while the door is closing. */
            d->normal_rooms = 0;
            enter(d, DUNGEON_DOOR_CLOSE);
            break;
        default: return;
        }
    }
    if (d->state == DUNGEON_ROOM) {
        uint32_t p = d->room_ms ? d->state_ms * 100u / d->room_ms : 0;
        int start = d->entry ? 8 : 118;
        int target = d->focal_x + (d->entry ? -13 : 13);
        if (p < 45) d->hero_x = (int8_t)(start + (target - start) * (int)p / 45);
        else if (d->room == DUNGEON_AMBUSH && p > 60 && p < 78)
            d->hero_x = (int8_t)(target + (d->entry ? -8 : 8));
        else d->hero_x = (int8_t)target;
        d->hero_y = (int8_t)(42 - ((p / 11u) & 1u));
        /* approach → telegraph → lunge → player counter → defeat */
        d->phase = p < 18 ? 0 : p < 38 ? 1 : p < 57 ? 2 : p < 76 ? 3 : 4;
        if ((d->room == DUNGEON_AMBUSH || d->room == DUNGEON_ELITE) && p == 58)
            particles_burst(d, d->hero_x + (d->entry ? 10 : -10), d->hero_y - 10, 2);
    } else if (d->state == DUNGEON_BOSS) {
        uint32_t p = d->state_ms * 100u / BOSS_MS;
        d->phase = p < 17 ? 0 : p < 36 ? 1 : p < 54 ? 2 : p < 74 ? 3 : 4;
        if (p < 20) d->hero_x = (int8_t)(28 + (int)p / 3);
        else if (p < 60) d->hero_x = p > 43 && p < 55 ? 25 : 38;
        else d->hero_x = 45;
        d->hero_y = (int8_t)(42 - ((p / 10u) & 1u));
        if (p == 43 || p == 61 || p == 82) particles_burst(d, d->focal_x, d->focal_y - 8, 3);
    }
}

const char *dungeon_state_name(dungeon_state_t state)
{
    static const char *const names[] = {"TITLE", "DOOR_OPEN", "ROOM", "BOSS", "DOOR_CLOSE"};
    return state < DUNGEON_STATE_COUNT ? names[state] : "UNKNOWN";
}

const char *dungeon_room_name(dungeon_room_t room)
{
    static const char *const names[] = {"ARRIVAL", "CORRIDOR", "TREASURE", "AMBUSH", "ELITE", "SHRINE", "EXIT"};
    return room < DUNGEON_ROOM_COUNT ? names[room] : "UNKNOWN";
}

const char *dungeon_boss_name(dungeon_boss_t boss)
{
    static const char *const names[] = {"WARDEN", "WYRM", "EYE"};
    return boss < DUNGEON_BOSS_COUNT ? names[boss] : "UNKNOWN";
}
