// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <stdbool.h>

typedef struct {
    float support[3];
    /* Slowly tracked pose, used only to emphasize a brief walking sway. */
    float tilt_reference[3];
    float linear[3];
    /* Leaky integration of in-plane acceleration preserves a brief slide
     * through its constant-velocity interval without long-term drift. */
    float translation_velocity[2];
    float strong_translation_velocity_x;
    float gyro_bias[3];
    float gyro_sum[3];
    float stationary_reference[3];
    float stationary_magnitude_reference;
    float pitch_guard;
    float previous_gravity_x;
    float tilt_sweep_x;
    /* Measured specific-force magnitude while the board is still.  QMI scale
       tolerance must not turn into a permanent linear acceleration. */
    float rest_gravity_magnitude;
    float stationary_seconds;
    unsigned stationary_samples;
    bool gyro_bias_ready;
    bool gravity_history_ready;
    bool initialized;
} motion_filter;

typedef struct {
    /* Screen coordinate system: +x right, +y down.  Gravity and the device
     * acceleration remain distinct all the way through the application. */
    float gravity[3];
    float linear_device_acceleration[3];
    float fluid_acceleration[3];
    float force[2];              /* gravity plus opposite device acceleration, m/s^2 */
    float translation_target[2]; /* bounded enclosure-relative flow target, m/s;
                                   x also includes a brief roll response */
    float translation_scale[2];  /* simulation cells / physical m, based on pose and axis */
} motion_output;

/* accel is m/s^2 specific force, gyro is rad/s.  gravity_map converts raw
 * QMI axes directly into screen gravity axes; it is applied explicitly by the
 * board configuration and is never inferred from the startup pose. */
bool motion_update(motion_filter *filter, const float accel[3], const float gyro[3], float dt,
                   const float gravity_map[3][3], motion_output *out);
