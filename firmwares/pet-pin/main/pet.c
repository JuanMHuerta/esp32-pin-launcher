// SPDX-License-Identifier: GPL-3.0-only
#include "pet.h"
#include <math.h>
#include <string.h>

static float clampf(float v, float lo, float hi)
{
    return fminf(hi, fmaxf(lo, v));
}
static uint32_t random_next(pet_t *p)
{
    uint32_t n = p->rng;
    n ^= n << 13;
    n ^= n >> 17;
    n ^= n << 5;
    return p->rng = n;
}
const char *pet_state_name(pet_state_t s)
{
    static const char *const names[] = {"idle", "walk", "sniff",    "eat", "sleep",
                                        "love", "play", "surprise", "wave"};
    return s < PET_STATE_COUNT && s >= 0 ? names[s] : "invalid";
}
void pet_set_state(pet_t *p, pet_state_t state, uint32_t duration_ms)
{
    if (state < 0 || state >= PET_STATE_COUNT) {
        return;
    }
    p->state = state;
    p->state_ms = 0;
    p->duration_ms = duration_ms;
    p->last_hop_ms = 0;
    p->visited |= 1u << state;
    p->transitions++;
    if (state == PET_LOVE || state == PET_PLAY || state == PET_SURPRISE) {
        p->lift_speed = 44;
    }
    if (state == PET_WALK || state == PET_PLAY) {
        p->target_x = 31 + random_next(p) % 73;
        if (fabsf(p->target_x - p->x) < 15) {
            p->target_x = p->x < 67 ? 99 : 35;
        }
    }
}
void pet_init(pet_t *p, uint32_t seed)
{
    memset(p, 0, sizeof(*p));
    p->rng = seed ? seed : 0x6d69736fu;
    p->x = p->target_x = 67;
    p->facing = 1;
    p->next_blink_ms = 2400;
    pet_set_state(p, PET_WAVE, 2400);
}
void pet_set_tilt(pet_t *p, float tilt)
{
    p->tilt = isfinite(tilt) ? clampf(tilt, -1, 1) : 0;
}
void pet_event(pet_t *p, pet_event_t event, int x, int y)
{
    if (event < PET_TAP || event > PET_SHAKE) {
        return;
    }
    if (event == PET_SHAKE && p->shake_cooldown_ms) {
        return;
    }
    p->interactions++;
    p->food = false;
    if (event == PET_HOLD) {
        p->manual_sleep = !p->manual_sleep;
        pet_set_state(p, p->manual_sleep ? PET_SLEEP : PET_WAVE, p->manual_sleep ? 60000 : 2400);
        return;
    }
    p->manual_sleep = false;
    if (event == PET_SHAKE) {
        p->shake_cooldown_ms = 6000;
        pet_set_state(p, PET_SURPRISE, 2600);
    } else if (event == PET_SWIPE) {
        pet_set_state(p, PET_PLAY, 4200);
        p->target_x = clampf((float)x, 30, 104);
    } else if (p->state == PET_SLEEP || (fabsf(x - p->x) <= 22 && y >= 8 && y <= 54)) {
        pet_set_state(p, PET_LOVE, 2600);
    } else {
        // Allow a full-width trip, including acceleration and the eased arrival.
        pet_set_state(p, PET_WALK, 8000);
        p->food = true;
        p->food_x = clampf((float)x, 29, 105);
        p->target_x = p->food_x;
    }
}
static void next_activity(pet_t *p)
{
    static const pet_state_t choices[] = {PET_IDLE, PET_WALK, PET_SNIFF, PET_WAVE,
                                          PET_PLAY, PET_EAT,  PET_SLEEP};
    unsigned i = random_next(p) % (sizeof(choices) / sizeof(choices[0]));
    pet_state_t s = choices[i];
    if (s == p->state) {
        s = PET_IDLE;
    }
    uint32_t duration = 3200 + random_next(p) % 3000;
    if (s == PET_SLEEP) {
        duration = 11000;
    }
    if (s == PET_WALK) {
        duration = 7000;
    }
    p->food = false;
    pet_set_state(p, s, duration);
}
void pet_step(pet_t *p, uint32_t dt_ms)
{
    /* Bound integration after a debugger stop or a delayed hardware frame. */
    if (dt_ms > 100) {
        dt_ms = 100;
    }
    const float dt = dt_ms * .001f;
    p->uptime_ms += dt_ms;
    p->state_ms += dt_ms;
    p->blink_ms += dt_ms;
    if (p->blink_ms >= p->next_blink_ms + 140) {
        p->blink_ms = 0;
        p->next_blink_ms = 2000 + random_next(p) % 4200;
    }
    p->shake_cooldown_ms = p->shake_cooldown_ms > dt_ms ? p->shake_cooldown_ms - dt_ms : 0;
    if (p->state_ms >= p->duration_ms && !p->manual_sleep) {
        next_activity(p);
    }

    float velocity = 0;
    if (p->state == PET_WALK || p->state == PET_PLAY) {
        float distance = p->target_x - p->x;
        float max_speed = p->state == PET_PLAY ? 22 : 13;
        velocity = clampf(distance * 3, -max_speed, max_speed);
        if (fabsf(distance) < 1 && fabsf(p->speed) < 3) {
            if (p->food) {
                p->food = false;
                pet_set_state(p, PET_EAT, 3200);
            } else if (p->state == PET_WALK) {
                pet_set_state(p, PET_SNIFF, 2600);
            } else {
                p->target_x = p->x < 67 ? 99 : 35;
            }
        }
    }
    p->speed += (velocity - p->speed) * (dt / (.12f + dt));
    p->x = clampf(p->x + p->speed * dt, 28, 106);
    if (fabsf(p->speed) > 1) {
        p->facing = p->speed > 0 ? 1 : -1;
    }
    p->gait = fmodf(p->gait + fabsf(p->speed) * dt * .55f, 6.2831853f);
    if ((p->state == PET_LOVE || p->state == PET_PLAY) && p->state_ms - p->last_hop_ms >= 800 &&
        p->state_ms + 700 < p->duration_ms && p->lift <= .01f) {
        p->lift_speed = 40;
        p->last_hop_ms = p->state_ms;
    }
    if (p->lift > 0 || p->lift_speed > 0) {
        p->lift += p->lift_speed * dt - 80 * dt * dt;
        p->lift_speed -= 160 * dt;
        if (p->lift <= 0) {
            p->lift = 0;
            p->lift_speed = 0;
        }
    }
    p->lean += ((p->tilt * 2 + p->speed * .065f) - p->lean) * (dt / (.18f + dt));
    p->gaze +=
        ((p->tilt * 2 + (fabsf(p->speed) > 2 ? p->facing : 0)) - p->gaze) * (dt / (.18f + dt));
    p->sleep_blend += ((p->state == PET_SLEEP ? 1 : 0) - p->sleep_blend) * (dt / (.25f + dt));
}
