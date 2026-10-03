// SPDX-License-Identifier: GPL-3.0-only
#include "fluid.h"
#include "motion.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static const float identity[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
static const float g = 9.80665f;

static void run_tilt(bool pitch, bool reversed_gyro)
{
    motion_filter filter = {0};
    motion_output motion = {0};
    fluid_t *fluid = fluid_create();
    assert(fluid);
    const float dt = 1.0f / IMU_HZ;
    const float angle_max = 12.0f * 3.14159265f / 180.0f;
    const float rate = angle_max / 0.6f; /* deliberately slow hand tilt */
    float next_physics = 0.0f;
    float peak_target = 0.0f, peak_linear = 0.0f, peak_speed = 0.0f;
    float force_at_hold = 0.0f, force_at_reverse = 0.0f;
    float bias_at_hold = 0.0f;
    for (int sample = 0; sample < 7 * IMU_HZ; ++sample) {
        float t = sample * dt;
        float angle = 0.0f, angular_rate = 0.0f;
        if (t >= 3.0f && t < 3.6f) {
            angle = rate * (t - 3.0f);
            angular_rate = rate;
        } else if (t >= 3.6f && t < 4.3f) {
            angle = angle_max;
        } else if (t >= 4.3f && t < 4.9f) {
            angle = angle_max - rate * (t - 4.3f);
            angular_rate = -rate;
        }
        float accel[3] = {pitch ? 0.0f : g * sinf(angle), g * cosf(angle),
                          pitch ? g * sinf(angle) : 0.0f};
        float gyro[3] = {pitch ? -angular_rate : 0.0f, 0.0f, pitch ? 0.0f : angular_rate};
        if (reversed_gyro) {
            gyro[0] = -gyro[0];
            gyro[2] = -gyro[2];
        }
        gyro[pitch ? 0 : 2] += 0.23f; /* measured board-scale static offset */
        assert(motion_update(&filter, accel, gyro, dt, identity, &motion));
        if (fabsf(t - 4.2f) < dt * 0.5f) {
            bias_at_hold = filter.gyro_bias[pitch ? 0 : 2];
        }
        if (t >= 3.0f && t < 5.0f) {
            int axis = pitch ? 1 : 0;
            float target = fabsf(motion.translation_target[axis] * motion.translation_scale[axis]);
            if (target > peak_target) {
                peak_target = target;
            }
            float linear = fabsf(motion.fluid_acceleration[axis]);
            if (linear > peak_linear) {
                peak_linear = linear;
            }
            if (fabsf(t - 3.8f) < dt * 0.5f) {
                force_at_hold = motion.force[axis];
            }
            if (fabsf(t - 4.5f) < dt * 0.5f) {
                force_at_reverse = motion.force[axis];
            }
        }
        if (t + 0.0001f >= next_physics) {
            assert(fluid_step_with_translation(
                fluid, motion.force[0] * GRAVITY / g, motion.force[1] * GRAVITY / g,
                motion.translation_target[0] * motion.translation_scale[0],
                motion.translation_target[1] * motion.translation_scale[1]));
            fluid_metrics metrics;
            fluid_get_metrics(fluid, &metrics);
            if (t >= 3.0f && t < 5.0f && metrics.average_speed > peak_speed) {
                peak_speed = metrics.average_speed;
            }
            next_physics += 1.0f / PHYSICS_HZ;
        }
    }
    fluid_metrics metrics;
    fluid_get_metrics(fluid, &metrics);
    printf("%s gyro=%s target=%.3f linear=%.3f speed=%.3f hold_force=%.3f reverse_force=%.3f "
           "bias=%.3f rest=%.3f resets=%u\n",
           pitch ? "pitch" : "roll", reversed_gyro ? "reversed" : "correct", peak_target,
           peak_linear, peak_speed, force_at_hold, force_at_reverse, bias_at_hold,
           metrics.average_speed, metrics.resets);
    assert(filter.gyro_bias_ready);
    assert(fabsf(bias_at_hold - 0.23f) < 0.05f);
    if (pitch) {
        assert(peak_target < 0.6f);
        assert(peak_speed < 0.5f);
    } else {
        assert(force_at_hold > 3.0f);
    }
    assert(metrics.resets == 0);
    fluid_destroy(fluid);
}

static void run_vertical_slide_with_pitch(void)
{
    motion_filter filter = {0};
    motion_output motion = {0};
    const float dt = 1.0f / IMU_HZ;
    float peak_target = 0.0f;
    for (int sample = 0; sample < 5 * IMU_HZ; ++sample) {
        float t = sample * dt;
        float phase = t - 3.0f;
        float angle =
            phase >= 0.0f && phase < 1.0f ? 0.05f * sinf(2.0f * 3.14159265f * 1.4f * phase) : 0.0f;
        float angular_rate =
            phase >= 0.0f && phase < 1.0f
                ? 0.05f * 2.0f * 3.14159265f * 1.4f * cosf(2.0f * 3.14159265f * 1.4f * phase)
                : 0.0f;
        float slide = phase >= 0.0f && phase < 0.12f
                          ? 0.25f * g
                          : (phase >= 0.35f && phase < 0.47f ? -0.25f * g : 0.0f);
        float accel[3] = {0.0f, g * cosf(angle) + slide, g * sinf(angle)};
        float gyro[3] = {-angular_rate + 0.23f, 0.0f, 0.0f};
        assert(motion_update(&filter, accel, gyro, dt, identity, &motion));
        float target = fabsf(motion.translation_target[1] * motion.translation_scale[1]);
        if (target > peak_target) {
            peak_target = target;
        }
    }
    printf("vertical slide with pitch target=%.3f\n", peak_target);
    assert(peak_target > 3.0f);
}

static void run_very_slow_roll(void)
{
    motion_filter filter = {0};
    motion_output motion = {0};
    const float dt = 1.0f / IMU_HZ;
    const float rate = 3.14159265f / 180.0f; /* one degree per second */
    float bias_during_roll = 0.0f;
    for (int sample = 0; sample < 8 * IMU_HZ; ++sample) {
        float t = sample * dt;
        float angle = rate * fminf(fmaxf(t - 3.0f, 0.0f), 4.0f);
        float gyro[3] = {0.0f, 0.0f, 0.23f + (t >= 3.0f && t < 7.0f ? rate : 0.0f)};
        float accel[3] = {g * sinf(angle), g * cosf(angle), 0.0f};
        assert(motion_update(&filter, accel, gyro, dt, identity, &motion));
        if (fabsf(t - 6.9f) < dt * 0.5f) {
            bias_during_roll = filter.gyro_bias[2];
        }
    }
    printf("one-degree-per-second roll bias=%.5f expected=0.23000\n", bias_during_roll);
    assert(filter.gyro_bias_ready);
    assert(fabsf(bias_during_roll - 0.23f) < 0.005f);
    assert(motion.force[0] > 0.5f);
}

int main(void)
{
    run_tilt(false, false);
    run_tilt(true, false);
    run_tilt(true, true);
    run_vertical_slide_with_pitch();
    run_very_slow_roll();
    puts("PASS: roll response, pitch restraint, slow-roll gyro calibration, vertical slide with "
         "pitch");
}
