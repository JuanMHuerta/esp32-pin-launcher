// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "render.h"
#include "motion.h"
typedef struct {
    motion_output motion;
    float linear_peak[2];
    float translation_peak[2];
    int16_t accel[3], gyro[3];
    float gyro_bias[3];
    float rest_gravity_magnitude;
    float stationary_seconds;
    uint32_t samples, errors;
    int64_t sample_us;
    bool imu_ok;
    bool gyro_calibrated;
} board_status_t;
typedef struct {
    uint32_t rgb_us, transfer_us, frame_us;
} board_display_metrics;
void board_init(void);
void board_present(const fluid_renderer *r, board_display_metrics *metrics);
void board_status(board_status_t *out);
void board_brightness(uint8_t value);
