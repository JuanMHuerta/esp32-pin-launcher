// SPDX-License-Identifier: GPL-3.0-only
#include "input.h"
#include "paint.h"
#include "pet.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void tick(pet_t *p, unsigned ms)
{
    while (ms) {
        unsigned dt = ms > 20 ? 20 : ms;
        pet_step(p, dt);
        ms -= dt;
    }
}
static void test_interactions(void)
{
    pet_t p;
    pet_init(&p, 1);
    pet_event(&p, PET_TAP, 67, 30);
    assert(p.state == PET_LOVE);
    tick(&p, 200);
    assert(p.lift > 3 && p.lift < 8);
    tick(&p, 600);
    assert(p.lift >= 0);
    pet_event(&p, PET_TAP, 0, 58);
    assert(p.state == PET_WALK && p.food && p.target_x == 29);
    for (int i = 0; i < 400 && p.state != PET_EAT; ++i) {
        tick(&p, 20);
    }
    assert(p.state == PET_EAT && !p.food && fabsf(p.x - 29) < 2);
    for (int side = 0; side < 2; ++side) {
        pet_init(&p, 1);
        p.x = side ? 28 : 106;
        p.speed = side ? -22 : 22; // Also reverse an ongoing chase at the edge.
        pet_event(&p, PET_TAP, side ? 133 : 0, 58);
        for (int i = 0; i < 400 && p.state != PET_EAT; ++i) {
            tick(&p, 20);
        }
        assert(p.state == PET_EAT && !p.food);
        assert(fabsf(p.x - (side ? 105 : 29)) < 2);
    }
    pet_event(&p, PET_HOLD, 67, 30);
    tick(&p, 70000);
    assert(p.state == PET_SLEEP && p.manual_sleep && p.sleep_blend > .99f);
    pet_event(&p, PET_TAP, 0, 0);
    assert(!p.manual_sleep && p.state == PET_LOVE);
    pet_event(&p, PET_HOLD, 67, 30);
    pet_event(&p, PET_HOLD, 67, 30);
    assert(!p.manual_sleep && p.state == PET_STRETCH);
    pet_event(&p, PET_SWIPE, 133, 20);
    assert(p.state == PET_PLAY && p.target_x == 104);
    pet_event(&p, PET_SHAKE, 0, 0);
    assert(p.state == PET_SURPRISE);
    unsigned events = p.interactions;
    tick(&p, 500);
    pet_event(&p, PET_SHAKE, 0, 0);
    assert(p.interactions == events && p.state_ms == 500);
    tick(&p, 6000);
    pet_event(&p, PET_SHAKE, 0, 0);
    assert(p.interactions == events + 1);
    pet_set_tilt(&p, NAN);
    assert(p.tilt == 0);
    pet_set_tilt(&p, 100);
    tick(&p, 1000);
    assert(p.tilt == 1 && p.lean <= 4);
    pet_state_t prior = p.state;
    pet_set_state(&p, PET_STATE_COUNT, 0);
    assert(p.state == prior);
    printf("PASS interactions, feeding arrival, nap/wake, bounded hops, shake cooldown\n");
}
static void test_scene_behavior(void)
{
    pet_t p;
    pet_init(&p, 77);
    assert(pet_daylight(&p) == 1);
    p.uptime_ms = PET_DAY_MS / 2;
    assert(pet_daylight(&p) < .001f);
    p.uptime_ms = PET_DAY_MS / 4;
    assert(fabsf(pet_daylight(&p) - .5f) < .001f);
    p.uptime_ms += 100000ULL * PET_DAY_MS;
    assert(fabsf(pet_daylight(&p) - .5f) < .001f);
    pet_set_state(&p, PET_IDLE, 10000);
    pet_set_tilt(&p, .9f);
    tick(&p, 500);
    assert(p.state == PET_IDLE);
    tick(&p, 100);
    assert(p.state == PET_BALANCE);
    tick(&p, 1000);
    assert(p.lean > 4 && p.gaze > 2);
    pet_set_tilt(&p, .4f);
    tick(&p, 500);
    assert(p.state == PET_BALANCE); // Hysteresis: don't chatter near the trigger.
    pet_set_tilt(&p, 0);
    tick(&p, 500);
    assert(p.state == PET_WAVE);
    pet_set_tilt(&p, -1);
    tick(&p, 1000);
    assert(p.state == PET_WAVE); // Let a recovery finish before another reaction.
    p.x = 106;
    p.speed = 0;
    pet_event(&p, PET_TAP, 0, 58);
    tick(&p, 4000);
    assert(p.food && p.state == PET_WALK); // Tilting doesn't interrupt feeding.
    tick(&p, 3000);
    assert(!p.food && p.state == PET_EAT); // Finish the meal before balancing.
    pet_event(&p, PET_HOLD, 0, 0);
    tick(&p, 10000);
    assert(p.manual_sleep && p.state == PET_SLEEP);
    pet_event(&p, PET_SHAKE, 0, 0);
    assert(!p.manual_sleep && p.state == PET_SURPRISE);
    tick(&p, 6000);
    pet_event(&p, PET_SHAKE, 0, 0);
    assert(p.state == PET_DANCE);
    tick(&p, 6000);
    pet_event(&p, PET_SHAKE, 0, 0);
    assert(p.state == PET_PLAY);

    pet_init(&p, 77);
    pet_set_state(&p, PET_SLEEP, 600);
    tick(&p, 600);
    assert(p.state == PET_STRETCH);
    uint32_t nap_ms[2];
    for (int night = 0; night < 2; ++night) {
        pet_init(&p, 77);
        p.uptime_ms = night ? PET_DAY_MS / 2 : 0;
        p.activities = 1u << PET_SLEEP;
        pet_set_state(&p, PET_IDLE, 20);
        tick(&p, 20);
        assert(p.state == PET_SLEEP);
        nap_ms[night] = p.duration_ms;
    }
    assert(nap_ms[1] > nap_ms[0]);
    /* Every autonomous activity appears within a short unattended session,
       including eating a berry that was found and approached first. */
    pet_init(&p, 77);
    bool approached = false, fed = false;
    unsigned seen = 0;
    pet_state_t previous = p.state;
    for (unsigned ms = 0; ms < 180000; ms += 20) {
        pet_step(&p, 20);
        seen |= 1u << p.state;
        if (p.food) {
            approached = true;
            assert(p.state == PET_WALK);
        }
        if (p.state == PET_EAT) {
            assert(approached);
            fed = true;
        }
        if (p.state_ms == 0) {
            assert(p.state != previous);
        }
        previous = p.state;
    }
    unsigned automatic = ((1u << PET_STATE_COUNT) - 1) &
                         ~((1u << PET_LOVE) | (1u << PET_SURPRISE) | (1u << PET_BALANCE));
    assert((seen & automatic) == automatic && fed);
    puts("PASS cycle periodicity, tilt sustain/recovery, feeding/nap protection, varied shake "
         "responses, wake stretch and autonomous variety");
}
static void test_touch(void)
{
    for (int cy = 0; cy < 2; ++cy) {
        for (int cx = 0; cx < 2; ++cx) {
            unsigned rx = cx * 535, ry = cy * 239;
            uint8_t report[5] = {1, (uint8_t)(ry >> 8), (uint8_t)ry, (uint8_t)(rx >> 8),
                                 (uint8_t)rx};
            bool down;
            int x, y;
            assert(touch_decode(report, &down, &x, &y));
            assert(down && x == cx * 133 && y == (1 - cy) * 59);
        }
    }
    bool down;
    int x = 9, y = 9;
    uint8_t bad[5] = {1, 0, 240, 0, 0};
    assert(!touch_decode(bad, &down, &x, &y));
    uint8_t up[5] = {0, 0, 255, 255, 255};
    assert(touch_decode(up, &down, &x, &y) && !down);
    gesture_t g = {0};
    input_event_t e;
    assert(!gesture_update(&g, true, 67, 30, 0, &e));
    assert(!gesture_update(&g, true, 68, 31, 100, &e));
    assert(gesture_update(&g, false, 0, 0, 120, &e));
    assert(e.type == PET_TAP && e.x == 68 && e.y == 31);
    assert(!gesture_update(&g, false, 0, 0, 130, &e));
    assert(!gesture_update(&g, true, 50, 20, 200, &e));
    assert(gesture_update(&g, true, 51, 21, 1000, &e) && e.type == PET_HOLD);
    assert(!gesture_update(&g, true, 51, 21, 2000, &e));
    assert(!gesture_update(&g, false, 0, 0, 2200, &e));
    assert(!gesture_update(&g, true, 20, 30, UINT32_MAX - 100, &e));
    assert(!gesture_update(&g, true, 100, 30, 20, &e));
    assert(gesture_update(&g, false, 0, 0, 100, &e) && e.type == PET_SWIPE);
    assert(!gesture_update(&g, true, 20, 30, 200, &e));
    assert(!gesture_update(&g, false, 0, 0, 203, &e));
    printf("PASS four touch corners, invalid reports, debounce, hold suppression, swipe, timer "
           "wrap\n");
}
static void test_motion(void)
{
    motion_t m = {0};
    float a[3] = {0, 0, 1};
    for (int i = 0; i < 100; ++i) {
        assert(!motion_update(&m, a, 20));
    }
    for (int i = 0; i < 100; ++i) {
        a[1] = i * .007f;
        a[2] = sqrtf(1 - a[1] * a[1]);
        assert(!motion_update(&m, a, 20));
    }
    assert(m.tilt > .8f);
    a[0] = 3;
    assert(!motion_update(&m, a, 20));
    assert(motion_update(&m, a, 20));
    for (int i = 0; i < 100; ++i) {
        a[0] = i % 2 ? 3 : -3;
        assert(!motion_update(&m, a, 20));
    }
    a[0] = NAN;
    assert(!motion_update(&m, a, 20));
    assert(isfinite(m.tilt));
    printf("PASS stationary/tilt rejection, shake confirmation, noise bounds and cooldown\n");
}
static void test_paint(void)
{
    struct {
        uint64_t before;
        uint16_t pixels[PET_W * PET_H];
        uint64_t after;
    } buf;
    struct {
        uint64_t before;
        uint16_t pixels[DISPLAY_W * 40];
        uint64_t after;
    } strip;
    buf.before = buf.after = strip.before = strip.after = 0x1234fedc8765abcdULL;
    pet_t p;
    pet_init(&p, 42);
    for (int state = 0; state < PET_STATE_COUNT; ++state) {
        pet_set_state(&p, state, 600000);
        for (int i = 0; i < 240; ++i) {
            tick(&p, 33);
            p.uptime_ms = (uint64_t)i * 1703;
            pet_paint(&p, buf.pixels);
            assert(buf.before == 0x1234fedc8765abcdULL && buf.after == buf.before);
        }
    }
    // Pixel expansion and wire order are checked against independently decoded
    // panel coordinates, including every row and all strip boundaries.
    for (int i = 0; i < PET_W * PET_H; ++i) {
        buf.pixels[i] = (uint16_t)(i * 7);
    }
    for (int y = 0; y < DISPLAY_H; y += 40) {
        assert(pet_expand_strip(buf.pixels, y, 40, strip.pixels));
        for (int j = 0; j < 40; ++j) {
            for (int x = 0; x < DISPLAY_W; ++x) {
                const uint8_t *wire = (const uint8_t *)&strip.pixels[j * DISPLAY_W + x];
                uint16_t color = (uint16_t)((wire[0] << 8) | wire[1]);
                assert(color == buf.pixels[((y + j) / 4) * PET_W + x / 4]);
            }
        }
        assert(strip.before == 0x1234fedc8765abcdULL && strip.after == strip.before);
    }
    assert(!pet_expand_strip(buf.pixels, -4, 40, strip.pixels));
    assert(!pet_expand_strip(buf.pixels, 220, 40, strip.pixels));
    assert(!pet_expand_strip(buf.pixels, 0, 39, strip.pixels));
    assert(!pet_expand_strip(buf.pixels, 1, 40, strip.pixels));
    assert(pet_rgb(0xff0000) == 0xf800 && pet_rgb(0x00ff00) == 0x07e0);
    assert(pet_rgb(0x0000ff) == 0x001f && pet_rgb(0xffffff) == 0xffff);
    // Rendering covers the entire frame, stays pure, and loops the sky palette.
    pet_init(&p, 42);
    p.uptime_ms = 0;
    memset(buf.pixels, 0xff, sizeof(buf.pixels));
    pet_t unchanged = p;
    pet_paint(&p, buf.pixels);
    assert(!memcmp(&p, &unchanged, sizeof(p)));
    for (int i = 0; i < PET_W * PET_H; ++i) {
        assert(buf.pixels[i] != 0xffff);
    }
    uint16_t noon = buf.pixels[6 * PET_W + 67];
    p.uptime_ms = PET_DAY_MS / 2;
    pet_paint(&p, buf.pixels);
    assert(buf.pixels[6 * PET_W + 67] != noon);
    p.uptime_ms = PET_DAY_MS;
    pet_paint(&p, buf.pixels);
    assert(buf.pixels[6 * PET_W + 67] == noon);
    for (int edge = 0; edge < 2; ++edge) {
        p.x = edge ? 106 : 28;
        pet_set_tilt(&p, edge ? 1 : -1);
        for (int state = 0; state < PET_STATE_COUNT; ++state) {
            pet_set_state(&p, state, 10000);
            tick(&p, 1000);
            pet_paint(&p, buf.pixels);
            assert(buf.before == 0x1234fedc8765abcdULL && buf.after == buf.before);
        }
    }
    printf("PASS %u animation frames, render purity/coverage, edge poses, buffer guards, "
           "strip bounds and every expanded wire pixel\n",
           PET_STATE_COUNT * 240);
}
static void test_long_run(void)
{
    pet_t p;
    pet_init(&p, 123);
    unsigned autonomous = 0;
    for (unsigned i = 0; i < 24 * 60 * 60 * 30; ++i) {
        pet_step(&p, i % 3 ? 33 : 34);
        assert(isfinite(p.x) && p.x >= 28 && p.x <= 106);
        assert(isfinite(p.lift) && p.lift >= 0 && p.lift < 9);
        assert(p.state >= 0 && p.state < PET_STATE_COUNT);
        autonomous |= 1u << p.state;
    }
    unsigned all_auto = ((1u << PET_STATE_COUNT) - 1) &
                        ~((1u << PET_LOVE) | (1u << PET_SURPRISE) | (1u << PET_BALANCE));
    assert((autonomous & all_auto) == all_auto);
    assert(p.uptime_ms == 86400000 && p.transitions > 5000);
    printf("PASS 24 simulated hours / %u transitions / autonomous states=0x%x\n", p.transitions,
           autonomous);
}
int main(void)
{
    test_interactions();
    test_scene_behavior();
    test_touch();
    test_motion();
    test_paint();
    test_long_run();
    puts("ALL TESTS PASSED");
    return 0;
}
