#pragma once

/*
 * Fluid pin tuning.  Keep all behavioural constants here: the solver and
 * renderer deliberately contain no alternative profiles or hidden tuning.
 */
#define FILL_RATIO                 0.40f
/*
 * PUSH_GAIN is the response for large motions; the extra low-motion terms
 * below fade out smoothly.
 */
#define PUSH_GAIN                  1.20f
#define TILT_RESPONSE_GAIN         1.12f
#define TILT_HORIZONTAL_GAIN       1.55f
#define WALK_PUSH_EXTRA_GAIN       1.20f
#define WALK_PUSH_KNEE             (0.20f * 9.80665f)
#define WALK_TILT_EXTRA_GAIN       0.40f
#define WALK_TILT_KNEE             (0.25f * 9.80665f)
#define WALK_TILT_REFERENCE_TAU    0.65f
/* A short leaky integration estimates enclosure velocity during a slide.
 * Post-projection PIC/FLIP velocity is nudged toward this bounded target. */
#define TRANSLATION_VELOCITY_TAU   0.35f
#define TRANSLATION_VELOCITY_DEADZONE 0.01f
#define TRANSLATION_VELOCITY_MAX   0.25f /* physical m/s */
#define STRONG_TRANSLATION_VELOCITY_TAU 0.12f
#define STRONG_TRANSLATION_VELOCITY_MAX 0.35f /* additional physical m/s */
#define STRONG_TRANSLATION_ACCELERATION_CAP (0.60f * 9.80665f)
#define TRANSLATION_VELOCITY_SCALE_UPRIGHT 18.0f /* cells / physical m */
#define TRANSLATION_VELOCITY_SCALE_FACE_UP 35.0f /* no upright surface wave when flat */
#define UPRIGHT_VERTICAL_TRANSLATION_GAIN 2.5f
#define TRANSLATION_ACTIVITY_LIMIT (0.80f * 9.80665f)
#define PITCH_GATE_START           0.08f /* rad/s */
#define PITCH_GATE_FULL            0.35f
#define PITCH_GATE_DECAY_TAU       0.12f
#define PITCH_VERTICAL_BOOST_REDUCTION 0.65f
#define VERTICAL_SLIDE_START       0.35f /* change in specific-force magnitude, m/s^2 */
#define VERTICAL_SLIDE_FULL        0.90f
#define TRANSLATION_TARGET_DEADZONE 0.10f /* simulation cells/s */
#define TRANSLATION_TARGET_BLEND_RANGE 0.40f
#define TRANSLATION_CORRECTION_LIMIT 0.80f /* cells/s per physics step */
#define STRONG_TRANSLATION_CORRECTION_LIMIT 1.60f
#define STRONG_TRANSLATION_TARGET_START 3.50f /* cells/s */
#define STRONG_TRANSLATION_TARGET_FULL 4.20f
#define VERY_STRONG_TRANSLATION_CORRECTION_LIMIT 3.00f
#define VERY_STRONG_TRANSLATION_TARGET_FULL 10.0f
/* A rotating tank carries water along its changing downhill direction. */
#define TILT_SWEEP_FILTER_TAU       0.04f
#define TILT_SWEEP_CELLS_PER_RAD   2.8f
#define TILT_SWEEP_MAX_TARGET      8.0f /* cells/s */
#define TILT_SWEEP_RATE_START      0.05f /* rad/s */
#define TILT_SWEEP_RATE_FULL       0.25f
/* Vertical tank sway briefly spreads/compresses the free surface. */
#define VERTICAL_SURFACE_RESPONSE  0.05f
#define VERTICAL_SURFACE_DEPTH     2.50f /* simulation cells */
#define FLIP_RATIO                 0.72f
#define SPEED_MULTIPLIER           1.60f
#define GLOW                       1.10f

#define GRID_X                     38
#define GRID_Y                     18
#define GRID_CELLS                 (GRID_X * GRID_Y)
#define CELL_SIZE                  1.0f

#define PARTICLE_RADIUS            0.34f
#define BASE_PARTICLES_AT_60       860
#define PARTICLE_COUNT             573 /* floor(860 * 0.40 / 0.60) */

/* With simulation time scaled by 1.6, the unconstrained fall across the
 * 15-cell short axis takes about 0.63 real seconds. */
#define GRAVITY                    30.0f
#define BASE_DT                     (1.0f / 60.0f)
#define SIM_DT                      (BASE_DT * SPEED_MULTIPLIER)
#define PHYSICS_HZ                 60
#define RENDER_HZ                  60
#define IMU_HZ                     125

#define PARTICLE_SEPARATION_ITERS  2
#define HASH_CELL_SIZE             (2.2f * PARTICLE_RADIUS)
/* ceil(38 / 0.748) + 1 and ceil(18 / 0.748) + 1. */
#define HASH_X                     52
#define HASH_Y                     26
#define HASH_BUCKETS               (HASH_X * HASH_Y)

#define PRESSURE_ITERS             22
#define PRESSURE_OVER_RELAXATION   1.85f
#define DENSITY_DRIFT_K            1.0f
#define VELOCITY_DAMPING           0.990f

/* One-cell solid container border; particle centres are in cell units. */
#define MIN_X                      (1.0f + PARTICLE_RADIUS)
#define MAX_X                      ((float)GRID_X - 1.0f - PARTICLE_RADIUS)
#define MIN_Y                      (1.0f + PARTICLE_RADIUS)
#define MAX_Y                      ((float)GRID_Y - 1.0f - PARTICLE_RADIUS)

#define PIXEL_GRID_X               67
#define PIXEL_GRID_Y               30
#define PIXEL_COUNT                (PIXEL_GRID_X * PIXEL_GRID_Y)
#define PIXEL_NATIVE_SIZE          8
#define PANEL_W                    (PIXEL_GRID_X * PIXEL_NATIVE_SIZE)
#define PANEL_H                    (PIXEL_GRID_Y * PIXEL_NATIVE_SIZE)
/* Six logical rows per transfer amortize panel-window commands while the
 * two DMA buffers still fit in internal SRAM. */
#define FLOW_BAND_ROWS             (6 * PIXEL_NATIVE_SIZE)

/* Shorter visual trail makes a new walking impulse visible on the next frame. */
#define RENDER_PERSISTENCE         0.45f
#define RENDER_DENSITY_THRESHOLD   0.035f
/* Restore the boundary sample only while the tank is being pushed into that
 * wall.  This is deliberately below a small hand tilt, but above IMU noise. */
#define RENDER_WALL_CONTACT_ACCEL  1.50f /* simulation cells / s^2 */
#define RENDER_SPEED_LEVELS        16
#define RENDER_LAYERS              3

#define MAX_LINEAR_ACCELERATION    (2.0f * 9.80665f)
#define GRAVITY_FILTER_TAU         0.35f
/* Track gait acceleration quickly while rejecting single-sample IMU noise. */
#define LINEAR_FILTER_TAU          0.015f
/* The uncalibrated QMI8658 can read about 13 degrees/s at rest.  Once its
 * bias is known, a much tighter threshold keeps slow hand tilts out of the
 * bias estimate. */
#define GYRO_INITIAL_STATIONARY_THRESHOLD 0.35f
#define GYRO_STATIONARY_THRESHOLD  0.08f
#define STATIONARY_POSE_TOLERANCE  0.08f /* m/s^2 change over calibration window */

/*
 * QMI8658 board coordinates to landscape screen gravity coordinates.
 * This is intentionally one explicit block so diagnostic builds can validate
 * the four required poses without a sign being hidden in motion code.
 *
 * QMI +X maps to the visible screen-down direction after the panel's QSPI
 * orientation transform; QMI +Y maps to visible screen-right.  The signs were
 * verified on the physical board after the initial gravity inversion report.
 * Z only preserves the 3D filter basis.
 */
#define IMU_TO_SCREEN_GRAVITY {{0.0f,  1.0f, 0.0f}, \
                               { 1.0f, 0.0f, 0.0f}, \
                               {0.0f, 0.0f, 1.0f}}

/* Development options are compile-time only; production defaults are quiet. */
#ifndef FLUID_DIAGNOSTICS
#define FLUID_DIAGNOSTICS          0
#endif
#ifndef FLUID_IMU_DIAGNOSTIC
#define FLUID_IMU_DIAGNOSTIC       0
#endif
#ifndef FLUID_SYNTHETIC_TEST
#define FLUID_SYNTHETIC_TEST       0
#endif
#ifndef FLUID_RENDER_SELF_TEST
#define FLUID_RENDER_SELF_TEST     0
#endif
