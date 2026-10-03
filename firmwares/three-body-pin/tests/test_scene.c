// SPDX-License-Identifier: GPL-3.0-only
#include "scene.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void conservation(const orbit_t *s)
{
    double x = 0, y = 0, vx = 0, vy = 0;
    for (int i = 0; i < 3; i++) {
        const orbit_body_t *b = &s->body[i];
        assert(isfinite(b->x) && isfinite(b->y) && isfinite(b->vx) && isfinite(b->vy));
        x += b->x * b->m;
        y += b->y * b->m;
        vx += b->vx * b->m;
        vy += b->vy * b->m;
    }
    assert(fabs(x) < 1e-8 && fabs(y) < 1e-8 && fabs(vx) < 1e-9 && fabs(vy) < 1e-9);
    assert(fabs((orbit_energy(s) - s->energy0) / s->energy0) < .0001);
}
int main(void)
{
    orbit_t *s = malloc(sizeof(*s));
    assert(s);
    for (unsigned n = 0; n < ORBIT_PRESETS; n++) {
        orbit_init(s, n);
        orbit_body_t start[3];
        memcpy(start, s->body, sizeof(start));
        if (!n) {
            orbit_advance(s, 6.32591398);
            for (int i = 0; i < 3; i++) {
                assert(hypot(s->body[i].x - start[i].x, s->body[i].y - start[i].y) < 1e-5);
                assert(hypot(s->body[i].vx - start[i].vx, s->body[i].vy - start[i].vy) < 1e-5);
            }
            conservation(s);
        }
        orbit_init(s, n);
        for (unsigned frame = 0; frame < 1799; frame++) {
            orbit_step(s, 25);
            conservation(s);
            assert(isfinite(s->scale) && s->scale > 0 && s->scale <= 64);
        }
        printf("%s: t=%.3f relative energy drift=%.9g\n", orbit_name(n), s->time,
               (orbit_energy(s) - s->energy0) / fabs(s->energy0));
        assert(s->count == ORBIT_TRAIL);
        assert(s->elapsed_ms == 44975);
        assert(s->preset == n);
        orbit_step(s, 25);
        assert(s->preset == (n + 1) % ORBIT_PRESETS && s->elapsed_ms == 0);
    }
    orbit_init(s, 0);
    orbit_advance(s, NAN);
    assert(s->time == 0);
    orbit_step(s, 5000);
    assert(s->elapsed_ms == 100);
    /* Display transfer covers every physical pixel, swaps RGB565 exactly once,
     * and rejects incomplete logical rows. Guard words detect overruns. */
    uint16_t *p = malloc((PIN_W * PIN_H + 2) * sizeof(*p));
    assert(p);
    p[0] = p[PIN_W * PIN_H + 1] = 0x1234;
    orbit_paint(s, p + 1);
    assert(p[0] == 0x1234 && p[PIN_W * PIN_H + 1] == 0x1234);
    uint16_t *out = malloc((PIN_DISPLAY_W * 40 + 2) * sizeof(*out));
    assert(out);
    out[0] = out[PIN_DISPLAY_W * 40 + 1] = 0xbeef;
    for (int y = 0; y < PIN_DISPLAY_H; y += 40) {
        assert(pin_expand_strip(p + 1, y, 40, out + 1));
        for (int r = 0; r < 40; r++) {
            for (int x = 0; x < PIN_DISPLAY_W; x++) {
                assert(out[1 + r * PIN_DISPLAY_W + x] ==
                       __builtin_bswap16(p[1 + ((y + r) / 2) * PIN_W + x / 2]));
            }
        }
    }
    assert(out[0] == 0xbeef && out[PIN_DISPLAY_W * 40 + 1] == 0xbeef);
    assert(!pin_expand_strip(p, 1, 40, out));
    assert(!pin_expand_strip(p, 220, 40, out));
    assert(!pin_expand_strip(p, 0, 3, out));
    free(out);
    free(p);
    free(s);
    puts("three-body tests passed");
}
