// SPDX-License-Identifier: GPL-3.0-only
#include "board.h"
#include "app_switcher.h"
#include "fluid.h"
#include "render.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>

#include "driver/usb_serial_jtag.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static fluid_t *simulation;
static fluid_renderer *renderer;
static fluid_snapshot snapshots[3];
static portMUX_TYPE snapshot_lock = portMUX_INITIALIZER_UNLOCKED;
static int published_snapshot;
static int reading_snapshot = -1;
static const char *TAG = "fluid";

typedef struct {
    uint32_t steps;
    uint32_t maximum_step_us;
    uint64_t integration_us;
    uint64_t separation_us;
    uint64_t p2g_us;
    uint64_t density_us;
    uint64_t pressure_us;
    uint64_t g2p_us;
    uint64_t splat_us;
} physics_window;
static physics_window physics;

static int log_to_host(const char *format, va_list args)
{
    return usb_serial_jtag_is_connected() ? vprintf(format, args) : 0;
}

/* 1 kHz FreeRTOS ticks cannot express 16.666 ms directly.  This alternating
 * 16/17-tick scheduler keeps the fixed simulation step at 60 real updates/s. */
static bool delay_hz(TickType_t *wake, uint32_t *fraction, int hertz)
{
    *fraction += configTICK_RATE_HZ;
    TickType_t ticks = *fraction / hertz;
    *fraction %= hertz;
    return xTaskDelayUntil(wake, ticks) == pdTRUE;
}

static void synthetic_motion(uint32_t step, float *ax, float *ay)
{
    *ax = 0.0f;
    *ay = GRAVITY;
#if FLUID_RENDER_SELF_TEST
    /* Repeating burst for physical display validation.  It is excluded from
       production and the one-shot solver stress test. */
    uint32_t phase = step % 360u;
    if (phase < 120u) {
        float angle = (float)(phase / 6u) * 2.39996323f;
        *ax += 2.0f * GRAVITY * cosf(angle);
        *ay += 2.0f * GRAVITY * sinf(angle);
    }
#elif FLUID_SYNTHETIC_TEST
    /* Phase-2 test input: twenty directional impulses, then downward rest. */
    if (step < 20u * 6u) {
        float angle = (float)(step / 6u) * 2.39996323f;
        *ax += 2.0f * GRAVITY * cosf(angle);
        *ay += 2.0f * GRAVITY * sinf(angle);
    }
#else
    (void)step;
#endif
}

static void physics_task(void *context)
{
    (void)context;
    TickType_t wake = xTaskGetTickCount();
    uint32_t fraction = 0;
    uint32_t step = 0;
    while (true) {
        board_status_t status = {0};
        board_status(&status);
        float ax;
        float ay;
        float tx = 0.0f;
        float ty = 0.0f;
        synthetic_motion(step, &ax, &ay);
        if (status.imu_ok && !FLUID_SYNTHETIC_TEST && !FLUID_RENDER_SELF_TEST) {
            /* motion.force stays physical and unnormalised in the screen
             * plane.  This is the one conversion into artistic cell units. */
            ax = status.motion.force[0] * (GRAVITY / 9.80665f);
            ay = status.motion.force[1] * (GRAVITY / 9.80665f);
            tx = status.motion.translation_target[0] * status.motion.translation_scale[0];
            ty = status.motion.translation_target[1] * status.motion.translation_scale[1];
        }

        bool good = fluid_step_with_translation(simulation, ax, ay, tx, ty);
        int target = -1;
        portENTER_CRITICAL(&snapshot_lock);
        for (int i = 0; i < 3; ++i) {
            if (i != published_snapshot && i != reading_snapshot) {
                target = i;
                break;
            }
        }
        portEXIT_CRITICAL(&snapshot_lock);
        if (target >= 0) {
            fluid_publish(simulation, &snapshots[target]);
            snapshots[target].sensor_us = status.sample_us;
            fluid_metrics metrics = snapshots[target].metrics;
            portENTER_CRITICAL(&snapshot_lock);
            published_snapshot = target;
            ++physics.steps;
            physics.integration_us += metrics.integration_us;
            physics.separation_us += metrics.separation_us;
            physics.p2g_us += metrics.p2g_us;
            physics.density_us += metrics.density_us;
            physics.pressure_us += metrics.pressure_us;
            physics.g2p_us += metrics.g2p_us;
            physics.splat_us += metrics.render_splat_us;
            if (metrics.step_us > physics.maximum_step_us) {
                physics.maximum_step_us = metrics.step_us;
            }
            portEXIT_CRITICAL(&snapshot_lock);
        }
        if (!good) {
            ESP_LOGE(TAG, "invalid fluid state reseeded");
        }
        ++step;
        if (!delay_hz(&wake, &fraction, PHYSICS_HZ)) {
            wake = xTaskGetTickCount();
            /* An overrun must still give IDLE1 time to service its watchdog.
             * Physics state advances one fixed step; it is never catch-up
             * stepped with a larger dt. */
            vTaskDelay(1);
        }
    }
}

static void render_task(void *context)
{
    (void)context;
    TickType_t wake = xTaskGetTickCount();
    uint32_t fraction = 0;
    uint32_t frames = 0;
    uint32_t previous_imu_samples = 0;
    uint32_t sequence = 0;
    uint32_t dropped = 0;
    uint64_t display_us = 0;
    uint64_t transfer_us = 0;
    int64_t window_start = esp_timer_get_time();

    while (true) {
        portENTER_CRITICAL(&snapshot_lock);
        reading_snapshot = published_snapshot;
        int current = reading_snapshot;
        portEXIT_CRITICAL(&snapshot_lock);
        const fluid_snapshot *snapshot = &snapshots[current];
        if (sequence && snapshot->sequence > sequence + 1) {
            dropped += snapshot->sequence - sequence - 1;
        }
        sequence = snapshot->sequence;
        render_reconstruct(renderer, snapshot);
        fluid_metrics metrics = snapshot->metrics;
        portENTER_CRITICAL(&snapshot_lock);
        reading_snapshot = -1;
        portEXIT_CRITICAL(&snapshot_lock);

        board_display_metrics display = {0};
        board_present(renderer, &display);
        display_us += display.frame_us;
        transfer_us += display.transfer_us;
        ++frames;

        int64_t now = esp_timer_get_time();
        if (now - window_start >= 1000000) {
            physics_window current_physics;
            portENTER_CRITICAL(&snapshot_lock);
            current_physics = physics;
            physics = (physics_window){0};
            portEXIT_CRITICAL(&snapshot_lock);
            board_status_t status = {0};
            board_status(&status);
            uint32_t elapsed_ms = (uint32_t)((now - window_start) / 1000);
            uint32_t safe_steps = current_physics.steps ? current_physics.steps : 1;
            uint32_t safe_frames = frames ? frames : 1;
            if (FLUID_DIAGNOSTICS) {
                ESP_LOGI(
                    TAG,
                    "PERF phys_hz_x100=%lu render_fps_x100=%lu imu_hz=%lu n=%d mass_x100000=%ld "
                    "ceiling=%d "
                    "avg_v_x1000=%ld max_v_x1000=%ld integ=%lu sep=%lu p2g=%lu density=%lu "
                    "pressure=%lu g2p=%lu splat=%lu "
                    "display=%lu transfer=%lu max_step=%lu internal=%u psram=%u dropped=%lu "
                    "resets=%lu "
                    "g=(%ld,%ld,%ld) accel=(%d,%d,%d) rest_g=%ld still_ms=%ld gyro=(%d,%d,%d) "
                    "bias=(%ld,%ld,%ld) calibrated=%d "
                    "lin=(%ld,%ld) lin_peak=(%ld,%ld) force=(%ld,%ld) target=(%ld,%ld) "
                    "target_peak=(%ld,%ld) "
                    "center=(%ld,%ld) mean_v=(%ld,%ld)%s",
                    (unsigned long)(current_physics.steps * 100000u / elapsed_ms),
                    (unsigned long)(frames * 100000u / elapsed_ms),
                    (unsigned long)((status.samples - previous_imu_samples) * 1000u / elapsed_ms),
                    metrics.particle_count, (long)lroundf(metrics.render_mass_ratio * 100000.0f),
                    metrics.ceiling_particles, (long)lroundf(metrics.average_speed * 1000.0f),
                    (long)lroundf(metrics.maximum_speed * 1000.0f),
                    (unsigned long)(current_physics.integration_us / safe_steps),
                    (unsigned long)(current_physics.separation_us / safe_steps),
                    (unsigned long)(current_physics.p2g_us / safe_steps),
                    (unsigned long)(current_physics.density_us / safe_steps),
                    (unsigned long)(current_physics.pressure_us / safe_steps),
                    (unsigned long)(current_physics.g2p_us / safe_steps),
                    (unsigned long)(current_physics.splat_us / safe_steps),
                    (unsigned long)(display_us / safe_frames),
                    (unsigned long)(transfer_us / safe_frames),
                    (unsigned long)current_physics.maximum_step_us,
                    heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                    heap_caps_get_free_size(MALLOC_CAP_SPIRAM), (unsigned long)dropped,
                    (unsigned long)metrics.resets,
                    (long)lroundf(status.motion.gravity[0] * 1000.0f),
                    (long)lroundf(status.motion.gravity[1] * 1000.0f),
                    (long)lroundf(status.motion.gravity[2] * 1000.0f), status.accel[0],
                    status.accel[1], status.accel[2],
                    (long)lroundf(status.rest_gravity_magnitude * 1000.0f),
                    (long)lroundf(status.stationary_seconds * 1000.0f), status.gyro[0],
                    status.gyro[1], status.gyro[2], (long)lroundf(status.gyro_bias[0] * 1000.0f),
                    (long)lroundf(status.gyro_bias[1] * 1000.0f),
                    (long)lroundf(status.gyro_bias[2] * 1000.0f), status.gyro_calibrated,
                    (long)lroundf(status.motion.linear_device_acceleration[0] * 1000.0f),
                    (long)lroundf(status.motion.linear_device_acceleration[1] * 1000.0f),
                    (long)lroundf(status.linear_peak[0] * 1000.0f),
                    (long)lroundf(status.linear_peak[1] * 1000.0f),
                    (long)lroundf(status.motion.force[0] * 1000.0f),
                    (long)lroundf(status.motion.force[1] * 1000.0f),
                    (long)lroundf(status.motion.translation_target[0] *
                                  status.motion.translation_scale[0] * 1000.0f),
                    (long)lroundf(status.motion.translation_target[1] *
                                  status.motion.translation_scale[1] * 1000.0f),
                    (long)lroundf(status.translation_peak[0] * 1000.0f),
                    (long)lroundf(status.translation_peak[1] * 1000.0f),
                    (long)lroundf(metrics.centroid_x * 1000.0f),
                    (long)lroundf(metrics.centroid_y * 1000.0f),
                    (long)lroundf(metrics.mean_vx * 1000.0f),
                    (long)lroundf(metrics.mean_vy * 1000.0f),
                    FLUID_IMU_DIAGNOSTIC ? " IMU_DIAG" : "");
            }
            previous_imu_samples = status.samples;
            frames = dropped = 0;
            display_us = transfer_us = 0;
            window_start = now;
        }
        if (!delay_hz(&wake, &fraction, RENDER_HZ)) {
            wake = xTaskGetTickCount();
        }
    }
}

void app_main(void)
{
    app_switcher_init();
    usb_serial_jtag_driver_config_t usb = {.rx_buffer_size = 256, .tx_buffer_size = 2048};
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb));
    esp_log_set_vprintf(log_to_host);
    board_init();
    board_brightness(0xd0);
    simulation = fluid_create();
    renderer = render_create();
    ESP_ERROR_CHECK(simulation && renderer ? ESP_OK : ESP_ERR_NO_MEM);
    fluid_publish(simulation, &snapshots[0]);
    ESP_LOGI(TAG, "READY V9 FLIP/PIC 536x240 n=%d solver_bytes=%u", PARTICLE_COUNT,
             (unsigned)fluid_memory_bytes());
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(physics_task, "physics", 6144, NULL, 3, NULL, 1) ==
                            pdPASS
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(render_task, "renderer", 6144, NULL, 2, NULL, 0) ==
                            pdPASS
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
}
