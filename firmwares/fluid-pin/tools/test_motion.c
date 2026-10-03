// SPDX-License-Identifier: GPL-3.0-only
#include "motion.h"
#include "fluid_config.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const float identity[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    motion_filter filter = {0};
    motion_output out = {0};
    float acceleration[3] = {0, 9.80665f, 0};
    float gyro[3] = {0};
    for (int i = 0; i < 250; ++i) {
        assert(motion_update(&filter, acceleration, gyro, 0.008f, identity, &out));
    }
    assert(fabsf(out.gravity[0]) < 0.001f && fabsf(out.gravity[1] - 9.80665f) < 0.001f);
    assert(fabsf(out.force[0]) < 0.001f && fabsf(out.force[1] - 9.80665f) < 0.001f);
    assert(fabsf(out.translation_target[0]) < 0.001f && fabsf(out.translation_target[1]) < 0.001f);

    /* A fixed board-specific accelerometer scale error is not motion. */
    memset(&filter, 0, sizeof(filter));
    acceleration[0] = 0.0f;
    acceleration[1] = 10.32f;
    acceleration[2] = 0.0f;
    gyro[0] = gyro[1] = gyro[2] = 0.0f;
    for (int i = 0; i < 250; ++i) {
        assert(motion_update(&filter, acceleration, gyro, 0.008f, identity, &out));
    }
    assert(fabsf(out.linear_device_acceleration[0]) < 0.001f);
    assert(fabsf(out.linear_device_acceleration[1]) < 0.001f);

    /* Startup during a shove must not trap the stillness gate at a false g. */
    memset(&filter, 0, sizeof(filter));
    acceleration[0] = 0.0f;
    acceleration[1] = 13.5f;
    gyro[0] = 0.23f;
    assert(motion_update(&filter, acceleration, gyro, 0.008f, identity, &out));
    acceleration[1] = 9.61f;
    for (int i = 0; i < 375; ++i) {
        assert(motion_update(&filter, acceleration, gyro, 0.008f, identity, &out));
    }
    assert(filter.gyro_bias_ready);
    assert(fabsf(filter.rest_gravity_magnitude - 9.61f) < 0.05f);
    assert(fabsf(filter.gyro_bias[0] - 0.23f) < 0.005f);
    assert(fabsf(out.linear_device_acceleration[0]) < 0.03f);
    assert(fabsf(out.linear_device_acceleration[1]) < 0.03f);

    memset(&filter, 0, sizeof(filter));
    acceleration[0] = 0.0f;
    acceleration[1] = 9.80665f;
    gyro[2] = 1.5707963268f / 2.0f;
    float rotation_target_peak = 0.0f;
    for (int i = 0; i < 250; ++i) {
        float angle = (i + 1) * 0.008f * gyro[2];
        acceleration[0] = 9.80665f * sinf(angle);
        acceleration[1] = 9.80665f * cosf(angle);
        assert(motion_update(&filter, acceleration, gyro, 0.008f, identity, &out));
        assert(fabsf(out.gravity[0] - acceleration[0]) < 0.14f);
        assert(fabsf(out.gravity[1] - acceleration[1]) < 0.14f);
        float target = hypotf(out.translation_target[0], out.translation_target[1]);
        if (target > rotation_target_peak) {
            rotation_target_peak = target;
        }
    }
    /* Rotation now contributes a short, bounded sweep target. */
    printf("rotation target peak=%.3f m/s\n", rotation_target_peak);
    assert(rotation_target_peak > 0.05f && rotation_target_peak < 0.50f);

    memset(&filter, 0, sizeof(filter));
    acceleration[0] = acceleration[1] = 0.0f;
    acceleration[2] = 9.80665f;
    gyro[0] = 0.0f;
    gyro[2] = 0.0f;
    for (int i = 0; i < 250; ++i) {
        assert(motion_update(&filter, acceleration, gyro, 0.008f, identity, &out));
    }
    assert(hypotf(out.force[0], out.force[1]) < 0.001f);

    /* A partial in-plane gravity projection gets the documented modest tilt
       boost, while the full upright case above stays capped at one g. */
    memset(&filter, 0, sizeof(filter));
    acceleration[0] = 0.0f;
    acceleration[1] = 0.5f * 9.80665f;
    acceleration[2] = 0.8660254038f * 9.80665f;
    for (int i = 0; i < 250; ++i) {
        assert(motion_update(&filter, acceleration, gyro, 0.008f, identity, &out));
    }
    assert(fabsf(out.gravity[1] - acceleration[1]) < 0.001f);
    assert(fabsf(out.force[1] - acceleration[1] * TILT_RESPONSE_GAIN) < 0.001f);

    /* Reset to face-up before checking screen-plane device acceleration. */
    memset(&filter, 0, sizeof(filter));
    acceleration[0] = acceleration[1] = 0.0f;
    acceleration[2] = 9.80665f;
    for (int i = 0; i < 250; ++i) {
        assert(motion_update(&filter, acceleration, gyro, 0.008f, identity, &out));
    }

    acceleration[0] = 8.0f;
    for (int i = 0; i < 8; ++i) {
        assert(motion_update(&filter, acceleration, gyro, 0.008f, identity, &out));
    }
    assert(out.linear_device_acceleration[0] < -5.0f);
    assert(out.fluid_acceleration[0] > 5.0f && out.force[0] > 6.0f);

    acceleration[0] = 200.0f;
    for (int i = 0; i < 20; ++i) {
        assert(motion_update(&filter, acceleration, gyro, 0.008f, identity, &out));
    }
    assert(fabsf(out.linear_device_acceleration[0]) <= MAX_LINEAR_ACCELERATION + 0.001f);
    acceleration[0] = NAN;
    assert(!motion_update(&filter, acceleration, gyro, 0.008f, identity, &out));

    /* A large translation should remain close to the proven 1.20 response. */
    memset(&filter, 0, sizeof(filter));
    acceleration[0] = acceleration[1] = 0.0f;
    acceleration[2] = 9.80665f;
    for (int i = 0; i < 250; ++i) {
        assert(motion_update(&filter, acceleration, gyro, 0.008f, identity, &out));
    }
    acceleration[0] = 9.80665f;
    for (int i = 0; i < 8; ++i) {
        assert(motion_update(&filter, acceleration, gyro, 0.008f, identity, &out));
    }
    float previous_force =
        out.gravity[0] * TILT_RESPONSE_GAIN + out.fluid_acceleration[0] * PUSH_GAIN;
    assert(out.force[0] > previous_force);
    assert(out.force[0] < previous_force * 1.10f);
    assert(out.translation_target[0] > 0.05f);
    assert(out.translation_target[0] <= TRANSLATION_VELOCITY_MAX + STRONG_TRANSLATION_VELOCITY_MAX);
    puts("PASS: gravity, gyro tracking, quiet rest, face-up gravity, tilt response, small/large "
         "linear acceleration, clamp, invalid sample");
}
