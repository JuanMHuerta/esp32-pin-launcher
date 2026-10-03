// SPDX-License-Identifier: GPL-3.0-only
#include "scene.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint16_t background[WAYFARER_WIDTH * WAYFARER_HEIGHT];
static uint16_t whole[WAYFARER_WIDTH * WAYFARER_HEIGHT];
static uint16_t strips[WAYFARER_WIDTH * WAYFARER_HEIGHT];

static void test_random_event_schedule(void)
{
    wayfarer_scene_t first, second;
    wayfarer_scene_init(&first, 0x81d2a93bU);
    wayfarer_scene_init(&second, 0x81d2a93bU);
    unsigned seen = 0, bag = 0;
    const unsigned all = (1U << WAYFARER_EVENT_COUNT) - 2U;
    const unsigned locals = all & ~(1U << WAYFARER_EVENT_PLANET) & ~(1U << WAYFARER_EVENT_WARP);
    uint8_t last = WAYFARER_EVENT_NONE;
    uint32_t last_start = UINT32_MAX;
    for (uint32_t time = 0; time < 540000; time += 100) {
        wayfarer_scene_update(&first, time, false);
        wayfarer_scene_update(&second, time, false);
        assert(memcmp(&first, &second, sizeof(first)) == 0);
        if (first.event == WAYFARER_EVENT_WARP &&
            time - first.event_started_ms >= first.event_duration_ms - 150) {
            for (unsigned i = 0; i < WAYFARER_BODY_CAPACITY; ++i) {
                assert(!wayfarer_scene_project_body(&first, &first.bodies[i], time).visible);
            }
        }
        if (first.event != WAYFARER_EVENT_NONE) {
            seen |= 1U << first.event;
            if (first.event_started_ms != last_start && last_start != UINT32_MAX) {
                assert(last != first.event);
            }
            if (first.event_started_ms != last_start) {
                if (last_start != UINT32_MAX) {
                    if (last == WAYFARER_EVENT_WARP) {
                        assert(first.event == WAYFARER_EVENT_PLANET);
                    } else if (last != WAYFARER_EVENT_PLANET) {
                        assert(first.event == WAYFARER_EVENT_WARP);
                    } else {
                        assert(first.event != WAYFARER_EVENT_PLANET &&
                               first.event != WAYFARER_EVENT_WARP);
                    }
                }
                if (locals & (1U << first.event)) {
                    assert(!(bag & (1U << first.event)));
                    bag |= 1U << first.event;
                    if (bag == locals) {
                        bag = 0;
                    }
                }
                last = first.event;
                last_start = first.event_started_ms;
            }
        }
    }
    unsigned event_kinds = 0;
    for (unsigned i = 1; i < WAYFARER_EVENT_COUNT; ++i) {
        event_kinds += (seen >> i) & 1U;
    }
    assert(event_kinds == WAYFARER_EVENT_COUNT - 1);
}

static void test_render_consistency_and_bounds(void)
{
    uint16_t guarded[WAYFARER_WIDTH * 30 + 2];
    for (unsigned event = 0; event < WAYFARER_EVENT_COUNT; ++event) {
        for (unsigned variant = 0; variant < 6; ++variant) {
            for (int direction = -1; direction <= 1; direction += 2) {
                wayfarer_scene_t scene;
                wayfarer_scene_init(&scene, 0x155aa551U);
                wayfarer_scene_begin_event(&scene, (uint8_t)event, 0, 8000, (uint8_t)variant,
                                           direction);
                scene.next_event_ms = 3600000;
                wayfarer_scene_update(&scene, 4000, true);
                wayfarer_scene_render_strip(&scene, background, 4000, 0, WAYFARER_HEIGHT, whole);
                for (int y = 0; y < WAYFARER_HEIGHT; y += 30) {
                    wayfarer_scene_render_strip(&scene, background, 4000, y, 30,
                                                strips + y * WAYFARER_WIDTH);
                    guarded[0] = 0xbeef;
                    guarded[WAYFARER_WIDTH * 30 + 1] = 0xbeef;
                    wayfarer_scene_render_strip(&scene, background, 4000, y, 30, guarded + 1);
                    assert(guarded[0] == 0xbeef);
                    assert(guarded[WAYFARER_WIDTH * 30 + 1] == 0xbeef);
                }
                assert(memcmp(whole, strips, sizeof(whole)) == 0);
                /* Exterior variants/directions cannot leak through lit canopy
                 * material. Lighting is allowed to change the painted color. */
                /* Nebula variants intentionally tint the reflected cabin light. */
                if (scene.event != WAYFARER_EVENT_NEBULA) {
                    scene.variant = (uint8_t)((variant + 1) % 6);
                }
                scene.direction = (int8_t)-direction;
                wayfarer_scene_render_strip(&scene, background, 4000, 0, WAYFARER_HEIGHT, strips);
                for (unsigned i = 0; i < WAYFARER_WIDTH * 160; ++i) {
                    if (background[i]) {
                        assert(whole[i] == strips[i]);
                    }
                }
            }
        }
    }
}

static void test_approach_persistence_and_departure(void)
{
    wayfarer_scene_t scene;
    wayfarer_scene_init(&scene, 25);
    scene.event = WAYFARER_EVENT_NONE;
    scene.next_event_ms = 3600000;
    wayfarer_body_t *planet = &scene.bodies[0];
    planet->x = 240;
    planet->y = -55;
    wayfarer_scene_update(&scene, 1500, false);
    wayfarer_projection_t far = wayfarer_scene_project_body(&scene, planet, 1500);
    wayfarer_scene_update(&scene, 15000, false);
    wayfarer_projection_t near = wayfarer_scene_project_body(&scene, planet, 15000);
    assert(near.visible && near.width > far.width && near.x > far.x && near.y < far.y);
    wayfarer_body_t snapshot = *planet;
    wayfarer_scene_begin_event(&scene, WAYFARER_EVENT_TRAFFIC, 15000, 12000, 4, -1);
    assert(memcmp(planet, &snapshot, sizeof(snapshot)) == 0);
    wayfarer_projection_t after = wayfarer_scene_project_body(&scene, planet, 15000);
    assert(after.x == near.x && after.y == near.y && after.width == near.width);
    wayfarer_scene_begin_event(&scene, WAYFARER_EVENT_WARP, 15000, 8000, 2, 1);
    assert(memcmp(planet, &snapshot, sizeof(snapshot)) == 0);
    wayfarer_scene_update(&scene, 15900, false);
    after = wayfarer_scene_project_body(&scene, planet, 15900);
    assert(after.width > near.width && after.x > near.x);
    for (unsigned t = 16000; t <= 22600; t += 100) {
        wayfarer_scene_update(&scene, t, false);
    }
    assert(!wayfarer_scene_project_body(&scene, planet, 22600).visible);
    uint32_t leg = scene.leg;
    wayfarer_scene_update(&scene, 23000, false);
    assert(scene.leg == leg + 1);
    assert(scene.bodies[0].type == WAYFARER_BODY_WORLD);
    assert(scene.bodies[0].born_ms == 23000);
    assert(wayfarer_scene_project_body(&scene, &scene.bodies[0], 23000).width < far.width + 8);
}

static void test_varied_ship_trajectories(void)
{
    unsigned quadrants = 0;
    for (unsigned variant = 0; variant < 16; ++variant) {
        wayfarer_scene_t scene;
        wayfarer_scene_init(&scene, 25);
        memset(scene.bodies, 0, sizeof(scene.bodies));
        wayfarer_scene_begin_event(&scene, WAYFARER_EVENT_TRAFFIC, 0, 12000, (uint8_t)variant, 1);
        const wayfarer_body_t *ship = &scene.bodies[0];
        assert(ship->type == WAYFARER_BODY_SHIP && ship->vy != 0);
        quadrants |= 1U << ((ship->vx > 0 ? 1 : 0) + (ship->vy > 0 ? 2 : 0));
        assert(ship->angle != 0 && ship->angle != 32);
    }
    assert(quadrants == 15);
}

static void test_lighting_motion_and_partial_tiles(void)
{
    wayfarer_scene_t scene;
    wayfarer_scene_init(&scene, 41);
    scene.event = WAYFARER_EVENT_NEBULA;
    scene.next_event_ms = 3600000;
    wayfarer_scene_render_strip(&scene, background, 1000, 0, WAYFARER_HEIGHT, whole);
    wayfarer_scene_render_strip(&scene, background, 7500, 0, WAYFARER_HEIGHT, strips);
    unsigned cabin_changes = 0, darker_material = 0;
    for (int y = 0; y < 160; ++y) {
        for (int x = 0; x < WAYFARER_WIDTH; ++x) {
            unsigned i = (unsigned)(y * WAYFARER_WIDTH + x);
            if (!background[i]) {
                continue;
            }
            cabin_changes += whole[i] != strips[i];
            unsigned original =
                (background[i] >> 11) + ((background[i] >> 5) & 63) + (background[i] & 31);
            unsigned shaded = (whole[i] >> 11) + ((whole[i] >> 5) & 63) + (whole[i] & 31);
            darker_material += shaded + 4 < original;
        }
    }
    assert(cabin_changes > 1000);
    assert(darker_material > 1000);
    /* Odd strips must match the full frame even when a logical pixel is split. */
    for (int y = 0; y < WAYFARER_HEIGHT; y += 31) {
        int rows = WAYFARER_HEIGHT - y;
        if (rows > 31) {
            rows = 31;
        }
        wayfarer_scene_render_strip(&scene, background, 1000, y, rows, strips + y * WAYFARER_WIDTH);
    }
    assert(memcmp(whole, strips, sizeof(whole)) == 0);
}

static void test_cruise_and_instrument_motion(void)
{
    wayfarer_scene_t scene;
    wayfarer_scene_init(&scene, 41);
    scene.next_event_ms = 3600000;
    wayfarer_scene_update(&scene, 1000, false);
    wayfarer_scene_render_strip(&scene, background, 1000, 0, WAYFARER_HEIGHT, whole);
    wayfarer_scene_update(&scene, 2000, false);
    wayfarer_scene_render_strip(&scene, background, 2000, 0, WAYFARER_HEIGHT, strips);
    unsigned space_changes = 0, instrument_changes = 0;
    for (int y = 0; y < WAYFARER_HEIGHT; ++y) {
        for (int x = 0; x < WAYFARER_WIDTH; ++x) {
            unsigned i = (unsigned)(y * WAYFARER_WIDTH + x);
            if (whole[i] == strips[i]) {
                continue;
            }
            if (!background[i]) {
                ++space_changes;
            }
            if (x >= 230 && x < 306 && y >= 194 && y < 228) {
                ++instrument_changes;
            }
        }
    }
    assert(space_changes > 500);
    assert(instrument_changes > 100);
    assert(scene.travel_q8 == 2000U * 256);
}

static void test_jump_acceleration_continuity(void)
{
    wayfarer_scene_t scene;
    wayfarer_scene_init(&scene, 17);
    scene.next_event_ms = 3600000;
    wayfarer_scene_update(&scene, 1000, false);
    uint64_t before = scene.travel_q8;
    scene.event = WAYFARER_EVENT_WARP;
    scene.event_started_ms = 1000;
    scene.event_duration_ms = 4000;
    assert(scene.travel_q8 == before);
    wayfarer_scene_update(&scene, 1100, false);
    assert(scene.travel_q8 > before + 100 * 256);
    assert(scene.travel_q8 < before + 100 * 512);
    wayfarer_scene_update(&scene, 1900, false);
    assert(scene.speed_q8 == 1536);
    wayfarer_scene_update(&scene, 4900, false);
    assert(scene.speed_q8 > 256 && scene.speed_q8 < 512);
    before = scene.travel_q8;
    wayfarer_scene_update(&scene, 5000, false);
    assert(scene.travel_q8 > before);
    assert(scene.speed_q8 == 256);
    assert(scene.event == WAYFARER_EVENT_NONE);
}

static void test_navigation_ping_lifetime(void)
{
    wayfarer_scene_t scene;
    wayfarer_scene_init(&scene, 7);
    wayfarer_scene_update(&scene, 1200, true);
    assert(scene.ping_active);
    wayfarer_scene_update(&scene, 2800, false);
    assert(scene.ping_active);
    wayfarer_scene_update(&scene, 2900, false);
    assert(!scene.ping_active);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    FILE *file = fopen(argv[1], "rb");
    assert(file);
    assert(fread(background, sizeof(uint16_t), WAYFARER_WIDTH * WAYFARER_HEIGHT, file) ==
           WAYFARER_WIDTH * WAYFARER_HEIGHT);
    assert(fgetc(file) == EOF);
    fclose(file);
    test_random_event_schedule();
    test_approach_persistence_and_departure();
    test_varied_ship_trajectories();
    test_render_consistency_and_bounds();
    test_lighting_motion_and_partial_tiles();
    test_cruise_and_instrument_motion();
    test_jump_acceleration_continuity();
    test_navigation_ping_lifetime();
    puts("WAYFARER scene tests passed");
    return 0;
}
