#include "fluid.h"
#include "motion.h"
#include "render.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const float identity[3][3] = {{1,0,0}, {0,1,0}, {0,0,1}};
static const float physical_g = 9.80665f;

static float image_difference(const fluid_snapshot *a, const fluid_snapshot *b)
{
    float sum = 0.0f;
    for (int i = 0; i < PIXEL_COUNT; ++i)
        sum += fabsf(a->density[i] - b->density[i]);
    return sum / (2.0f * PARTICLE_COUNT);
}

static float centroid(const fluid_snapshot *snapshot, int axis)
{
    float weighted = 0.0f;
    for (int y = 0; y < PIXEL_GRID_Y; ++y)
        for (int x = 0; x < PIXEL_GRID_X; ++x)
            weighted += snapshot->density[y * PIXEL_GRID_X + x] * (axis == 0 ? x : y);
    return weighted / PARTICLE_COUNT;
}

static float display_difference(fluid_renderer *still, fluid_renderer *moving,
                                float *occupancy_difference, int axis, float *centroid_shift)
{
    static uint16_t bands[2][PANEL_W * FLOW_BAND_ROWS];
    int different = 0;
    int changed_occupancy = 0;
    int lit = 0;
    double weighted[2] = {0};
    int occupied_count[2] = {0};
    for (int y = 0; y < PANEL_H; y += FLOW_BAND_ROWS) {
        render_band(still, y, FLOW_BAND_ROWS, bands[0]);
        render_band(moving, y, FLOW_BAND_ROWS, bands[1]);
        for (int i = 0; i < PANEL_W * FLOW_BAND_ROWS; ++i) {
            for (int k = 0; k < 2; ++k)
                if (bands[k][i]) {
                    weighted[k] += axis ? y + i / PANEL_W : i % PANEL_W;
                    ++occupied_count[k];
                }
            if (bands[0][i] || bands[1][i]) {
                ++lit;
                if (bands[0][i] != bands[1][i]) ++different;
                if ((bands[0][i] == 0) != (bands[1][i] == 0)) ++changed_occupancy;
            }
        }
    }
    *occupancy_difference = lit ? (float)changed_occupancy / lit : 0.0f;
    *centroid_shift = occupied_count[0] && occupied_count[1]
        ? fabs(weighted[1] / occupied_count[1] - weighted[0] / occupied_count[0]) / PIXEL_NATIVE_SIZE
        : 0.0f;
    return lit ? (float)different / lit : 0.0f;
}

static float run_axis(int axis, bool face_up, bool pulse, bool short_slide)
{
    fluid_t *still = fluid_create();
    fluid_t *moving = fluid_create();
    fluid_renderer *still_renderer = render_create();
    fluid_renderer *moving_renderer = render_create();
    assert(still && moving && still_renderer && moving_renderer);
    motion_filter filter = {0};
    motion_filter still_filter = {0};
    motion_output motion = {0};
    motion_output still_motion = {0};
    fluid_snapshot still_image = {0}, moving_image = {0};
    const float imu_dt = 1.0f / IMU_HZ;
    const float physics_dt = 1.0f / PHYSICS_HZ;
    float next_physics = 0.0f;
    float peak_difference = 0.0f;
    float sum_difference = 0.0f;
    float max_speed = 0.0f;
    float max_force = 0.0f;
    float max_linear = 0.0f;
    float max_target = 0.0f;
    float peak_display_difference = 0.0f;
    float peak_occupancy_difference = 0.0f;
    float peak_centroid_shift = 0.0f;
    float peak_rendered_shift = 0.0f;
    float amplitude = short_slide && !face_up ? 0.25f : 0.15f;
    int moving_frames = 0;

    /* First settle with downward gravity, then hold the requested pose.
       A pure 0.15 g screen-plane translation starts at second six. */
    for (int sample = 0; sample < 14 * IMU_HZ; ++sample) {
        float t = sample * imu_dt;
        bool translating = t >= 6.0f && t < 11.0f;
        float still_a[3] = {0.0f, face_up && t >= 3.0f ? 0.0f : physical_g,
                           face_up && t >= 3.0f ? physical_g : 0.0f};
        float a[3] = {still_a[0], still_a[1], still_a[2]};
        if (translating) {
            float sign = face_up ? -1.0f : 1.0f;
            if (short_slide) {
                float phase = t - 6.0f;
                a[axis] += (phase < 0.12f ? sign :
                            (phase >= 0.35f && phase < 0.47f ? -sign : 0.0f)) * amplitude * physical_g;
            } else if (pulse)
                a[axis] += (t < 6.7f ? sign * amplitude * physical_g : 0.0f);
            else
                a[axis] += amplitude * physical_g * sinf(2.0f * 3.14159265f * 1.4f * (t - 6.0f));
        }
        float w[3] = {0.0f};
        assert(motion_update(&filter, a, w, imu_dt, identity, &motion));
        assert(motion_update(&still_filter, still_a, w, imu_dt, identity, &still_motion));
        if (translating) {
            float force = fabsf(motion.force[axis] - still_motion.force[axis]);
            float linear = fabsf(motion.fluid_acceleration[axis]);
            float target = fabsf(motion.translation_target[axis]) * motion.translation_scale[axis];
            if (force > max_force) max_force = force;
            if (linear > max_linear) max_linear = linear;
            if (target > max_target) max_target = target;
        }
        if (t + 0.0001f >= next_physics) {
            float still_x = still_motion.force[0] * GRAVITY / physical_g;
            float still_y = still_motion.force[1] * GRAVITY / physical_g;
            float ax = motion.force[0] * GRAVITY / physical_g;
            float ay = motion.force[1] * GRAVITY / physical_g;
            assert(fluid_step_with_translation(still, still_x, still_y,
                   still_motion.translation_target[0] * still_motion.translation_scale[0],
                   still_motion.translation_target[1] * still_motion.translation_scale[1]));
            assert(fluid_step_with_translation(moving, ax, ay,
                   motion.translation_target[0] * motion.translation_scale[0],
                   motion.translation_target[1] * motion.translation_scale[1]));
            if (translating) {
                fluid_publish(still, &still_image);
                fluid_publish(moving, &moving_image);
                render_reconstruct(still_renderer, &still_image);
                render_reconstruct(moving_renderer, &moving_image);
                float difference = image_difference(&still_image, &moving_image);
                float shift = fabsf(centroid(&still_image, axis) - centroid(&moving_image, axis));
                if (shift > peak_centroid_shift) peak_centroid_shift = shift;
                if (difference > peak_difference) peak_difference = difference;
                sum_difference += difference;
                ++moving_frames;
                assert(moving_image.metrics.render_mass_ratio >= 0.995f &&
                       moving_image.metrics.render_mass_ratio <= 1.005f);
                fluid_metrics m;
                fluid_get_metrics(moving, &m);
                if (m.average_speed > max_speed) max_speed = m.average_speed;
                if (moving_frames % 12 == 0) {
                    float occupancy, rendered_shift;
                    float display = display_difference(still_renderer, moving_renderer,
                                                       &occupancy, axis, &rendered_shift);
                    if (rendered_shift > peak_rendered_shift) peak_rendered_shift = rendered_shift;
                    if (display > peak_display_difference) peak_display_difference = display;
                    if (occupancy > peak_occupancy_difference) peak_occupancy_difference = occupancy;
                }
            }
            next_physics += physics_dt;
        }
    }
    fluid_publish(moving, &moving_image);
    fluid_metrics final = moving_image.metrics;
    printf("%s axis=%c mode=%s amp=%.2fg force_peak=%.3f linear_peak=%.3f target_peak=%.3f mean_image_difference=%.3f peak=%.3f display_peak=%.3f occupancy_peak=%.3f centroid_shift=%.3f max_speed=%.3f rest_speed=%.3f mass=%.6f\n",
           face_up ? "face-up" : "upright", axis == 0 ? 'x' : 'y',
           short_slide ? "slide" : (pulse ? "pulse" : "sine"), amplitude,
           max_force, max_linear, max_target, sum_difference / moving_frames, peak_difference,
           peak_display_difference, peak_occupancy_difference, peak_centroid_shift, max_speed,
           final.average_speed, final.render_mass_ratio);
    assert(final.particle_count == PARTICLE_COUNT && final.resets == 0);
    assert(final.render_mass_ratio >= 0.995f && final.render_mass_ratio <= 1.005f);
    printf("rendered_centroid_shift=%.3f logical pixels\n", peak_rendered_shift);
    if (short_slide) {
        /* A short hand slide must move the fluid by at least one logical
           pixel, not merely brighten already lit pixels. */
        assert(peak_centroid_shift > (face_up ? 2.0f : (axis == 1 ? 1.0f : 1.5f)));
        /* Measure the actual occupied shape moving by a logical pixel.
         * Counting changed pixels alone overestimated movement when holes
         * flickered inside the old, undersampled rendering. */
        assert(peak_rendered_shift > 1.0f);
    } else if (face_up) {
        assert(sum_difference / moving_frames > (pulse ? 0.07f : 0.04f));
    } else {
        assert(sum_difference / moving_frames > (pulse ? 0.08f : 0.06f));
    }
    assert(peak_display_difference > 0.01f);
    if (!short_slide) assert(peak_occupancy_difference > (pulse ? 0.08f : 0.02f));
    render_destroy(still_renderer);
    render_destroy(moving_renderer);
    fluid_destroy(still);
    fluid_destroy(moving);
    return max_speed;
}

static float rotation_reference(void)
{
    fluid_t *fluid = fluid_create();
    assert(fluid);
    for (int step = 0; step < 6 * PHYSICS_HZ; ++step)
        assert(fluid_step(fluid, 0.0f, GRAVITY));
    float maximum_speed = 0.0f;
    for (int step = 0; step < 4 * PHYSICS_HZ; ++step) {
        assert(fluid_step(fluid, GRAVITY, 0.0f));
        fluid_metrics metrics;
        fluid_get_metrics(fluid, &metrics);
        if (metrics.average_speed > maximum_speed) maximum_speed = metrics.average_speed;
    }
    printf("90-degree rotation max_speed=%.3f\n", maximum_speed);
    fluid_destroy(fluid);
    return maximum_speed;
}

static void translation_soak(void)
{
    fluid_t *fluid = fluid_create();
    assert(fluid);
    fluid_snapshot snapshot = {0};
    const int cycle = 6 * PHYSICS_HZ;
    for (int step = 0; step < 30 * 60 * PHYSICS_HZ; ++step) {
        int phase = step % cycle;
        bool face_up = phase < 2 * PHYSICS_HZ;
        float ax = 0.0f;
        float ay = face_up ? 0.0f : GRAVITY;
        float target_x = 0.0f, target_y = 0.0f;
        if (phase < PHYSICS_HZ ||
            (phase >= 2 * PHYSICS_HZ && phase < 3 * PHYSICS_HZ) ||
            (phase >= 4 * PHYSICS_HZ && phase < 5 * PHYSICS_HZ)) {
            float angle = 2.0f * 3.14159265f * 1.4f *
                          (phase % PHYSICS_HZ) / PHYSICS_HZ;
            float wave = 0.15f * GRAVITY * sinf(angle);
            float physical_velocity = 0.15f * physical_g /
                (2.0f * 3.14159265f * 1.4f) * cosf(angle);
            if (face_up) {
                ay += 2.0f * wave;
                target_y = physical_velocity * TRANSLATION_VELOCITY_SCALE_FACE_UP;
            } else if (phase >= 4 * PHYSICS_HZ) {
                ay += 2.0f * wave;
                target_y = physical_velocity * TRANSLATION_VELOCITY_SCALE_UPRIGHT *
                           UPRIGHT_VERTICAL_TRANSLATION_GAIN;
            } else {
                ax += 2.0f * wave;
                target_x = physical_velocity * TRANSLATION_VELOCITY_SCALE_UPRIGHT;
            }
        }
        assert(fluid_step_with_translation(fluid, ax, ay, target_x, target_y));
        if (step % 20 == 0) {
            fluid_publish(fluid, &snapshot);
            assert(snapshot.metrics.particle_count == PARTICLE_COUNT);
            assert(snapshot.metrics.render_mass_ratio >= 0.995f &&
                   snapshot.metrics.render_mass_ratio <= 1.005f);
            assert(snapshot.metrics.resets == 0);
        }
    }
    for (int step = 0; step < 15 * PHYSICS_HZ; ++step)
        assert(fluid_step(fluid, 0.0f, GRAVITY));
    fluid_publish(fluid, &snapshot);
    printf("translation_30min n=%d mass=%.6f ceiling=%d avg_speed=%.3f resets=%u\n",
           snapshot.metrics.particle_count, snapshot.metrics.render_mass_ratio,
           snapshot.metrics.ceiling_particles, snapshot.metrics.average_speed,
           snapshot.metrics.resets);
    assert(snapshot.metrics.ceiling_particles == 0);
    assert(snapshot.metrics.average_speed < 0.35f);
    fluid_destroy(fluid);
}

int main(int argc, char **argv)
{
    float upright_sine = run_axis(0, false, false, false);
    float face_up_sine = run_axis(1, true, false, false);
    float upright_pulse = run_axis(0, false, true, false);
    float face_up_pulse = run_axis(1, true, true, false);
    float upright_slide = run_axis(0, false, false, true);
    float upright_vertical_slide = run_axis(1, false, false, true);
    float face_up_slide = run_axis(1, true, false, true);
    float rotation = rotation_reference();
    assert(rotation > upright_sine && rotation > face_up_sine);
    assert(rotation > upright_pulse && rotation > face_up_pulse);
    assert(rotation > upright_slide && rotation > face_up_slide);
    assert(rotation > upright_vertical_slide);
    if (argc > 1 && strcmp(argv[1], "--soak") == 0) translation_soak();
    puts("PASS: both screen-axis translations move the logical image with bounded energy");
}
