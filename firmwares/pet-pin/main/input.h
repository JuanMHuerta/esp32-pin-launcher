#pragma once
#include "pet.h"
#include <stddef.h>

typedef struct {
    bool down, held;
    int start_x, start_y, x, y, travel;
    uint32_t started_ms;
} gesture_t;
typedef struct { pet_event_t type; int x, y; } input_event_t;
bool touch_decode(const uint8_t report[5], bool *down, int *x, int *y);
bool gesture_update(gesture_t *g, bool down, int x, int y,
                    uint32_t now_ms, input_event_t *event);
typedef struct {
    bool ready;
    float gravity[3], neutral_y, tilt;
    uint32_t active_ms, cooldown_ms, samples;
} motion_t;
/* Returns a shake event only after sustained acceleration; readings are in g. */
bool motion_update(motion_t *m, const float accel[3], uint32_t dt_ms);
