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
float pet_daylight(const pet_t *p)
{
    float t = (float)(p->uptime_ms % PET_DAY_MS) / PET_DAY_MS;
    return .5f + .5f * cosf(t * 6.2831853f);
}
const char *pet_state_name(pet_state_t s)
{
    static const char *const names[] = {"idle",    "walk",  "sniff",    "eat",    "sleep",
                                        "love",    "play",  "surprise", "wave",   "groom",
                                        "stretch", "dance", "look",     "balance"};
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
    if (event < PET_TAP || event > PET_SHAKE || (event == PET_SHAKE && p->shake_cooldown_ms)) {
        return;
    }
    p->interactions++;
    p->food = false;
    p->tilt_ms = p->settled_ms = 0;
    p->motion_cooldown_ms = 3500;
    if (event == PET_HOLD) {
        p->manual_sleep = !p->manual_sleep;
        pet_set_state(p, p->manual_sleep ? PET_SLEEP : PET_STRETCH, p->manual_sleep ? 60000 : 2800);
        return;
    }
    p->manual_sleep = false;
    if (event == PET_SHAKE) {
        static const pet_state_t reactions[] = {PET_SURPRISE, PET_DANCE, PET_PLAY};
        p->shake_cooldown_ms = 6000;
        pet_set_state(p, reactions[p->shakes++ % 3], 3600);
    } else if (event == PET_SWIPE) {
        pet_set_state(p, PET_PLAY, 4200);
        p->target_x = clampf((float)x, 30, 104);
    } else if (p->state == PET_SLEEP || (fabsf(x - p->x) <= 22 && y >= 8 && y <= 54)) {
        pet_set_state(p, PET_LOVE, 3000);
    } else {
        pet_set_state(p, PET_WALK, 8000);
        p->food = true;
        p->food_x = clampf((float)x, 29, 105);
        p->target_x = p->food_x;
    }
}
static void next_activity(pet_t *p)
{
    /* Like the newer scene apps, consume a shuffled cycle rather than repeating
       random choices. Every activity has a turn, with no immediate repeats. */
    const uint32_t automatic = ((1u << PET_STATE_COUNT) - 1) &
                               ~((1u << PET_LOVE) | (1u << PET_SURPRISE) | (1u << PET_BALANCE));
    p->activities &= ~(1u << p->state);
    if (!p->activities) {
        p->activities = automatic & ~(1u << p->state);
    }
    unsigned count = 0;
    for (unsigned s = 0; s < PET_STATE_COUNT; ++s) {
        count += (p->activities >> s) & 1u;
    }
    unsigned pick = random_next(p) % count;
    pet_state_t state = PET_IDLE;
    for (unsigned s = 0; s < PET_STATE_COUNT; ++s) {
        if (p->activities & (1u << s)) {
            if (!pick--) {
                state = (pet_state_t)s;
                break;
            }
        }
    }
    p->activities &= ~(1u << state);
    uint32_t duration = 3600 + random_next(p) % 2200;
    if (state == PET_SLEEP) {
        duration = pet_daylight(p) < .35f ? 18000 : 6500;
    }
    if (state == PET_WALK || state == PET_EAT) {
        duration = 8000;
    }
    p->food = false;
    /* Autonomous feeding is a small story: find a berry, walk over, then eat. */
    pet_set_state(p, state == PET_EAT ? PET_WALK : state, duration);
    if (state == PET_EAT) {
        p->food = true;
        p->food_x = p->target_x;
    }
}
static void motion_reaction(pet_t *p, uint32_t dt_ms)
{
    p->motion_cooldown_ms = p->motion_cooldown_ms > dt_ms ? p->motion_cooldown_ms - dt_ms : 0;
    if (p->manual_sleep || p->food || p->state == PET_EAT || p->motion_cooldown_ms) {
        p->tilt_ms = p->settled_ms = 0;
        return;
    }
    if (p->state == PET_BALANCE) {
        p->settled_ms = fabsf(p->tilt) < .28f ? p->settled_ms + dt_ms : 0;
        if (p->settled_ms >= 450 || p->state_ms >= p->duration_ms) {
            p->motion_cooldown_ms = 3000;
            pet_set_state(p, PET_WAVE, 2200);
        }
    } else {
        p->tilt_ms = fabsf(p->tilt) > .62f ? p->tilt_ms + dt_ms : 0;
        if (p->tilt_ms >= 550) {
            p->tilt_ms = p->settled_ms = 0;
            pet_set_state(p, PET_BALANCE, 8000);
        }
    }
}
void pet_step(pet_t *p, uint32_t dt_ms)
{
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
    motion_reaction(p, dt_ms);
    if (p->state_ms >= p->duration_ms && !p->manual_sleep) {
        if (p->state == PET_SLEEP) {
            pet_set_state(p, PET_STRETCH, 2800);
        } else {
            next_activity(p);
        }
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
    if ((p->state == PET_LOVE || p->state == PET_PLAY || p->state == PET_DANCE) &&
        p->state_ms - p->last_hop_ms >= (p->state == PET_DANCE ? 1000u : 800u) &&
        p->state_ms + 700 < p->duration_ms && p->lift <= .01f) {
        p->lift_speed = p->state == PET_DANCE ? 30 : 40;
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
    float lean = p->tilt * (p->state == PET_BALANCE ? 5 : 3) + p->speed * .065f;
    float gaze = p->tilt * 3 + (fabsf(p->speed) > 2 ? p->facing : 0);
    if (p->state == PET_LOOK || p->state == PET_IDLE) {
        gaze += sinf((float)(p->uptime_ms % 8000) * .000785398f) * 2;
    }
    p->lean += (lean - p->lean) * (dt / (.18f + dt));
    p->gaze += (clampf(gaze, -3, 3) - p->gaze) * (dt / (.18f + dt));
    p->sleep_blend += ((p->state == PET_SLEEP ? 1 : 0) - p->sleep_blend) * (dt / (.25f + dt));
}
