// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float view_x, view_y, roll, energy;
    float neutral[3], gravity[3], previous[3];
    float angle_x, angle_y;
    bool ready;
} motion_filter_t;
/* Acceleration in g, angular velocity in degrees/second, dt in seconds. */
bool motion_update(motion_filter_t *m, const float accel[3], const float gyro[3], float dt);
