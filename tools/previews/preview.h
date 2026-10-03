// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int preview_parameters(int argc, char **argv, unsigned *frames, unsigned *fps)
{
    if (argc != 3) {
        return 0;
    }
    char *end;
    unsigned long count = strtoul(argv[1], &end, 10);
    if (*end || count == 0 || count > 2400) {
        return 0;
    }
    unsigned long rate = strtoul(argv[2], &end, 10);
    if (*end || rate == 0 || rate > 40) {
        return 0;
    }
    *frames = (unsigned)count;
    *fps = (unsigned)rate;
    return 1;
}

static int preview_write(const uint16_t *pixels, size_t count)
{
    static uint8_t rgb[536 * 240 * 3];
    if (count > sizeof(rgb) / 3) {
        return 0;
    }
    for (size_t i = 0; i < count; ++i) {
        uint16_t color = pixels[i];
        rgb[i * 3] = ((color >> 11) & 31) * 255 / 31;
        rgb[i * 3 + 1] = ((color >> 5) & 63) * 255 / 63;
        rgb[i * 3 + 2] = (color & 31) * 255 / 31;
    }
    return fwrite(rgb, 3, count, stdout) == count;
}
