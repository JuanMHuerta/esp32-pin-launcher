// SPDX-License-Identifier: GPL-3.0-only
#include "motion.h"
#include <math.h>

static float clamp(float v, float a, float b)
{
    return v < a ? a : v > b ? b : v;
}
static float deadband(float rate)
{
    return fabsf(rate) < 0.8f ? 0 : rate;
}
bool motion_update(motion_filter_t *m, const float accel[3], const float gyro[3], float dt)
{
    if (!m->ready) {
        for (int i = 0; i < 3; ++i) {
            m->neutral[i] = m->gravity[i] = m->previous[i] = accel[i];
        }
        m->ready = true;
    }
    dt = clamp(dt, 0, 0.1f);
    float smoothing = dt / (0.20f + dt);
    float change = 0;
    for (int i = 0; i < 3; ++i) {
        m->gravity[i] += (accel[i] - m->gravity[i]) * smoothing;
        float d = accel[i] - m->previous[i];
        change += d * d;
        m->previous[i] = accel[i];
    }
    /* The QMI axes are turned 90 degrees from the landscape display axes.
       Rotate both gravity and gyro into the same screen coordinate system. */
    float blend = dt / (1.8f + dt);
    float target_x = clamp(m->gravity[1] - m->neutral[1], -1.3f, 1.3f);
    float target_y = clamp(m->neutral[0] - m->gravity[0], -1.3f, 1.3f);
    m->angle_x = clamp((m->angle_x - deadband(gyro[0]) * dt * 0.025f) * (1 - blend), -1.5f, 1.5f);
    m->angle_y = clamp((m->angle_y - deadband(gyro[1]) * dt * 0.025f) * (1 - blend), -1.5f, 1.5f);
    m->view_x = clamp(target_x + m->angle_x, -1.5f, 1.5f);
    m->view_y = clamp(target_y + m->angle_y, -1.5f, 1.5f);
    m->roll += deadband(gyro[2]) * dt * (1024.0f / 360.0f);
    if (m->roll > 512) {
        m->roll -= 1024;
    }
    if (m->roll < -512) {
        m->roll += 1024;
    }
    float speed = sqrtf(gyro[0] * gyro[0] + gyro[1] * gyro[1] + gyro[2] * gyro[2]);
    m->energy += (clamp(speed / 160, 0, 1) - m->energy) * smoothing;
    return change > 0.22f;
}
