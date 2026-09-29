#pragma once
#include <stdbool.h>
#include <stdint.h>

enum { PET_W = 134, PET_H = 60, PET_SCALE = 4,
       DISPLAY_W = PET_W * PET_SCALE, DISPLAY_H = PET_H * PET_SCALE };
typedef enum {
    PET_IDLE, PET_WALK, PET_SNIFF, PET_EAT, PET_SLEEP,
    PET_LOVE, PET_PLAY, PET_SURPRISE, PET_WAVE, PET_STATE_COUNT
} pet_state_t;
typedef enum { PET_TAP, PET_SWIPE, PET_HOLD, PET_SHAKE } pet_event_t;

/* Hardware independent, fixed memory simulation. Distances are logical pixels;
   time is milliseconds. The renderer never changes the simulation. */
typedef struct {
    uint64_t uptime_ms;
    uint32_t rng, state_ms, duration_ms, blink_ms, next_blink_ms, shake_cooldown_ms;
    uint32_t interactions, transitions, visited;
    pet_state_t state;
    float x, target_x, speed, gait, lift, lift_speed, lean, gaze, sleep_blend;
    float tilt, food_x;
    int facing;
    bool food, manual_sleep;
    uint32_t last_hop_ms;
} pet_t;

void pet_init(pet_t *pet, uint32_t seed);
void pet_step(pet_t *pet, uint32_t dt_ms);
void pet_event(pet_t *pet, pet_event_t event, int x, int y);
void pet_set_tilt(pet_t *pet, float tilt);
/* Used by the hardware diagnostic console and host animation previews. */
void pet_set_state(pet_t *pet, pet_state_t state, uint32_t duration_ms);
const char *pet_state_name(pet_state_t state);
