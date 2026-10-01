#pragma once

#include <stdbool.h>
#include <stdint.h>

enum { MECH_W = 134, MECH_H = 60, MECH_SCALE = 4,
       DISPLAY_W = MECH_W * MECH_SCALE, DISPLAY_H = MECH_H * MECH_SCALE };

typedef enum { MECH_TITLE, MECH_IDLE, MECH_SCAN, MECH_POWERUP, MECH_LAUNCH,
               MECH_RETURN, MECH_STATE_COUNT } mech_state_t;

typedef struct {
    uint64_t uptime_ms;
    uint32_t state_ms, cycles, interactions;
    mech_state_t state;
    bool manual_launch;
} mech_t;

void mech_init(mech_t *mech);
void mech_step(mech_t *mech, uint32_t dt_ms);
void mech_activate(mech_t *mech);
const char *mech_state_name(mech_state_t state);
