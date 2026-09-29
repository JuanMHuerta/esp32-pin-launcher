#include "input.h"
#include <math.h>
#include <stdlib.h>

bool touch_decode(const uint8_t r[5], bool *down, int *x, int *y)
{
    unsigned count = r[0] & 15, flag = r[1] >> 6;
    if (count > 1 || flag == 3) return false;
    *down = count && flag != 1;
    if (!*down) return true;
    unsigned raw_y = ((r[1] & 15) << 8) | r[2];
    unsigned raw_x = ((r[3] & 15) << 8) | r[4];
    if (raw_y >= DISPLAY_H || raw_x >= DISPLAY_W) return false;
    *x = raw_x / PET_SCALE;
    *y = (DISPLAY_H - 1 - raw_y) / PET_SCALE;
    return true;
}
bool gesture_update(gesture_t *g, bool down, int x, int y,
                    uint32_t now_ms, input_event_t *event)
{
    if (down && !g->down) {
        *g = (gesture_t){.down = true, .start_x = x, .start_y = y, .x = x, .y = y,
                         .started_ms = now_ms};
    }
    if (!g->down) return false;
    if (down) {
        g->x = x; g->y = y;
        int distance = abs(x - g->start_x) + abs(y - g->start_y);
        if (distance > g->travel) g->travel = distance;
        if (!g->held && g->travel < 12 && now_ms - g->started_ms >= 800) {
            g->held = true;
            *event = (input_event_t){PET_HOLD, x, y};
            return true;
        }
        return false;
    }
    g->down = false;
    if (g->held || now_ms - g->started_ms < 5) return false;
    *event = (input_event_t){g->travel >= 12 ? PET_SWIPE : PET_TAP, g->x, g->y};
    return true;
}
bool motion_update(motion_t *m, const float a[3], uint32_t dt_ms)
{
    for (int i = 0; i < 3; ++i)
        if (!isfinite(a[i]) || fabsf(a[i]) > 8.1f) return false;
    if (!m->ready) {
        for (int i = 0; i < 3; ++i) m->gravity[i] = a[i];
        m->neutral_y = a[1];
        m->ready = true;
    }
    /* Power-on samples can still contain the previous configuration's data.
       Establish the resting pose before allowing any motion events. */
    if (m->samples < 10) {
        for (int i = 0; i < 3; ++i) m->gravity[i] = a[i];
        m->neutral_y = a[1];
        m->samples++;
        m->tilt = 0;
        return false;
    }
    if (dt_ms > 100) dt_ms = 100;
    float alpha = (float)dt_ms / (220 + dt_ms), energy = 0;
    for (int i = 0; i < 3; ++i) {
        float d = a[i] - m->gravity[i];
        energy += d * d;
        m->gravity[i] += d * alpha;
    }
    m->tilt = fminf(1, fmaxf(-1, (m->gravity[1] - m->neutral_y) * 1.6f));
    m->samples++;
    m->cooldown_ms = m->cooldown_ms > dt_ms ? m->cooldown_ms - dt_ms : 0;
    m->active_ms = energy > .85f ? m->active_ms + dt_ms : 0;
    if (m->active_ms >= 40 && !m->cooldown_ms) {
        m->active_ms = 0;
        m->cooldown_ms = 6000;
        return true;
    }
    return false;
}
