// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <stdbool.h>
#include <stdint.h>

enum { WAYFARER_WIDTH = 536, WAYFARER_HEIGHT = 240 };

typedef enum {
    WAYFARER_EVENT_NONE = 0,
    WAYFARER_EVENT_TRAFFIC,
    WAYFARER_EVENT_ALERT,
    WAYFARER_EVENT_PLANET,
    WAYFARER_EVENT_WARP,
    WAYFARER_EVENT_NEBULA,
    WAYFARER_EVENT_CONTACT,
    WAYFARER_EVENT_STATION,
    WAYFARER_EVENT_CONVOY,
    WAYFARER_EVENT_COMET,
    WAYFARER_EVENT_ECLIPSE,
    WAYFARER_EVENT_COUNT,
} wayfarer_event_t;

typedef enum {
    WAYFARER_BODY_NONE, WAYFARER_BODY_WORLD, WAYFARER_BODY_SHIP,
    WAYFARER_BODY_STATION, WAYFARER_BODY_COMET, WAYFARER_BODY_ECLIPSE, WAYFARER_BODY_ASTEROID
} wayfarer_body_type_t;

enum { WAYFARER_BODY_CAPACITY = 12 };
typedef struct {
    uint64_t born_travel_q8;
    uint32_t born_ms, phase_started_ms, phase_duration_ms;
    int16_t x, y, z, vx, vy, vz, size;
    uint8_t type, model, angle;
    bool phase_active;
} wayfarer_body_t;

typedef struct {
    int x, y, z, width;
    unsigned light;
    bool visible;
} wayfarer_projection_t;

typedef struct {
    uint32_t seed;
    uint32_t rng;
    uint64_t travel_q8;
    uint32_t updated_ms;
    uint16_t speed_q8;
    uint16_t events_remaining;
    uint32_t next_event_ms;
    uint32_t event_started_ms;
    uint32_t event_duration_ms;
    uint32_t ping_started_ms;
    wayfarer_body_t bodies[WAYFARER_BODY_CAPACITY];
    uint32_t leg;
    int16_t focus_x, focus_y;
    uint8_t journey_stage, local_event;
    int16_t event_y;
    uint8_t event;
    uint8_t previous_event;
    uint8_t variant;
    int8_t direction;
    bool ping_active;
} wayfarer_scene_t;

void wayfarer_scene_init(wayfarer_scene_t *scene, uint32_t seed);
void wayfarer_scene_update(wayfarer_scene_t *scene, uint32_t now_ms, bool tap);
void wayfarer_scene_begin_event(wayfarer_scene_t *scene, uint8_t event, uint32_t now_ms,
                                uint32_t duration_ms, uint8_t variant, int direction);
wayfarer_projection_t wayfarer_scene_project_body(const wayfarer_scene_t *scene,
                                                  const wayfarer_body_t *body, uint32_t now_ms);
void wayfarer_scene_render_strip(const wayfarer_scene_t *scene, const uint16_t *background,
                                 uint32_t now_ms, int y0, int rows, uint16_t *pixels);
