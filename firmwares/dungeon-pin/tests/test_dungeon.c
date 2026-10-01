#include "../main/dungeon.h"
#include "../main/paint.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void advance(dungeon_t *d, unsigned ms)
{
    while (ms) { unsigned step = ms > 17 ? 17 : ms; dungeon_step(d, step); ms -= step; }
}

static unsigned nonzero(const uint16_t *frame)
{
    unsigned count = 0;
    for (unsigned i = 0; i < DUNGEON_W * DUNGEON_H; ++i) if (frame[i]) count++;
    return count;
}

static uint32_t frame_hash(const uint16_t *frame)
{
    uint32_t h = 2166136261u;
    for (unsigned i = 0; i < DUNGEON_W * DUNGEON_H; ++i) h = (h ^ frame[i]) * 16777619u;
    return h;
}

static int focal_is_lit(const uint16_t *frame, int x, int y)
{
    for (int yy = y - 11; yy <= y + 3; ++yy) for (int xx = x - 11; xx <= x + 11; ++xx) {
        if (xx < 0 || yy < 0 || xx >= DUNGEON_W || yy >= DUNGEON_H) continue;
        uint16_t p = frame[yy * DUNGEON_W + xx];
        if ((p & 0xf800u) >= 0x2800u || (p & 0x07e0u) >= 0x0340u) return 1;
    }
    return 0;
}

int main(void)
{
    dungeon_t a, b, other;
    dungeon_init(&a, 0x12345678u);
    dungeon_init(&b, 0x12345678u);
    dungeon_init(&other, 0x87654321u);
    assert(a.state == DUNGEON_TITLE);
    advance(&a, 1900);
    assert(a.state == DUNGEON_DOOR_OPEN && a.run == 1 && a.target_rooms >= 4 && a.target_rooms <= 6);

    unsigned bosses = 0, previous_normal = 0;
    for (unsigned elapsed = 0; elapsed < 180000; elapsed += 17) {
        dungeon_state_t before = a.state;
        dungeon_step(&a, 17);
        assert(a.hero_x >= -20 && a.hero_x <= DUNGEON_W + 20);
        assert(a.hero_y >= -20 && a.hero_y <= DUNGEON_H + 20);
        assert(a.particle_count <= DUNGEON_PARTICLES);
        for (unsigned i = 0; i < DUNGEON_PARTICLES; ++i)
            assert(a.particles[i].life <= 14);
        if (before != DUNGEON_BOSS && a.state == DUNGEON_BOSS) {
            assert(a.normal_rooms >= 4 && a.normal_rooms <= 6);
            assert(a.normal_rooms == a.target_rooms);
            bosses++;
        }
        previous_normal = a.normal_rooms;
    }
    assert(bosses >= 3);
    assert(previous_normal <= 6);

    /* Every card has a bounded resolution; none depends on a player action. */
    for (unsigned room = 0; room < DUNGEON_ROOM_COUNT; ++room) {
        dungeon_init(&a, room + 1);
        advance(&a, 1900 + 430);
        a.state = DUNGEON_ROOM;
        a.room = (dungeon_room_t)room;
        a.state_ms = a.room_ms;
        dungeon_step(&a, 1);
        assert(a.state == DUNGEON_DOOR_CLOSE);
    }
    for (unsigned boss = 0; boss < DUNGEON_BOSS_COUNT; ++boss) {
        dungeon_init(&a, boss + 10);
        a.state = DUNGEON_BOSS;
        a.boss = (dungeon_boss_t)boss;
        a.state_ms = 6500;
        dungeon_step(&a, 1);
        assert(a.state == DUNGEON_DOOR_CLOSE);
    }

    for (unsigned i = 0; i < 4000; ++i) { dungeon_step(&b, 17); dungeon_step(&other, 17); }
    assert(memcmp(&b, &other, sizeof(b)) != 0);
    dungeon_init(&a, 0x12345678u);
    dungeon_init(&b, 0x12345678u);
    for (unsigned i = 0; i < 4000; ++i) { dungeon_step(&a, 17); dungeon_step(&b, 17); }
    assert(memcmp(&a, &b, sizeof(a)) == 0);

    uint16_t frame[DUNGEON_W * DUNGEON_H], strip[DISPLAY_W * 40];
    uint16_t previous_enemy[DUNGEON_W * DUNGEON_H];

    /* Every room gets a visible focal composition; the families must remain
     * materially distinct even when the simulation state is identical. */
    uint32_t room_art[DUNGEON_ROOM_COUNT];
    for (unsigned room = 0; room < DUNGEON_ROOM_COUNT; ++room) {
        dungeon_init(&a, 700 + room);
        a.state = DUNGEON_ROOM;
        a.room = (dungeon_room_t)room;
        a.phase = 3;
        a.focal_x = 67;
        a.focal_y = 35;
        dungeon_paint(&a, frame);
        assert(nonzero(frame) > 100);
        assert(focal_is_lit(frame, a.focal_x, a.focal_y));
        room_art[room] = frame_hash(frame);
    }
    assert(room_art[DUNGEON_CORRIDOR] != room_art[DUNGEON_TREASURE]);
    assert(room_art[DUNGEON_AMBUSH] != room_art[DUNGEON_SHRINE]);

    /* The three layout selections are all clipped-safe and change the
     * vanishing-point composition rather than changing the simulation. */
    uint32_t layout_art[3];
    for (unsigned layout = 0; layout < 3; ++layout) {
        dungeon_init(&a, 720 + layout);
        a.state = DUNGEON_ROOM;
        a.room = DUNGEON_CORRIDOR;
        a.layout = (uint8_t)layout;
        a.phase = 1;
        dungeon_paint(&a, frame);
        assert(nonzero(frame) > 100);
        layout_art[layout] = frame_hash(frame);
    }
    assert(layout_art[0] != layout_art[1] && layout_art[1] != layout_art[2]);
    for (unsigned kind = 0; kind < 4; ++kind) {
        dungeon_init(&a, 77);
        advance(&a, 2330);
        a.state = DUNGEON_ROOM;
        a.room = DUNGEON_AMBUSH;
        a.enemy = (uint8_t)kind;
        dungeon_paint(&a, frame);
        if (kind) assert(memcmp(frame, previous_enemy, sizeof(frame)) != 0);
        memcpy(previous_enemy, frame, sizeof(frame));
    }
    /* All telegraph/lunge/counter/defeat art is a distinct packed frame. */
    for (unsigned kind = 0; kind < 4; ++kind) {
        uint16_t phase_frame[5][DUNGEON_W * DUNGEON_H];
        for (unsigned phase = 0; phase < 5; ++phase) {
            dungeon_init(&a, 91 + kind);
            advance(&a, 2330);
            a.state = DUNGEON_ROOM;
            a.room = DUNGEON_AMBUSH;
            a.enemy = (uint8_t)kind;
            a.phase = (uint8_t)phase;
            dungeon_paint(&a, phase_frame[phase]);
            assert(nonzero(phase_frame[phase]) > 100);
            if (phase) assert(memcmp(phase_frame[phase], phase_frame[phase - 1], sizeof(frame)) != 0);
        }
    }
    for (unsigned kind = 0; kind < DUNGEON_BOSS_COUNT; ++kind) {
        uint16_t phase_frame[5][DUNGEON_W * DUNGEON_H];
        for (unsigned phase = 0; phase < 5; ++phase) {
            dungeon_init(&a, 111 + kind);
            a.state = DUNGEON_BOSS;
            a.boss = (dungeon_boss_t)kind;
            a.phase = (uint8_t)phase;
            dungeon_paint(&a, phase_frame[phase]);
            assert(nonzero(phase_frame[phase]) > 100);
            if (phase) assert(memcmp(phase_frame[phase], phase_frame[phase - 1], sizeof(frame)) != 0);
        }
    }
    /* Every set-piece family is composited safely, including edge-clipped wipe gates. */
    for (unsigned set = 0; set < 3; ++set) for (unsigned room = 0; room < DUNGEON_ROOM_COUNT; ++room) {
        dungeon_init(&a, 131 + set + room);
        a.state = DUNGEON_ROOM;
        a.room = (dungeon_room_t)room;
        a.catacomb_set = (uint8_t)set;
        a.phase = 3;
        dungeon_paint(&a, frame);
        assert(nonzero(frame) > 100);
        a.state = DUNGEON_DOOR_OPEN;
        for (unsigned ms = 0; ms <= 430; ms += 215) {
            a.state_ms = ms;
            dungeon_paint(&a, frame);
            assert(nonzero(frame) > 100);
            a.state = DUNGEON_DOOR_CLOSE;
            dungeon_paint(&a, frame);
            assert(nonzero(frame) > 100);
            a.state = DUNGEON_DOOR_OPEN;
        }
    }
    for (unsigned state = 0; state < DUNGEON_STATE_COUNT; ++state) {
        dungeon_init(&a, 42);
        advance(&a, 1900 + 430);
        a.state = (dungeon_state_t)state;
        a.state_ms = 150;
        for (unsigned room = 0; room < DUNGEON_ROOM_COUNT; ++room) {
            a.room = (dungeon_room_t)room;
            a.focal_x = 67;
            a.focal_y = 39;
            dungeon_paint(&a, frame);
            assert(nonzero(frame) > 100);
            if (state != DUNGEON_TITLE && state != DUNGEON_DOOR_CLOSE && state != DUNGEON_DOOR_OPEN)
                assert(focal_is_lit(frame, a.focal_x, a.focal_y));
            assert(dungeon_expand_strip(frame, 0, 40, strip));
        }
    }
    assert(!dungeon_expand_strip(frame, 1, 40, strip));
    assert(!dungeon_expand_strip(frame, 0, 39, strip));
    puts("dungeon simulation and renderer checks passed");
    return 0;
}
