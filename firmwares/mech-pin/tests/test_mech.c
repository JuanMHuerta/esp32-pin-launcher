#include "../main/mech.h"
#include "../main/paint.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void advance(mech_t *m, unsigned ms)
{
    while (ms) { unsigned step = ms > 17 ? 17 : ms; mech_step(m, step); ms -= step; }
}

int main(void)
{
    mech_t m;
    mech_init(&m);
    assert(m.state == MECH_TITLE);
    advance(&m, 1800);
    assert(m.state == MECH_IDLE);
    mech_activate(&m);
    assert(m.state == MECH_POWERUP && m.interactions == 1);
    advance(&m, 2800 + 4100 + 3000);
    assert(m.state == MECH_IDLE && m.cycles == 1);
    uint16_t frame[MECH_W * MECH_H], strip[DISPLAY_W * 40];
    memset(frame, 0xa5, sizeof(frame));
    mech_paint(&m, frame);
    assert(mech_expand_strip(frame, 0, 40, strip));
    assert(!mech_expand_strip(frame, 1, 40, strip));
    assert(!mech_expand_strip(frame, 0, 39, strip));
    puts("mech simulation and renderer checks passed");
    return 0;
}
