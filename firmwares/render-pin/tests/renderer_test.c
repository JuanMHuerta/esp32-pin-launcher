#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "renderer.h"

static size_t changed(const uint16_t *a, const uint16_t *b)
{
    size_t count = 0;
    for (size_t i = 0; i < PIN_WIDTH * PIN_HEIGHT; ++i)
        count += a[i] != b[i];
    return count;
}

int main(void)
{
    enum { PIXELS = PIN_WIDTH * PIN_HEIGHT, GUARD = 8 };
    uint16_t *a = calloc(PIXELS + GUARD * 2, sizeof(uint16_t));
    uint16_t *b = calloc(PIXELS + GUARD * 2, sizeof(uint16_t));
    assert(a && b);
    for (int i = 0; i < GUARD; ++i) {
        a[i] = a[PIXELS + GUARD + i] = 0xA55A;
        b[i] = b[PIXELS + GUARD + i] = 0xA55A;
    }
    uint16_t *whole = a + GUARD;
    uint16_t *strips = b + GUARD;
    renderer_t scene;
    renderer_init();
    renderer_prepare(&scene, 2200, 0, 0, 0, 0, 0, 268, 120, -1);
    renderer_strip(&scene, 0, PIN_HEIGHT, whole);
    for (int y = 0; y < PIN_HEIGHT; y += 60)
        renderer_strip(&scene, y, 60, strips + y * PIN_WIDTH);
    assert(memcmp(whole, strips, PIXELS * sizeof(uint16_t)) == 0);

    /* Motion energy alone must never trigger a brightness/glow effect. */
    renderer_prepare(&scene, 2200, 0, 0, 0, 1, 0, 268, 120, -1);
    renderer_strip(&scene, 0, PIN_HEIGHT, strips);
    assert(memcmp(whole, strips, PIXELS * sizeof(uint16_t)) == 0);

    renderer_prepare(&scene, 4200, 0, 0, 0, 0, 0, 268, 120, -1);
    renderer_strip(&scene, 0, PIN_HEIGHT, strips);
    assert(changed(whole, strips) > 1000);

    renderer_prepare(&scene, 2200, 0.7f, -0.5f, 0, 0, 0, 268, 120, -1);
    renderer_strip(&scene, 0, PIN_HEIGHT, strips);
    assert(changed(whole, strips) > 1000);

    renderer_prepare(&scene, 2200, 0, 0, 130, 0.5f, 0, 268, 120, -1);
    renderer_strip(&scene, 0, PIN_HEIGHT, strips);
    assert(changed(whole, strips) > 1000);

    renderer_prepare(&scene, 2200, 0, 0, 0, 0, 1, 268, 120, -1);
    renderer_strip(&scene, 0, PIN_HEIGHT, strips);
    assert(changed(whole, strips) > 1000);

    renderer_prepare(&scene, 2200, 0, 0, 0, 0, 0, 268, 120, 700);
    renderer_strip(&scene, 0, PIN_HEIGHT, strips);
    assert(changed(whole, strips) > 100);

    renderer_prepare(&scene, 2200, 2.0f, -2.0f, 1024.0f, 1.0f,
                     2, 0, 0, 1400);
    for (int y = 0; y < PIN_HEIGHT; y += 60)
        renderer_strip(&scene, y, 60, strips + y * PIN_WIDTH);

    for (int i = 0; i < GUARD; ++i) {
        assert(a[i] == 0xA55A && a[PIXELS + GUARD + i] == 0xA55A);
        assert(b[i] == 0xA55A && b[PIXELS + GUARD + i] == 0xA55A);
    }
    /* Bright stars must travel continuously, including depth recycling. */
    renderer_t previous;
    renderer_prepare(&previous, 0, 0, 0, 0, 0, 0, 268, 120, -1);
    for (int t = 40; t < 120000; t += 40) {
        renderer_prepare(&scene, t, 0, 0, 0, 0, 0, 268, 120, -1);
        int visible_edges = 0;
        for (int i = 0; i < STAR_COUNT; ++i) {
            projected_star_t *p = &scene.stars[i], *q = &previous.stars[i];
            if (p->alpha > 60 && q->alpha > 60) {
                assert(abs(p->x - q->x) < 16);
                assert(abs(p->y - q->y) < 16);
            }
        }
        for (int e = 0; e < EDGE_COUNT; ++e) {
            projected_edge_t *edge = &scene.edges[e];
            projected_star_t *p = &scene.stars[edge->a], *q = &scene.stars[edge->b];
            if (edge->alpha > 20 && p->alpha && q->alpha &&
                p->x >= 0 && p->x < PIN_WIDTH && p->y >= 0 && p->y < PIN_HEIGHT &&
                q->x >= 0 && q->x < PIN_WIDTH && q->y >= 0 && q->y < PIN_HEIGHT)
                ++visible_edges;
        }
        assert(visible_edges >= 5);
        previous = scene;
    }
    free(a);
    free(b);
    puts("renderer checks passed");
    return 0;
}
