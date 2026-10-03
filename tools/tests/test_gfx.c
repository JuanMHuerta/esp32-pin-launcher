// SPDX-License-Identifier: GPL-3.0-only
#include "pin_gfx.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static uint16_t guarded[PIN_W * PIN_H + 2];

int main(void)
{
    uint16_t *pixels = guarded + 1;
    guarded[0] = guarded[PIN_W * PIN_H + 1] = 0xbeef;
    pin_rect(pixels, -1, -1, 3, 3, 0xffff);
    assert(pixels[0] == 0xffff && pixels[PIN_W + 1] == 0xffff);
    assert(pixels[2] == 0 && pixels[2 * PIN_W] == 0);
    pin_rect(pixels, INT_MAX, INT_MAX, INT_MAX, INT_MAX, 1);
    pin_rect(pixels, INT_MIN, INT_MIN, INT_MAX, INT_MAX, 1);
    pin_rect(pixels, 0, 0, -1, -1, 1);
    pin_line(pixels, INT_MIN, 0, 0, 0, 1);
    pin_line(pixels, 0, 0, INT_MAX, INT_MAX, 1);
    pin_line(pixels, -5, 5, 5, 5, 0x1234);
    for (int x = 0; x <= 5; ++x) {
        assert(pixels[5 * PIN_W + x] == 0x1234);
    }
    assert(guarded[0] == 0xbeef && guarded[PIN_W * PIN_H + 1] == 0xbeef);

    uint16_t strip[PIN_DISPLAY_W * 2 + 2];
    strip[0] = strip[PIN_DISPLAY_W * 2 + 1] = 0xbeef;
    pixels[0] = 0xf800;
    assert(pin_expand_strip(pixels, 0, 2, strip + 1));
    assert(strip[1] == 0x00f8 && strip[2] == 0x00f8);
    assert(strip[1 + PIN_DISPLAY_W] == strip[1]);
    assert(!pin_expand_strip(pixels, 1, 2, strip + 1));
    assert(!pin_expand_strip(pixels, 238, 4, strip + 1));
    assert(strip[0] == 0xbeef && strip[PIN_DISPLAY_W * 2 + 1] == 0xbeef);
    assert(pin_rgb(-1, -1, -1) == 0);
    assert(pin_rgb(256, 256, 256) == 0xffff);
    puts("graphics tests passed: clipping, extreme coordinates, strip bounds and byte order");
    return 0;
}
