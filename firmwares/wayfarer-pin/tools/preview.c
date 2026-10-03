// SPDX-License-Identifier: GPL-3.0-only
#include "scene.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint16_t background[WAYFARER_WIDTH * WAYFARER_HEIGHT];
static uint16_t frame[WAYFARER_WIDTH * WAYFARER_HEIGHT];
static uint8_t rgb[WAYFARER_WIDTH * WAYFARER_HEIGHT * 3];

int main(int argc, char **argv)
{
    if (argc != 8) {
        return 2;
    }
    unsigned frame_count = (unsigned)strtoul(argv[3], NULL, 10);
    unsigned fps = (unsigned)strtoul(argv[4], NULL, 10);
    int event = atoi(argv[5]);
    if (!frame_count || !fps || event >= WAYFARER_EVENT_COUNT) {
        return 2;
    }
    FILE *input = fopen(argv[1], "rb");
    FILE *output = fopen(argv[2], "wb");
    if (!input || !output) {
        return 3;
    }
    size_t count = fread(background, sizeof(uint16_t), WAYFARER_WIDTH * WAYFARER_HEIGHT, input);
    fclose(input);
    if (count != WAYFARER_WIDTH * WAYFARER_HEIGHT) {
        return 4;
    }

    wayfarer_scene_t scene;
    wayfarer_scene_init(&scene, (uint32_t)strtoul(argv[7], NULL, 10));
    if (event >= 0) {
        if (event==WAYFARER_EVENT_PLANET) { scene.bodies[0].model=(uint8_t)(atoi(argv[6])%6); }
        wayfarer_scene_begin_event(&scene,(uint8_t)event,0,frame_count*1000/fps+1,
                                    (uint8_t)atoi(argv[6]),1);
        scene.next_event_ms = 3600000;
    }
    for (uint32_t index = 0; index < frame_count; ++index) {
        uint32_t now_ms = index * 1000 / fps;
        wayfarer_scene_update(&scene, now_ms, false);
        wayfarer_scene_render_strip(&scene, background, now_ms, 0, WAYFARER_HEIGHT, frame);
        for (size_t i = 0; i < WAYFARER_WIDTH * WAYFARER_HEIGHT; ++i) {
            uint16_t color = frame[i];
            rgb[i * 3] = (uint8_t)(((color >> 11) & 31) * 255 / 31);
            rgb[i * 3 + 1] = (uint8_t)(((color >> 5) & 63) * 255 / 63);
            rgb[i * 3 + 2] = (uint8_t)((color & 31) * 255 / 31);
        }
        if (fwrite(rgb, 1, sizeof(rgb), output) != sizeof(rgb)) {
            return 5;
        }
    }
    fclose(output);
    return 0;
}
