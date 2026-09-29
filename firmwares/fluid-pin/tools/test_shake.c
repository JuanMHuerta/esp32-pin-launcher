#include "fluid.h"
#include "motion.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static const float identity[3][3] = {{1,0,0}, {0,1,0}, {0,0,1}};
static const float g = 9.80665f;

static float centroid_x(const fluid_snapshot *snapshot)
{
    float weighted = 0.0f;
    for (int y = 0; y < PIXEL_GRID_Y; ++y)
        for (int x = 0; x < PIXEL_GRID_X; ++x)
            weighted += snapshot->density[y * PIXEL_GRID_X + x] * x;
    return weighted / PARTICLE_COUNT;
}

static float right_edge_density(const fluid_snapshot *snapshot)
{
    float density = 0.0f;
    for (int y = 0; y < PIXEL_GRID_Y; ++y)
        for (int x = PIXEL_GRID_X - 6; x < PIXEL_GRID_X; ++x)
            density += snapshot->density[y * PIXEL_GRID_X + x];
    return density / PARTICLE_COUNT;
}

static void hard_shake(float amplitude_g)
{
    motion_filter filter = {0};
    motion_output output = {0};
    fluid_t *fluid = fluid_create();
    fluid_t *force_only = fluid_create();
    assert(fluid && force_only);
    const float dt = 1.0f / IMU_HZ;
    float next_physics = 0.0f;
    float peak_target = 0.0f, peak_force = 0.0f, peak_speed = 0.0f;
    float peak_visual_difference = 0.0f, peak_centroid_difference = 0.0f;
    int active_targets = 0, strong_samples = 0, strong_targets = 0;
    fluid_snapshot snapshot = {0};
    fluid_snapshot comparison = {0};
    for (int sample = 0; sample < 16 * IMU_HZ; ++sample) {
        float t = sample * dt;
        bool shaking = t >= 3.0f && t < 6.0f;
        float acceleration[3] = {shaking ? amplitude_g * g *
            sinf(2.0f * 3.14159265f * 2.8f * (t - 3.0f)) : 0.0f, g, 0.0f};
        float gyro[3] = {0.0f};
        assert(motion_update(&filter, acceleration, gyro, dt, identity, &output));
        if (shaking) {
            float target = fabsf(output.translation_target[0] * output.translation_scale[0]);
            if (target > peak_target) peak_target = target;
            if (target > 0.5f) ++active_targets;
            if (fabsf(acceleration[0]) > 0.8f * g) {
                ++strong_samples;
                if (target > 0.5f) ++strong_targets;
            }
            if (fabsf(output.force[0]) > peak_force) peak_force = fabsf(output.force[0]);
        }
        if (t + 0.0001f >= next_physics) {
            assert(fluid_step_with_translation(fluid,
                output.force[0] * GRAVITY / g, output.force[1] * GRAVITY / g,
                output.translation_target[0] * output.translation_scale[0],
                output.translation_target[1] * output.translation_scale[1]));
            assert(fluid_step(force_only, output.force[0] * GRAVITY / g,
                               output.force[1] * GRAVITY / g));
            fluid_metrics metrics;
            fluid_get_metrics(fluid, &metrics);
            if (shaking && metrics.average_speed > peak_speed) peak_speed = metrics.average_speed;
            if (shaking) {
                fluid_publish(fluid, &snapshot);
                fluid_publish(force_only, &comparison);
                float difference = 0.0f;
                for (int pixel = 0; pixel < PIXEL_COUNT; ++pixel)
                    difference += fabsf(snapshot.density[pixel] - comparison.density[pixel]);
                difference /= 2.0f * PARTICLE_COUNT;
                if (difference > peak_visual_difference) peak_visual_difference = difference;
                float shift = fabsf(centroid_x(&snapshot) - centroid_x(&comparison));
                if (shift > peak_centroid_difference) peak_centroid_difference = shift;
            }
            next_physics += 1.0f / PHYSICS_HZ;
        }
    }
    fluid_publish(fluid, &snapshot);
    printf("hard shake %.1fg target_peak=%.3f active_samples=%d strong_targets=%d/%d force_peak=%.3f speed_peak=%.3f extra_image=%.3f extra_shift=%.3f rest_speed=%.3f resets=%u\n",
           amplitude_g, peak_target, active_targets, strong_targets, strong_samples, peak_force, peak_speed,
           peak_visual_difference, peak_centroid_difference,
           snapshot.metrics.average_speed, snapshot.metrics.resets);
    assert(strong_targets > strong_samples / 2);
    assert(snapshot.metrics.resets == 0);
    assert(snapshot.metrics.average_speed < 0.35f);
    assert(peak_centroid_difference > 1.0f);
    fluid_destroy(fluid);
    fluid_destroy(force_only);
}

static void wall_reversal(void)
{
    fluid_t *fluid = fluid_create();
    assert(fluid);
    fluid_snapshot snapshot = {0};
    float right = 0.0f, left = 0.0f;
    float release_100ms = 0.0f, release_200ms = 0.0f, release_500ms = 0.0f;
    /* Begin the reversal at rest.  A pool still travelling into the wall
     * must first lose its incoming momentum, which is not wall adhesion. */
    for (int step = 0; step < 8 * PHYSICS_HZ; ++step)
        assert(fluid_step(fluid, GRAVITY, 0.0f));
    for (int step = 0; step < 7 * PHYSICS_HZ; ++step) {
        float acceleration_x = step < 2 * PHYSICS_HZ ? GRAVITY :
            (step < 4 * PHYSICS_HZ ? -GRAVITY : 0.0f);
        float acceleration_y = step < 4 * PHYSICS_HZ ? 0.0f : GRAVITY;
        assert(fluid_step(fluid, acceleration_x, acceleration_y));
        if (step == 2 * PHYSICS_HZ - 1) {
            fluid_publish(fluid, &snapshot);
            right = centroid_x(&snapshot);
        }
        if (step == 2 * PHYSICS_HZ + 5 || step == 2 * PHYSICS_HZ + 11 ||
            step == 2 * PHYSICS_HZ + 29) {
            fluid_publish(fluid, &snapshot);
            float release = right - centroid_x(&snapshot);
            if (step == 2 * PHYSICS_HZ + 5) release_100ms = release;
            if (step == 2 * PHYSICS_HZ + 11) release_200ms = release;
            if (step == 2 * PHYSICS_HZ + 29) release_500ms = release;
        }
        if (step == 4 * PHYSICS_HZ - 1) {
            fluid_publish(fluid, &snapshot);
            left = centroid_x(&snapshot);
        }
    }
    fluid_publish(fluid, &snapshot);
    printf("wall reversal centroid right=%.3f left=%.3f release_100ms=%.3f release_200ms=%.3f release_500ms=%.3f change=%.3f rest_speed=%.3f resets=%u\n",
           right, left, release_100ms, release_200ms, release_500ms,
           right - left, snapshot.metrics.average_speed, snapshot.metrics.resets);
    assert(right - left > 12.0f);
    /* These bounds follow acceleration from rest.  The former centroid
     * servo imposed an instantaneous speed unrelated to the body force. */
    assert(release_100ms > 0.45f && release_200ms > 2.0f && release_500ms > 12.0f);
    assert(snapshot.metrics.resets == 0);
    fluid_destroy(fluid);
}

static void wall_release(void)
{
    fluid_t *force_only = fluid_create();
    fluid_t *moving = fluid_create();
    assert(force_only && moving);
    fluid_snapshot force_image = {0}, moving_image = {0};
    for (int step = 0; step < 3 * PHYSICS_HZ; ++step) {
        assert(fluid_step(force_only, GRAVITY, 0.0f));
        assert(fluid_step(moving, GRAVITY, 0.0f));
    }
    fluid_publish(force_only, &force_image);
    float wall_centroid = centroid_x(&force_image);
    float peak_extra_release = 0.0f, peak_departure = 0.0f;
    float edge_start = right_edge_density(&force_image), edge_at_pulse = 0.0f;
    float release_at_pulse = 0.0f, release_at_300ms = 0.0f;
    for (int step = 0; step < 2 * PHYSICS_HZ; ++step) {
        bool shifting = step < 9;
        float force = shifting ? -1.4f * GRAVITY : GRAVITY;
        float target = shifting ? -(TRANSLATION_VELOCITY_MAX +
            STRONG_TRANSLATION_VELOCITY_MAX - TRANSLATION_VELOCITY_DEADZONE) *
            TRANSLATION_VELOCITY_SCALE_UPRIGHT : 0.0f;
        assert(fluid_step(force_only, force, 0.0f));
        assert(fluid_step_with_translation(moving, force, 0.0f, target, 0.0f));
        fluid_publish(force_only, &force_image);
        fluid_publish(moving, &moving_image);
        float extra = centroid_x(&force_image) - centroid_x(&moving_image);
        float departure = wall_centroid - centroid_x(&moving_image);
        if (extra > peak_extra_release) peak_extra_release = extra;
        if (departure > peak_departure) peak_departure = departure;
        if (step == 8) {
            edge_at_pulse = right_edge_density(&moving_image);
            release_at_pulse = departure;
        }
        if (step == 17) release_at_300ms = departure;
    }
    printf("wall release starting_x=%.3f extra_release=%.3f release_150ms=%.3f release_300ms=%.3f departure=%.3f edge=%.3f->%.3f resets=%u\n",
           wall_centroid, peak_extra_release, release_at_pulse, release_at_300ms,
           peak_departure, edge_start, edge_at_pulse,
           moving_image.metrics.resets);
    assert(moving_image.metrics.resets == 0);
    assert(peak_extra_release > 1.0f);
    assert(peak_departure > 2.0f);
    fluid_destroy(force_only);
    fluid_destroy(moving);
}

static void rotating_wall_release(void)
{
    motion_filter filter = {0};
    motion_output output = {0};
    fluid_t *moving = fluid_create();
    fluid_t *force_only = fluid_create();
    assert(moving && force_only);
    fluid_snapshot moving_image = {0}, force_image = {0};
    const float dt = 1.0f / IMU_HZ;
    const float turn_seconds = 0.40f;
    const float turn_rate = -3.14159265f / turn_seconds;
    float next_physics = 0.0f, start_x = 0.0f;
    float release_100ms = 0.0f, release_200ms = 0.0f, release_400ms = 0.0f;
    float extra_100ms = 0.0f, extra_200ms = 0.0f, extra_400ms = 0.0f;
    float target_peak = 0.0f;
    for (int sample = 0; sample < 5 * IMU_HZ; ++sample) {
        float t = sample * dt;
        if (sample == 3 * IMU_HZ) {
            fluid_publish(moving, &moving_image);
            start_x = centroid_x(&moving_image);
        }
        float turn = fminf(fmaxf(t - 3.0f, 0.0f), turn_seconds);
        float angle = 0.5f * 3.14159265f + turn_rate * turn;
        float accel[3] = {g * sinf(angle), g * cosf(angle), 0.0f};
        float gyro[3] = {0.0f, 0.0f, t >= 3.0f && t < 3.0f + turn_seconds ? turn_rate : 0.0f};
        assert(motion_update(&filter, accel, gyro, dt, identity, &output));
        if (t >= 3.0f && t < 3.0f + turn_seconds) {
            float target = fabsf(output.translation_target[0] * output.translation_scale[0]);
            if (target > target_peak) target_peak = target;
        }
        if (t + 0.0001f < next_physics) continue;
        assert(fluid_step_with_translation(moving,
            output.force[0] * GRAVITY / g, output.force[1] * GRAVITY / g,
            output.translation_target[0] * output.translation_scale[0],
            output.translation_target[1] * output.translation_scale[1]));
        assert(fluid_step(force_only, output.force[0] * GRAVITY / g,
                          output.force[1] * GRAVITY / g));
        next_physics += 1.0f / PHYSICS_HZ;
        if (fabsf(t - 3.1f) < dt || fabsf(t - 3.2f) < dt || fabsf(t - 3.4f) < dt) {
            fluid_publish(moving, &moving_image);
            fluid_publish(force_only, &force_image);
            float departure = start_x - centroid_x(&moving_image);
            float extra = centroid_x(&force_image) - centroid_x(&moving_image);
            if (fabsf(t - 3.1f) < dt) { release_100ms = departure; extra_100ms = extra; }
            if (fabsf(t - 3.2f) < dt) { release_200ms = departure; extra_200ms = extra; }
            if (fabsf(t - 3.4f) < dt) { release_400ms = departure; extra_400ms = extra; }
        }
    }
    printf("rotating wall target_peak=%.3f release_100/200/400ms=%.3f/%.3f/%.3f extra=%.3f/%.3f/%.3f resets=%u\n",
           target_peak, release_100ms, release_200ms, release_400ms,
           extra_100ms, extra_200ms, extra_400ms, moving_image.metrics.resets);
    assert(moving_image.metrics.resets == 0);
    assert(target_peak > 5.0f && target_peak <= TILT_SWEEP_MAX_TARGET + 0.1f);
    assert(extra_200ms > 1.5f && extra_400ms > 4.0f);
    fluid_destroy(moving);
    fluid_destroy(force_only);
}

static void settled_wall_release(void)
{
    fluid_t *fluid = fluid_create();
    assert(fluid);
    fluid_snapshot snapshot = {0};
    for (int step = 0; step < 10 * PHYSICS_HZ; ++step)
        assert(fluid_step(fluid, GRAVITY, 0.0f));
    fluid_publish(fluid, &snapshot);
    float start_edge = right_edge_density(&snapshot);
    float edge_50ms = 0.0f, edge_200ms = 0.0f;
    for (int step = 0; step < 12; ++step) {
        assert(fluid_step(fluid, -GRAVITY, 0.0f));
        if (step == 2 || step == 11) {
            fluid_publish(fluid, &snapshot);
            if (step == 2) edge_50ms = right_edge_density(&snapshot);
            else edge_200ms = right_edge_density(&snapshot);
        }
    }
    printf("settled wall edge start=%.3f after_50ms=%.3f after_200ms=%.3f resets=%u\n",
           start_edge, edge_50ms, edge_200ms, snapshot.metrics.resets);
    /* At 50 ms physical acceleration has moved less than a tenth of a cell.
     * Require departure, without the old servo's instantaneous bulk kick. */
    assert(start_edge - edge_50ms > 0.002f);
    assert(start_edge - edge_200ms > 0.05f);
    assert(snapshot.metrics.resets == 0);
    fluid_destroy(fluid);
}

int main(void)
{
    hard_shake(1.2f);
    hard_shake(2.0f);
    wall_reversal();
    wall_release();
    rotating_wall_release();
    settled_wall_release();
}
