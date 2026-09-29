#include "motion.h"
#include "fluid_config.h"

#include <math.h>
#include <string.h>

static float norm3(const float value[3])
{
    return sqrtf(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
}

static void map_vector(const float map[3][3], const float source[3], float target[3])
{
    for (int row = 0; row < 3; ++row) {
        target[row] = 0.0f;
        for (int column = 0; column < 3; ++column) target[row] += map[row][column] * source[column];
    }
}

static float clampf(float value, float low, float high)
{
    return value < low ? low : (value > high ? high : value);
}

/* The reference follows settled orientation, so a brief sway has more effect
 * than a held tilt.  Large rotations receive almost no extra gain. */
static void tilt_response(const float gravity[3], const float reference[3], float response[2])
{
    float delta[3];
    for (int i = 0; i < 3; ++i) delta[i] = gravity[i] - reference[i];
    float ratio = norm3(delta) / WALK_TILT_KNEE;
    float extra = WALK_TILT_EXTRA_GAIN / (1.0f + ratio * ratio);
    response[0] = (gravity[0] + extra * (gravity[0] - reference[0])) *
                  TILT_RESPONSE_GAIN * TILT_HORIZONTAL_GAIN;
    response[1] = (gravity[1] + extra * (gravity[1] - reference[1])) * TILT_RESPONSE_GAIN;
    float length = hypotf(response[0], response[1]);
    if (length > 9.80665f) {
        float scale = 9.80665f / length;
        response[0] *= scale;
        response[1] *= scale;
    }
}

bool motion_update(motion_filter *filter, const float accel[3], const float gyro[3], float dt,
                   const float gravity_map[3][3], motion_output *out)
{
    if (!isfinite(dt) || dt <= 0.0f || dt > 0.1f) return false;
    for (int i = 0; i < 3; ++i)
        if (!isfinite(accel[i]) || !isfinite(gyro[i])) return false;

    const float gravity_length = norm3(accel);
    if (!isfinite(gravity_length) || gravity_length < 0.1f) return false;
    if (!filter->initialized) {
        if (gravity_length < 4.0f || gravity_length > 20.0f) return false;
        for (int i = 0; i < 3; ++i) {
            filter->support[i] = accel[i] * (9.80665f / gravity_length);
            filter->tilt_reference[i] = filter->support[i];
        }
        filter->rest_gravity_magnitude = gravity_length;
        filter->initialized = true;
    }

    float angular_velocity[3];
    for (int i = 0; i < 3; ++i) angular_velocity[i] = gyro[i] - filter->gyro_bias[i];
    float old_support[3] = {filter->support[0], filter->support[1], filter->support[2]};
    /* A world-fixed vector is seen rotating by -omega cross vector in device
     * coordinates.  The accelerometer correction below prevents gyro drift. */
    filter->support[0] -= dt * (angular_velocity[1] * old_support[2] - angular_velocity[2] * old_support[1]);
    filter->support[1] -= dt * (angular_velocity[2] * old_support[0] - angular_velocity[0] * old_support[2]);
    filter->support[2] -= dt * (angular_velocity[0] * old_support[1] - angular_velocity[1] * old_support[0]);

    float confidence = clampf(1.0f - fabsf(gravity_length - 9.80665f) / 2.0f, 0.0f, 1.0f);
    float alpha = dt / (GRAVITY_FILTER_TAU + dt) * confidence;
    for (int i = 0; i < 3; ++i)
        filter->support[i] += alpha * (accel[i] * (9.80665f / gravity_length) - filter->support[i]);

    float support_length = norm3(filter->support);
    if (!isfinite(support_length) || support_length < 0.1f) {
        memset(filter, 0, sizeof(*filter));
        return false;
    }
    for (int i = 0; i < 3; ++i) filter->support[i] *= 9.80665f / support_length;

    /* The sensor's rest magnitude is a property of this physical board, not
       a device acceleration.  Compare against its calibrated rest value
       rather than an ideal g, so the gyro bias can settle even when the
       accelerometer scale is a few percent off. */
    float still_rate = filter->gyro_bias_ready ? GYRO_STATIONARY_THRESHOLD :
                                                GYRO_INITIAL_STATIONARY_THRESHOLD;
    /* The first sample may be taken during a shove.  Until the gyro has a
     * bias, compare gravity magnitude within the candidate still window;
     * otherwise a bad first sample can prevent calibration forever. */
    bool plausible_rest = filter->gyro_bias_ready ?
        fabsf(gravity_length - filter->rest_gravity_magnitude) < 0.75f :
        gravity_length > 7.0f && gravity_length < 12.0f;
    bool stationary = plausible_rest && norm3(angular_velocity) < still_rate;
    float measured_direction[3];
    for (int i = 0; i < 3; ++i)
        measured_direction[i] = accel[i] * (9.80665f / gravity_length);
    if (filter->stationary_samples == 0) {
        memcpy(filter->stationary_reference, measured_direction, sizeof(measured_direction));
        filter->stationary_magnitude_reference = gravity_length;
    } else {
        float direction_change[3];
        for (int i = 0; i < 3; ++i)
            direction_change[i] = measured_direction[i] - filter->stationary_reference[i];
        stationary = stationary && norm3(direction_change) < STATIONARY_POSE_TOLERANCE &&
                     fabsf(gravity_length - filter->stationary_magnitude_reference) < 0.20f;
    }
    if (stationary) {
        if (filter->gyro_bias_ready)
            filter->rest_gravity_magnitude += 0.01f * (gravity_length - filter->rest_gravity_magnitude);
        for (int i = 0; i < 3; ++i) filter->gyro_sum[i] += gyro[i];
        filter->stationary_seconds += dt;
        ++filter->stationary_samples;
        if (filter->stationary_seconds >= 1.0f) {
            if (!filter->gyro_bias_ready)
                filter->rest_gravity_magnitude = filter->stationary_magnitude_reference;
            for (int i = 0; i < 3; ++i) {
                filter->gyro_bias[i] = filter->gyro_sum[i] / filter->stationary_samples;
                filter->gyro_sum[i] = 0.0f;
            }
            filter->stationary_seconds = 0.0f;
            filter->stationary_samples = 0;
            filter->gyro_bias_ready = true;
        }
    } else {
        memset(filter->gyro_sum, 0, sizeof(filter->gyro_sum));
        filter->stationary_seconds = 0.0f;
        filter->stationary_samples = 0;
    }

    const float support_scale = filter->rest_gravity_magnitude / 9.80665f;
    for (int i = 0; i < 3; ++i) {
        float raw_linear = accel[i] - filter->support[i] * support_scale;
        filter->linear[i] += dt / (LINEAR_FILTER_TAU + dt) * (raw_linear - filter->linear[i]);
    }

    map_vector(gravity_map, filter->support, out->gravity);
    /* gravity_map includes the sign inversion from accelerometer specific
     * force to gravity.  Its negative therefore maps linear specific force
     * into physical screen acceleration. */
    float mapped_linear[3];
    map_vector(gravity_map, filter->linear, mapped_linear);
    for (int i = 0; i < 3; ++i) {
        out->linear_device_acceleration[i] = -mapped_linear[i];
        out->linear_device_acceleration[i] = clampf(out->linear_device_acceleration[i],
                                                     -MAX_LINEAR_ACCELERATION, MAX_LINEAR_ACCELERATION);
        out->fluid_acceleration[i] = -out->linear_device_acceleration[i];
    }
    /* A pitch rotates gravity out of screen-down.  If the accelerometer
     * magnitude stays near its rest value, a temporary filter residual is
     * rotation, not a vertical enclosure slide.  Real vertical movement
     * changes that magnitude and retains the full inertial response. */
    float mapped_gyro[3];
    map_vector(gravity_map, angular_velocity, mapped_gyro);
    float pitch = clampf((fabsf(mapped_gyro[0]) - PITCH_GATE_START) /
                         (PITCH_GATE_FULL - PITCH_GATE_START), 0.0f, 1.0f);
    float guard_decay = PITCH_GATE_DECAY_TAU / (PITCH_GATE_DECAY_TAU + dt);
    filter->pitch_guard = fmaxf(pitch, filter->pitch_guard * guard_decay);
    float magnitude_change = fabsf(gravity_length - filter->rest_gravity_magnitude);
    float slide_evidence = clampf((magnitude_change - VERTICAL_SLIDE_START) /
                                  (VERTICAL_SLIDE_FULL - VERTICAL_SLIDE_START), 0.0f, 1.0f);
    float upright_alignment = clampf((out->gravity[1] / 9.80665f - 0.60f) / 0.30f, 0.0f, 1.0f);
    float vertical_gate = 1.0f - filter->pitch_guard * upright_alignment * (1.0f - slide_evidence);
    float effective_fluid_y = out->fluid_acceleration[1] * vertical_gate;

    float mapped_reference[3];
    map_vector(gravity_map, filter->tilt_reference, mapped_reference);
    float tilt[2];
    tilt_response(out->gravity, mapped_reference, tilt);
    float reference_alpha = dt / (WALK_TILT_REFERENCE_TAU + dt);
    for (int i = 0; i < 3; ++i)
        filter->tilt_reference[i] += reference_alpha * (filter->support[i] - filter->tilt_reference[i]);

    float motion_length = hypotf(out->fluid_acceleration[0], effective_fluid_y);
    float motion_ratio = motion_length / WALK_PUSH_KNEE;
    float push = PUSH_GAIN + WALK_PUSH_EXTRA_GAIN / (1.0f + motion_ratio * motion_ratio);
    out->force[0] = tilt[0] + out->fluid_acceleration[0] * push;
    out->force[1] = tilt[1] + effective_fluid_y * push;
    float in_plane_g = clampf(hypotf(out->gravity[0], out->gravity[1]) / 9.80665f, 0.0f, 1.0f);
    float scale = TRANSLATION_VELOCITY_SCALE_FACE_UP +
        (TRANSLATION_VELOCITY_SCALE_UPRIGHT - TRANSLATION_VELOCITY_SCALE_FACE_UP) * in_plane_g;
    float upright_down = clampf((out->gravity[1] / 9.80665f - 0.70f) / 0.30f, 0.0f, 1.0f);
    out->translation_scale[0] = scale;
    /* Keep a real up/down slide, but avoid applying the full upright vertical
     * speed boost to the tangential acceleration produced by pitching. */
    out->translation_scale[1] = scale * (1.0f + (UPRIGHT_VERTICAL_TRANSLATION_GAIN - 1.0f) *
        upright_down * (1.0f - PITCH_VERTICAL_BOOST_REDUCTION * filter->pitch_guard));
    float gravity_rate_x = 0.0f;
    if (filter->gravity_history_ready)
        gravity_rate_x = (out->gravity[0] - filter->previous_gravity_x) / (9.80665f * dt);
    filter->previous_gravity_x = out->gravity[0];
    filter->gravity_history_ready = true;
    float roll_gate = clampf((fabsf(mapped_gyro[2]) - TILT_SWEEP_RATE_START) /
                             (TILT_SWEEP_RATE_FULL - TILT_SWEEP_RATE_START), 0.0f, 1.0f);
    float sweep_alpha = dt / (TILT_SWEEP_FILTER_TAU + dt);
    filter->tilt_sweep_x += sweep_alpha *
        (gravity_rate_x * roll_gate * TILT_SWEEP_CELLS_PER_RAD - filter->tilt_sweep_x);
    filter->tilt_sweep_x = clampf(filter->tilt_sweep_x,
                                   -TILT_SWEEP_MAX_TARGET, TILT_SWEEP_MAX_TARGET);
    /* Strong shifts still carry enclosure momentum.  Limit their contribution
     * to the short velocity estimate instead of clearing it at the threshold. */
    float translation_acceleration_scale = motion_length > TRANSLATION_ACTIVITY_LIMIT ?
        TRANSLATION_ACTIVITY_LIMIT / motion_length : 1.0f;
    float strong_acceleration_x = motion_length > TRANSLATION_ACTIVITY_LIMIT ?
        out->fluid_acceleration[0] / motion_length *
        fminf(motion_length - TRANSLATION_ACTIVITY_LIMIT,
              STRONG_TRANSLATION_ACCELERATION_CAP) : 0.0f;
    if (filter->strong_translation_velocity_x * strong_acceleration_x < 0.0f)
        filter->strong_translation_velocity_x = 0.0f;
    float strong_decay = STRONG_TRANSLATION_VELOCITY_TAU /
                         (STRONG_TRANSLATION_VELOCITY_TAU + dt);
    filter->strong_translation_velocity_x = clampf(
        (filter->strong_translation_velocity_x + strong_acceleration_x * dt) * strong_decay,
        -STRONG_TRANSLATION_VELOCITY_MAX, STRONG_TRANSLATION_VELOCITY_MAX);
    for (int i = 0; i < 2; ++i) {
        if (i == 1) filter->translation_velocity[i] *= vertical_gate;
        float decay = TRANSLATION_VELOCITY_TAU / (TRANSLATION_VELOCITY_TAU + dt);
        filter->translation_velocity[i] = clampf(
            (filter->translation_velocity[i] + (i == 1 ? effective_fluid_y :
                out->fluid_acceleration[i]) * translation_acceleration_scale * dt) * decay,
            -TRANSLATION_VELOCITY_MAX, TRANSLATION_VELOCITY_MAX);
        float velocity = filter->translation_velocity[i] +
                         (i == 0 ? filter->strong_translation_velocity_x : 0.0f);
        float excess = fmaxf(fabsf(velocity) - TRANSLATION_VELOCITY_DEADZONE, 0.0f);
        out->translation_target[i] = copysignf(excess, velocity) *
                                     (i == 1 ? vertical_gate : 1.0f);
    }
    out->translation_target[0] = clampf(out->translation_target[0] +
        filter->tilt_sweep_x / scale,
        -TILT_SWEEP_MAX_TARGET / scale - TRANSLATION_VELOCITY_MAX -
            STRONG_TRANSLATION_VELOCITY_MAX,
         TILT_SWEEP_MAX_TARGET / scale + TRANSLATION_VELOCITY_MAX +
            STRONG_TRANSLATION_VELOCITY_MAX);
    return isfinite(out->force[0]) && isfinite(out->force[1]);
}
