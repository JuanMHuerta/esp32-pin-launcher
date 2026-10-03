// SPDX-License-Identifier: GPL-3.0-only
#include "dungeon.h"

#include <math.h>
#include <string.h>

enum { TITLE_MS = 1200, REST_MS = 1400 };
static const float pi = 3.14159265359f;
/* A side excursion, two backtracks, a loop, then the boss. Every consecutive
 * pair is joined by a carved passage in the SAME persistent map. */
static const uint8_t route[] = {0, 1, 2, 1, 4, 3, 6, 3, 4, 5, 4, 7, 6, 7, 8};
static const uint8_t edges[][2] = {{0, 1}, {1, 2}, {1, 4}, {3, 4}, {3, 6},
                                   {4, 5}, {4, 7}, {6, 7}, {7, 8}, {5, 8}};

static uint32_t random_next(dungeon_t *d)
{
    uint32_t x = d->rng ? d->rng : 0x6d2b79f5u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return d->rng = x;
}

static void particles_clear(dungeon_t *d)
{
    memset(d->particles, 0, sizeof(d->particles));
    d->particle_count = 0;
    d->particle_ms = 0;
}

static void particles_burst(dungeon_t *d)
{
    particles_clear(d);
    for (unsigned i = 0; i < DUNGEON_PARTICLES; ++i) {
        uint32_t r = random_next(d);
        dungeon_particle_t *p = &d->particles[i];
        p->x = 67;
        p->y = 33;
        p->dx = (int8_t)((int)(r & 7u) - 3);
        p->dy = (int8_t)(-1 - (int)((r >> 3) & 3u));
        p->life = (uint8_t)(7 + ((r >> 6) & 7u));
        p->color = (uint8_t)(1 + ((r >> 9) % 3u));
        d->particle_count++;
    }
}

static void particles_step(dungeon_t *d, uint32_t dt)
{
    if (!d->particle_count) {
        return;
    }
    d->particle_ms += dt;
    unsigned ticks = d->particle_ms / 33u;
    d->particle_ms %= 33u;
    if (!ticks) {
        return;
    }
    d->particle_count = 0;
    for (unsigned i = 0; i < DUNGEON_PARTICLES; ++i) {
        dungeon_particle_t *p = &d->particles[i];
        for (unsigned n = 0; n < ticks && p->life; ++n) {
            p->x = (int8_t)(p->x + p->dx);
            p->y = (int8_t)(p->y + p->dy);
            p->dy++;
            p->life--;
        }
        if (p->life) {
            d->particle_count++;
        }
    }
}

int dungeon_walkable(const dungeon_t *d, float x, float y)
{
    int xx = (int)x, yy = (int)y;
    return x >= 0 && y >= 0 && xx < DUNGEON_MAP_SIZE && yy < DUNGEON_MAP_SIZE && !d->tiles[yy][xx];
}

static int combat_node(const dungeon_node_t *node)
{
    return node->boss || node->room == DUNGEON_AMBUSH || node->room == DUNGEON_ELITE ||
           node->room == DUNGEON_CRYPT;
}

int dungeon_is_combat(const dungeon_t *d)
{
    return (d->state == DUNGEON_ROOM || d->state == DUNGEON_BOSS) &&
           combat_node(&d->nodes[d->current_node % DUNGEON_NODES]);
}

unsigned dungeon_contact_ms(unsigned weapon)
{
    return weapon % DUNGEON_WEAPON_COUNT == 1 || weapon % DUNGEON_WEAPON_COUNT == 5 ? 170u : 130u;
}

static void set_state(dungeon_t *d, dungeon_state_t state)
{
    d->state = state;
    d->state_ms = 0;
}

static void facing(dungeon_t *d, float yaw)
{
    d->yaw = yaw;
    d->dir_x = cosf(yaw);
    d->dir_y = sinf(yaw);
}

static void build_world(dungeon_t *d)
{
    memset(d->tiles, 1, sizeof(d->tiles));
    memset(d->nodes, 0, sizeof(d->nodes));
    for (unsigned i = 0; i < DUNGEON_NODES; ++i) {
        dungeon_node_t *n = &d->nodes[i];
        n->x = (uint8_t)(3 + (i % 3u) * 6u);
        n->y = (uint8_t)(15 - (i / 3u) * 6u);
        for (int y = n->y - 2; y <= n->y + 2; ++y) {
            for (int x = n->x - 2; x <= n->x + 2; ++x) {
                d->tiles[y][x] = 0;
            }
        }
        n->enemy = (uint8_t)((i + random_next(d)) % DUNGEON_ENEMY_COUNT);
        n->room = i == 0   ? DUNGEON_ARRIVAL
                  : i == 1 ? DUNGEON_JUNCTION
                  : i == 2 ? DUNGEON_AMBUSH
                  : i == 3 ? (random_next(d) & 1u ? DUNGEON_LIBRARY : DUNGEON_TREASURE)
                  : i == 4 ? DUNGEON_CRYPT
                  : i == 5 ? DUNGEON_ARMORY
                  : i == 6 ? (random_next(d) & 1u ? DUNGEON_GROVE : DUNGEON_ELITE)
                  : i == 7 ? DUNGEON_SHRINE
                           : DUNGEON_EXIT;
        n->boss = i == 8;
        if (combat_node(n)) {
            n->max_hp =
                (uint16_t)(n->boss ? 178 + random_next(d) % 33u
                                   : 62 + n->enemy * 3u + (n->room == DUNGEON_ELITE ? 18u : 0u));
            n->hp = n->max_hp;
        }
        /* Corner columns leave the center axes and connecting paths clear. */
        if ((i + d->layout) % 3u == 0) {
            int diagonal = d->layout < 3 ? -1 : 1;
            d->tiles[n->y - 1][n->x + diagonal] = 2;
            d->tiles[n->y + 1][n->x - diagonal] = 2;
        }
    }
    for (unsigned i = 0; i < sizeof(edges) / sizeof(edges[0]); ++i) {
        dungeon_node_t *a = &d->nodes[edges[i][0]], *b = &d->nodes[edges[i][1]];
        int x = a->x, y = a->y;
        for (;;) {
            d->tiles[y][x] = 0;
            if (x == b->x && y == b->y) {
                break;
            }
            if (x != b->x) {
                x += x < b->x ? 1 : -1;
            } else {
                y += y < b->y ? 1 : -1;
            }
        }
    }
}

static void begin_run(dungeon_t *d)
{
    d->run++;
    d->biome = (uint8_t)((d->run - 1u) % DUNGEON_BIOME_COUNT);
    d->layout = (uint8_t)(random_next(d) % DUNGEON_LAYOUT_COUNT);
    d->boss = (dungeon_boss_t)((d->boss + 1 + random_next(d) % (DUNGEON_BOSS_COUNT - 1)) %
                               DUNGEON_BOSS_COUNT);
    d->normal_rooms = 0;
    d->target_rooms = 8;
    d->route_cursor = 0;
    d->current_node = 0;
    d->goal_node = 0;
    build_world(d);
    d->camera_x = 2.0f;
    d->camera_y = 15.5f;
    facing(d, 0);
    d->room = DUNGEON_ARRIVAL;
    d->scene_boss = 0;
    d->phase = 0;
    d->scene_ms = 0;
    d->phase_ms = 0;
    particles_clear(d);
    /* Resources carry into the next biome. Only a visible defeat/restart resets. */
    set_state(d, DUNGEON_DOOR_OPEN);
}

static void begin_encounter(dungeon_t *d)
{
    dungeon_node_t *n = &d->nodes[d->current_node];
    d->room = (dungeon_room_t)n->room;
    d->enemy = n->enemy;
    d->scene_boss = n->boss;
    n->visited = 1;
    d->round = 0;
    d->strike = 0;
    d->phase = 0;
    d->phase_ms = 0;
    d->scene_ms = 0;
    d->enemy_contact = 0;
    d->player_contact = 0;
    d->launched = 0;
    d->interaction = 0;
    if (d->weapon == 1 && d->mp < 8) {
        d->weapon = 0;
    }
    set_state(d, n->boss ? DUNGEON_BOSS : DUNGEON_ROOM);
}

static void begin_move(dungeon_t *d, float x, float y, int center)
{
    d->from_x = d->camera_x;
    d->from_y = d->camera_y;
    d->to_x = x;
    d->to_y = y;
    float distance = fabsf(x - d->camera_x) + fabsf(y - d->camera_y);
    d->motion_ms = (uint32_t)(distance * 520.0f);
    if (!d->motion_ms) {
        d->motion_ms = 1;
    }
    d->move_to_center = (uint8_t)center;
    d->scene_boss = 0;
    set_state(d, DUNGEON_MOVE);
}

static void approach_goal(dungeon_t *d)
{
    dungeon_node_t *n = &d->nodes[d->goal_node];
    /* Keep encounters at a real 1.5-tile world distance. All sprite scale,
     * perspective, floor and wall motion derive from that same camera. */
    float stop = n->resolved ? 0.0f : 1.5f;
    begin_move(d, n->x + .5f - d->dir_x * stop, n->y + .5f - d->dir_y * stop, 0);
}

static void choose_goal(dungeon_t *d)
{
    if (++d->route_cursor >= sizeof(route)) {
        set_state(d, DUNGEON_DOOR_CLOSE);
        return;
    }
    d->goal_node = route[d->route_cursor];
    dungeon_node_t *n = &d->nodes[d->goal_node];
    float dx = n->x + .5f - d->camera_x, dy = n->y + .5f - d->camera_y;
    float yaw = fabsf(dx) > fabsf(dy) ? dx > 0 ? 0 : pi : dy > 0 ? pi / 2 : -pi / 2;
    float delta = yaw - d->yaw;
    while (delta > pi) {
        delta -= 2 * pi;
    }
    while (delta < -pi) {
        delta += 2 * pi;
    }
    d->from_yaw = d->yaw;
    d->to_yaw = d->yaw + delta;
    if (fabsf(delta) > .1f) {
        d->motion_ms = fabsf(delta) > 2 ? 850 : 580;
        set_state(d, DUNGEON_TURN);
    } else {
        approach_goal(d);
    }
}

static void move_to_center(dungeon_t *d)
{
    dungeon_node_t *n = &d->nodes[d->current_node];
    begin_move(d, n->x + .5f, n->y + .5f, 1);
}

static int need_rest(const dungeon_t *d)
{
    return (d->hp < 60 && d->hp_potions) || (d->mp < 35 && d->mp_potions);
}

static void restore(dungeon_t *d, unsigned hp, unsigned mp)
{
    if (hp) {
        unsigned value = d->hp + hp;
        d->hp = value > 100 ? 100 : (uint8_t)value;
    }
    if (mp) {
        unsigned value = d->mp + mp;
        d->mp = value > 100 ? 100 : (uint8_t)value;
    }
    d->last_restore_ms = d->uptime_ms;
    d->restore_mask = (uint8_t)((hp ? 1 : 0) | (mp ? 2 : 0));
}

static void start_rest(dungeon_t *d, int before)
{
    d->rest_return = (uint8_t)before;
    d->rest_drunk = 0;
    set_state(d, DUNGEON_REST);
}

static void arrive(dungeon_t *d)
{
    d->current_node = d->goal_node;
    dungeon_node_t *n = &d->nodes[d->current_node];
    d->room = (dungeon_room_t)n->room;
    d->enemy = n->enemy;
    d->scene_boss = n->boss;
    if (n->resolved) {
        choose_goal(d);
        return;
    }
    if (combat_node(n) && need_rest(d)) {
        start_rest(d, 1);
    } else {
        begin_encounter(d);
    }
}

static void interact(dungeon_t *d)
{
    dungeon_node_t *n = &d->nodes[d->current_node];
    if (d->interaction || n->resolved) {
        return;
    }
    d->interaction = 1;
    if (n->room == DUNGEON_TREASURE || n->room == DUNGEON_LIBRARY) {
        if (d->hp_potions < 4) {
            d->hp_potions++;
        }
        if (d->mp_potions < 4) {
            d->mp_potions++;
        }
    } else if (n->room == DUNGEON_SHRINE || n->room == DUNGEON_GROVE) {
        restore(d, 22, 34);
    } else if (n->room == DUNGEON_ARMORY) {
        d->weapon = (uint8_t)((d->weapon + 1 + random_next(d) % (DUNGEON_WEAPON_COUNT - 1)) %
                              DUNGEON_WEAPON_COUNT);
    }
}

static void enemy_hit(dungeon_t *d)
{
    if (d->enemy_contact) {
        return;
    }
    d->enemy_contact = 1;
    /* Alternate a successful block with a landed hit. HP never regenerates
     * during a pose change; the renderer reads this persistent resource. */
    unsigned damage = (d->round & 1u) ? 0 : d->scene_boss ? 5u : 4u;
    d->hp = d->hp > damage ? (uint8_t)(d->hp - damage) : 0;
    if (damage) {
        d->last_hurt_ms = d->uptime_ms;
    }
}

static void player_hit(dungeon_t *d)
{
    if (d->player_contact) {
        return;
    }
    d->player_contact = 1;
    d->strike++;
    dungeon_node_t *n = &d->nodes[d->current_node];
    static const uint8_t damage[] = {22, 24, 27, 24, 18, 24};
    unsigned amount = damage[d->weapon % DUNGEON_WEAPON_COUNT];
    n->hp = n->hp > amount ? (uint16_t)(n->hp - amount) : 0;
    n->hits++;
    d->last_hit_ms = d->uptime_ms;
    particles_burst(d);
}

static void next_phase(dungeon_t *d)
{
    dungeon_node_t *n = &d->nodes[d->current_node];
    if (d->phase == 4) {
        if (!n->resolved) {
            n->resolved = 1;
            if (!n->boss) {
                d->normal_rooms++;
            }
        }
        if (n->boss) {
            d->room = DUNGEON_EXIT;
            set_state(d, DUNGEON_DOOR_CLOSE);
            return;
        }
        if (need_rest(d)) {
            start_rest(d, 0);
        } else {
            move_to_center(d);
        }
        return;
    }
    if (!combat_node(n)) {
        d->phase = d->phase == 0 ? 1 : d->phase == 1 ? 3 : 4;
    } else if (d->phase == 3) {
        if (!n->hp) {
            d->phase = 4;
        } else {
            d->round++;
            d->phase = 1;
        }
    } else {
        d->phase++;
    }
    d->phase_ms = 0;
    d->enemy_contact = 0;
    d->player_contact = 0;
    d->launched = 0;
    if (d->phase == 3 && d->weapon == 1 && d->mp < 8) {
        d->weapon = 0;
    }
}

static unsigned phase_duration(const dungeon_t *d)
{
    static const unsigned times[] = {350, 400, 260, 500, 850};
    if (dungeon_is_combat(d)) {
        return times[d->phase % 5u];
    }
    return d->phase == 3 ? 900u : d->phase == 4 ? 500u : 300u;
}

void dungeon_init(dungeon_t *d, uint32_t seed)
{
    memset(d, 0, sizeof(*d));
    d->rng = seed ? seed : 0x51ed270bu;
    d->hp = 100;
    d->mp = 100;
    d->hp_potions = 2;
    d->mp_potions = 2;
    d->room = DUNGEON_ARRIVAL;
    build_world(d);
    d->camera_x = 2;
    d->camera_y = 15.5f;
    facing(d, 0);
    d->state = DUNGEON_TITLE;
}

void dungeon_step(dungeon_t *d, uint32_t dt)
{
    do {
        unsigned duration = d->state == DUNGEON_TITLE ? TITLE_MS
                            : d->state == DUNGEON_DOOR_OPEN || d->state == DUNGEON_DOOR_CLOSE
                                ? DUNGEON_DOOR_MS
                            : d->state == DUNGEON_MOVE || d->state == DUNGEON_TURN ? d->motion_ms
                            : d->state == DUNGEON_REST                             ? REST_MS
                            : d->state == DUNGEON_FALL ? 1600
                                                       : phase_duration(d);
        unsigned clock =
            d->state == DUNGEON_ROOM || d->state == DUNGEON_BOSS ? d->phase_ms : d->state_ms;
        unsigned remaining = clock < duration ? duration - clock : 0;
        if (dungeon_is_combat(d)) {
            unsigned event = d->phase == 2 && !d->enemy_contact    ? 130u
                             : d->phase == 3 && !d->launched       ? 105u
                             : d->phase == 3 && !d->player_contact ? dungeon_contact_ms(d->weapon)
                                                                   : duration;
            if (event > clock && event - clock < remaining) {
                remaining = event - clock;
            }
        } else if ((d->state == DUNGEON_ROOM && d->phase == 3 && !d->interaction) ||
                   (d->state == DUNGEON_REST && !d->rest_drunk)) {
            unsigned event = d->state == DUNGEON_REST ? 650u : 250u;
            if (event > clock && event - clock < remaining) {
                remaining = event - clock;
            }
        }
        unsigned chunk = dt < remaining ? dt : remaining;
        d->uptime_ms += chunk;
        d->state_ms += chunk;
        dt -= chunk;
        particles_step(d, chunk);
        if (d->state == DUNGEON_MOVE || d->state == DUNGEON_TURN) {
            float t = d->motion_ms ? (float)d->state_ms / d->motion_ms : 1;
            if (t > 1) {
                t = 1;
            }
            float ease = t * t * (3 - 2 * t);
            if (d->state == DUNGEON_MOVE) {
                d->camera_x = d->from_x + (d->to_x - d->from_x) * ease;
                d->camera_y = d->from_y + (d->to_y - d->from_y) * ease;
            } else {
                facing(d, d->from_yaw + (d->to_yaw - d->from_yaw) * ease);
            }
        } else if (d->state == DUNGEON_ROOM || d->state == DUNGEON_BOSS) {
            d->scene_ms += chunk;
            d->phase_ms += chunk;
            if (dungeon_is_combat(d)) {
                if (d->phase == 2 && d->phase_ms >= 130) {
                    enemy_hit(d);
                }
                if (d->phase == 3 && d->phase_ms >= 105 && !d->launched) {
                    d->launched = 1;
                    if (d->weapon == 1) {
                        d->mp = d->mp >= 8 ? (uint8_t)(d->mp - 8) : 0;
                    }
                }
                if (d->phase == 3 && d->phase_ms >= dungeon_contact_ms(d->weapon)) {
                    player_hit(d);
                }
            } else if (d->phase == 3 && d->phase_ms >= 250) {
                interact(d);
            }
            if (!d->hp) {
                set_state(d, DUNGEON_FALL);
                continue;
            }
        } else if (d->state == DUNGEON_REST && d->state_ms >= 650 && !d->rest_drunk) {
            unsigned hp = d->hp < 60 && d->hp_potions ? 35u : 0;
            unsigned mp = !hp && d->mp < 35 && d->mp_potions ? 40u : 0;
            if (hp) {
                d->hp_potions--;
            }
            if (mp) {
                d->mp_potions--;
            }
            restore(d, hp, mp);
            d->rest_drunk = 1;
        }
        clock = d->state == DUNGEON_ROOM || d->state == DUNGEON_BOSS ? d->phase_ms : d->state_ms;
        if (clock < duration) {
            if (dt) {
                continue;
            }
            break;
        }
        switch (d->state) {
        case DUNGEON_TITLE:
            begin_run(d);
            break;
        case DUNGEON_DOOR_OPEN:
            begin_encounter(d);
            break;
        case DUNGEON_ROOM:
        case DUNGEON_BOSS:
            next_phase(d);
            break;
        case DUNGEON_MOVE:
            if (d->move_to_center) {
                choose_goal(d);
            } else {
                arrive(d);
            }
            break;
        case DUNGEON_TURN:
            facing(d, d->to_yaw);
            approach_goal(d);
            break;
        case DUNGEON_REST:
            if (need_rest(d)) {
                start_rest(d, d->rest_return);
            } else if (d->rest_return) {
                begin_encounter(d);
            } else {
                move_to_center(d);
            }
            break;
        case DUNGEON_DOOR_CLOSE:
            begin_run(d);
            break;
        case DUNGEON_FALL:
            d->hp = 100;
            d->mp = 100;
            d->hp_potions = 2;
            d->mp_potions = 2;
            set_state(d, DUNGEON_TITLE);
            break;
        default:
            return;
        }
    } while (dt);
}

void dungeon_preview_scene(dungeon_t *d, dungeon_room_t room, unsigned kind, int boss)
{
    dungeon_node_t *n = &d->nodes[0];
    d->current_node = 0;
    d->room = room;
    d->enemy = kind % DUNGEON_ENEMY_COUNT;
    d->boss = (dungeon_boss_t)(kind % DUNGEON_BOSS_COUNT);
    d->scene_boss = (uint8_t)boss;
    n->room = room;
    n->enemy = d->enemy;
    n->boss = (uint8_t)boss;
    n->visited = 1;
    n->resolved = 0;
    n->max_hp = boss ? 190 : 80;
    n->hp = n->max_hp;
    d->state = boss ? DUNGEON_BOSS : DUNGEON_ROOM;
    d->camera_x = n->x + .5f - 1.5f;
    d->camera_y = n->y + .5f;
    facing(d, 0);
}

const char *dungeon_state_name(dungeon_state_t state)
{
    static const char *const names[] = {"TITLE", "DOOR_OPEN", "ROOM",   "BOSS", "DOOR_CLOSE",
                                        "WALK",  "TURN",      "POTION", "FALL"};
    return (unsigned)state < DUNGEON_STATE_COUNT ? names[state] : "UNKNOWN";
}

const char *dungeon_room_name(dungeon_room_t room)
{
    static const char *const names[] = {"ARRIVAL", "PASSAGE", "TREASURE", "AMBUSH",
                                        "ELITE",   "SHRINE",  "STAIRS",   "JUNCTION",
                                        "CRYPT",   "LIBRARY", "ARMORY",   "GROVE"};
    return (unsigned)room < DUNGEON_ROOM_COUNT ? names[room] : "UNKNOWN";
}

const char *dungeon_boss_name(dungeon_boss_t boss)
{
    static const char *const names[] = {"WARDEN", "WYRM", "EYE", "LICH", "HYDRA", "MINOTAUR"};
    return (unsigned)boss < DUNGEON_BOSS_COUNT ? names[boss] : "UNKNOWN";
}

const char *dungeon_biome_name(unsigned biome)
{
    static const char *const names[] = {"CATACOMBS", "OVERGROWN RUINS", "EMBER KEEP",
                                        "FROZEN VAULT"};
    return names[biome % DUNGEON_BIOME_COUNT];
}
