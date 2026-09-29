#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "motion.h"

int main(void)
{
    const float rest[3] = {0, 0, 1}, zero[3] = {0, 0, 0};
    motion_filter_t m = {0};
    assert(!motion_update(&m, rest, zero, 0.04f));
    for (int axis = 0; axis < 3; ++axis) {
        m = (motion_filter_t){0};
        motion_update(&m, rest, zero, 0.04f);
        float gyro[3] = {0}; gyro[axis] = 40;
        for (int i = 0; i < 25; ++i) motion_update(&m, rest, gyro, 0.04f);
        if (axis == 0) assert(m.view_x < -0.1f && fabsf(m.view_y) < 0.001f);
        if (axis == 1) assert(m.view_y < -0.1f && fabsf(m.view_x) < 0.001f);
        if (axis == 2) assert(m.roll > 100);
        assert(m.energy > 0.2f);
        for (int i = 0; i < 25; ++i) motion_update(&m, rest, zero, 0.04f);
        if (axis == 0) assert(m.view_x < -0.3f);
        if (axis == 1) assert(m.view_y < -0.3f);
        for (int i = 0; i < 300; ++i) motion_update(&m, rest, zero, 0.04f);
        assert(fabsf(m.view_x) < 0.003f && fabsf(m.view_y) < 0.003f);
    }
    m = (motion_filter_t){0};
    motion_update(&m, rest, zero, 0.04f);
    const float tilt[3] = {0.6f, -0.5f, 0.62f};
    for (int i = 0; i < 150; ++i) motion_update(&m, tilt, zero, 0.04f);
    assert(m.view_x < -0.49f && m.view_y < -0.59f);
    const float tiny[3] = {0.3f, -0.2f, 0.5f};
    for (int i = 0; i < 1000; ++i) motion_update(&m, tilt, tiny, 0.04f);
    assert(fabsf(m.roll) < 0.001f);
    puts("motion checks passed: accelerometer and all three gyro axes");
}
