#include "fluid.h"
#include "motion.h"
#include "render.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static const float identity[3][3] = {{1,0,0}, {0,1,0}, {0,0,1}};
static const float g = 9.80665f;

/* The previous input curve, evaluated on the same filtered IMU samples. */
static void previous_force(const motion_output *motion, float force[2])
{
    float x = motion->gravity[0] * TILT_RESPONSE_GAIN;
    float y = motion->gravity[1] * TILT_RESPONSE_GAIN;
    float length = hypotf(x, y);
    if (length > g) { x *= g / length; y *= g / length; }
    force[0] = x + motion->fluid_acceleration[0] * PUSH_GAIN;
    force[1] = y + motion->fluid_acceleration[1] * PUSH_GAIN;
}

int main(void)
{
    motion_filter filter = {0};
    motion_output motion = {0};
    fluid_t *previous = fluid_create();
    fluid_t *walking = fluid_create();
    fluid_t *still = fluid_create();
    fluid_renderer *walking_renderer = render_create();
    fluid_renderer *still_renderer = render_create();
    assert(previous && walking && still && walking_renderer && still_renderer);
    fluid_snapshot still_image = {0}, moving_image = {0};
    float occupancy_sum = 0, peak_occupancy = 0;
    int visual_frames = 0;
    static uint16_t still_row[PANEL_W], moving_row[PANEL_W];

    const float imu_dt = 1.0f / 125.0f;
    const float physics_dt = 1.0f / 60.0f;
    float next_physics = 0.0f;
    double old_x = 0.0, new_x = 0.0;
    double old_y = 0.0, new_y = 0.0;
    double old_speed = 0.0, new_speed = 0.0;
    int gait_steps = 0;
    fluid_metrics old_metrics = {0}, new_metrics = {0};

    /* Two quiet seconds calibrate the gyro, then 12 seconds of a small
       1.7 Hz walking sway with 3.4-degree tilt and 0.08 g translation on
       each screen axis.  End with eight quiet seconds. */
    for (int sample = 0; sample < 22 * 125; ++sample) {
        float t = sample * imu_dt;
        float angle = 0.0f, angular_velocity = 0.0f;
        float linear_x = 0.0f, linear_y = 0.0f;
        bool gait = t >= 2.0f && t < 14.0f;
        if (gait) {
            float phase = 2.0f * 3.14159265f * 1.7f * (t - 2.0f);
            angle = 0.06f * sinf(phase);
            angular_velocity = 0.06f * 2.0f * 3.14159265f * 1.7f * cosf(phase);
            linear_x = 0.08f * g * sinf(phase + 0.5f);
            linear_y = 0.08f * g * sinf(phase + 1.2f);
        }
        float accel[3] = {g * sinf(angle) + linear_x,
                          g * cosf(angle) + linear_y, 0.0f};
        float gyro[3] = {0.0f, 0.0f, angular_velocity};
        assert(motion_update(&filter, accel, gyro, imu_dt, identity, &motion));

        if (t + 0.0001f >= next_physics) {
            float old_force[2];
            previous_force(&motion, old_force);
            assert(fluid_step(previous, old_force[0] * GRAVITY / g, old_force[1] * GRAVITY / g));
            assert(fluid_step_with_translation(walking, motion.force[0] * GRAVITY / g,
                   motion.force[1] * GRAVITY / g,
                   motion.translation_target[0] * motion.translation_scale[0],
                   motion.translation_target[1] * motion.translation_scale[1]));
            fluid_get_metrics(previous, &old_metrics);
            fluid_get_metrics(walking, &new_metrics);
            assert(fluid_step(still, 0.0f, GRAVITY));
            fluid_publish(still, &still_image);
            fluid_publish(walking, &moving_image);
            render_reconstruct(still_renderer, &still_image);
            render_reconstruct(walking_renderer, &moving_image);
            if (gait) {
                old_x += fabsf(old_force[0]);
                new_x += fabsf(motion.force[0]);
                old_y += fabsf(old_force[1] - g);
                new_y += fabsf(motion.force[1] - g);
                old_speed += old_metrics.average_speed;
                new_speed += new_metrics.average_speed;
                ++gait_steps;
                /* Measure changed occupied pixels in the actual RGB565
                 * output.  Brightness alone cannot pass the visibility gate. */
                int changed = 0, lit = 0;
                for (int row = 0; row < PIXEL_GRID_Y; ++row) {
                    render_band(still_renderer, row * PIXEL_NATIVE_SIZE + 3, 1, still_row);
                    render_band(walking_renderer, row * PIXEL_NATIVE_SIZE + 3, 1, moving_row);
                    for (int col = 0; col < PIXEL_GRID_X; ++col) {
                        bool a = still_row[col * PIXEL_NATIVE_SIZE + 3] != 0;
                        bool b = moving_row[col * PIXEL_NATIVE_SIZE + 3] != 0;
                        changed += a != b;
                        lit += a || b;
                    }
                }
                float occupancy = lit ? (float)changed / lit : 0;
                occupancy_sum += occupancy;
                if (occupancy > peak_occupancy) peak_occupancy = occupancy;
                ++visual_frames;
            }
            next_physics += physics_dt;
        }
    }

    fluid_snapshot old_snapshot = {0}, new_snapshot = {0};
    fluid_publish(previous, &old_snapshot);
    fluid_publish(walking, &new_snapshot);
    printf("walking x_force=%.3f->%.3f y_force=%.3f->%.3f avg_speed=%.3f->%.3f\n",
           old_x / gait_steps, new_x / gait_steps, old_y / gait_steps, new_y / gait_steps,
           old_speed / gait_steps, new_speed / gait_steps);
    printf("after_rest old_speed=%.3f new_speed=%.3f mass=%.6f ceiling=%d resets=%u\n",
           old_metrics.average_speed, new_metrics.average_speed,
           new_snapshot.metrics.render_mass_ratio, new_metrics.ceiling_particles, new_metrics.resets);
    assert(new_x > old_x * 1.25);
    assert(new_y > old_y * 1.15);
    assert(new_speed > old_speed * 1.05);
    printf("walk visible occupancy mean=%.3f peak=%.3f\n",
           occupancy_sum / visual_frames, peak_occupancy);
    assert(occupancy_sum / visual_frames > 0.05f && peak_occupancy > 0.10f);
    assert(new_metrics.average_speed < 0.35f);
    assert(new_metrics.ceiling_particles == 0 && new_metrics.resets == 0);
    assert(new_metrics.particle_count == PARTICLE_COUNT);
    assert(new_snapshot.metrics.render_mass_ratio >= 0.995f &&
           new_snapshot.metrics.render_mass_ratio <= 1.005f);
    fluid_destroy(previous);
    fluid_destroy(walking);
    fluid_destroy(still);
    render_destroy(walking_renderer);
    render_destroy(still_renderer);
    puts("PASS: walking response, rest recovery, particle and visual mass");
}
