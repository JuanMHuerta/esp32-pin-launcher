#include "mech.h"

static const uint32_t durations[MECH_STATE_COUNT] = {
    1800, 10500, 2600, 2800, 4100, 3000,
};

static void enter(mech_t *m, mech_state_t state)
{
    m->state = state;
    m->state_ms = 0;
}

void mech_init(mech_t *m)
{
    *m = (mech_t){.state = MECH_TITLE};
}

void mech_activate(mech_t *m)
{
    if (m->state == MECH_TITLE || m->state == MECH_LAUNCH) return;
    m->interactions++;
    m->manual_launch = true;
    enter(m, MECH_POWERUP);
}

void mech_step(mech_t *m, uint32_t dt_ms)
{
    m->uptime_ms += dt_ms;
    m->state_ms += dt_ms;
    if (m->state_ms < durations[m->state]) return;
    /* Keep fractional frame time rather than making every phase one frame
       longer. This matters both for a smooth loop and deterministic tests. */
    m->state_ms -= durations[m->state];
    switch (m->state) {
    case MECH_TITLE: m->state = MECH_IDLE; break;
    case MECH_IDLE: m->state = MECH_SCAN; break;
    case MECH_SCAN: m->state = MECH_POWERUP; break;
    case MECH_POWERUP: m->state = MECH_LAUNCH; break;
    case MECH_LAUNCH: m->state = MECH_RETURN; break;
    case MECH_RETURN:
        m->cycles++;
        m->manual_launch = false;
        m->state = MECH_IDLE;
        break;
    default: enter(m, MECH_IDLE); break;
    }
}

const char *mech_state_name(mech_state_t state)
{
    static const char *const names[] = {"TITLE", "IDLE", "SCAN", "POWERUP", "LAUNCH", "RETURN"};
    return state < MECH_STATE_COUNT ? names[state] : "UNKNOWN";
}
