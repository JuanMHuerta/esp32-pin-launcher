// SPDX-License-Identifier: GPL-3.0-only
#include "board.h"
#include <math.h>
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_sh8601.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "board";
static esp_lcd_panel_handle_t panel;
static esp_lcd_panel_io_handle_t panel_io;
static SemaphoreHandle_t display_done;
static uint16_t *bands[2];
static i2c_master_bus_handle_t i2c;
static i2c_master_dev_handle_t imu;
static QueueHandle_t status_queue;

/* Waveshare's SH8601 ESP-IDF transport is the proven RM67162-compatible path
 * for this board revision.  The window is deliberately landscape 536 x 240. */
static const sh8601_lcd_init_cmd_t panel_init[] = {
    {0x11, NULL, 0, 120},
    {0x36, (uint8_t[]){0xf0}, 1, 0},
    {0x3a, (uint8_t[]){0x55}, 1, 0},
    {0x2a, (uint8_t[]){0x00, 0x00, 0x02, 0x17}, 4, 0},
    {0x2b, (uint8_t[]){0x00, 0x00, 0x00, 0xef}, 4, 0},
    {0x51, (uint8_t[]){0x00}, 1, 10},
    {0x29, NULL, 0, 10},
};
static bool transfer_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *data,
                          void *context)
{
    (void)io;
    (void)data;
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)context, &wake);
    return wake == pdTRUE;
}
static void display_open(void)
{
    display_done = xSemaphoreCreateBinary();
    ESP_ERROR_CHECK(display_done ? ESP_OK : ESP_ERR_NO_MEM);
    spi_bus_config_t bus = SH8601_PANEL_BUS_QSPI_CONFIG(
        47, 18, 7, 48, 5, PANEL_W * FLOW_BAND_ROWS * sizeof(uint16_t) + 64);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    esp_lcd_panel_io_spi_config_t io = SH8601_PANEL_IO_QSPI_CONFIG(6, transfer_done, display_done);
    /* Two DMA buffers let each 48-row band be rendered without a full-frame buffer. */
    io.trans_queue_depth = 2;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io, &panel_io));
    sh8601_vendor_config_t vendor = {.init_cmds = panel_init,
                                     .init_cmds_size = sizeof(panel_init) / sizeof(panel_init[0]),
                                     .flags.use_qspi_interface = 1};
    const esp_lcd_panel_dev_config_t config = {.reset_gpio_num = 17,
                                               .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
                                               .bits_per_pixel = 16,
                                               .vendor_config = &vendor};
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(panel_io, &config, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    for (int i = 0; i < 2; ++i) {
        bands[i] = heap_caps_malloc(PANEL_W * FLOW_BAND_ROWS * sizeof(uint16_t),
                                    MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        ESP_ERROR_CHECK(bands[i] ? ESP_OK : ESP_ERR_NO_MEM);
    }
}
static void wait_display(void)
{
    if (xSemaphoreTake(display_done, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "LCD DMA completion timeout");
        esp_restart();
    }
}

void board_present(const fluid_renderer *r, board_display_metrics *m)
{
    *m = (board_display_metrics){0};
    int64_t start = esp_timer_get_time();
    int64_t pending_start = 0;
    /* Use the panel driver's own CASET/RASET/RAMWR sequence for each band.
       This is deliberately conservative while validating physical output: it
       removes assumptions about this panel's RAM-write continuation semantics. */
    for (int y = 0; y < PANEL_H; y += FLOW_BAND_ROWS) {
        uint16_t *out = bands[(y / FLOW_BAND_ROWS) & 1];
        int64_t t = esp_timer_get_time();
        render_band(r, y, FLOW_BAND_ROWS, out);
        m->rgb_us += esp_timer_get_time() - t;
        /* Render into the other DMA buffer while the preceding band is sent.
         * Finish that transfer before issuing the next panel window command. */
        if (pending_start) {
            wait_display();
            m->transfer_us += esp_timer_get_time() - pending_start;
        }
        pending_start = esp_timer_get_time();
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, y, PANEL_W, y + FLOW_BAND_ROWS, out));
    }
    wait_display();
    m->transfer_us += esp_timer_get_time() - pending_start;
    m->frame_us = esp_timer_get_time() - start;
}
void board_brightness(uint8_t value)
{
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(panel_io, (0x02 << 24) | (0x51 << 8), &value, 1));
}
static esp_err_t read_reg(uint8_t reg, void *buf, size_t size)
{
    return i2c_master_transmit_receive(imu, &reg, 1, buf, size, 10);
}
static void write_reg(uint8_t reg, uint8_t val)
{
    uint8_t data[] = {reg, val};
    ESP_ERROR_CHECK(i2c_master_transmit(imu, data, sizeof(data), 10));
}
static void sensor_task(void *context)
{
    (void)context;
    board_status_t s = {0};
    motion_filter filter = {0};
    static const float screen_gravity_map[3][3] = IMU_TO_SCREEN_GRAVITY;
    TickType_t wake = xTaskGetTickCount();
    int64_t last = 0;
    while (true) {
        uint8_t status = 0;
        if (read_reg(0x2e, &status, 1) != ESP_OK) {
            ++s.errors;
        } else if ((status & 3) == 3) {
            uint8_t raw[12];
            if (read_reg(0x35, raw, sizeof(raw)) == ESP_OK) {
                float a[3], w[3];
                for (int i = 0; i < 3; ++i) {
                    s.accel[i] = (int16_t)(raw[i * 2] | (raw[i * 2 + 1] << 8));
                    s.gyro[i] = (int16_t)(raw[i * 2 + 6] | (raw[i * 2 + 7] << 8));
                    a[i] = s.accel[i] * (9.81f / 4096);
                    w[i] = s.gyro[i] * (3.14159265f / (180 * 32));
                }
                int64_t now = esp_timer_get_time();
                float dt = last ? (now - last) * 1e-6f : 1.0f / IMU_HZ;
                last = now;
                s.imu_ok = motion_update(&filter, a, w, dt, screen_gravity_map, &s.motion);
                if (s.imu_ok) {
                    for (int axis = 0; axis < 3; ++axis) {
                        s.gyro_bias[axis] = filter.gyro_bias[axis];
                    }
                    s.rest_gravity_magnitude = filter.rest_gravity_magnitude;
                    s.stationary_seconds = filter.stationary_seconds;
                    s.gyro_calibrated = filter.gyro_bias_ready;
                    for (int axis = 0; axis < 2; ++axis) {
                        float magnitude = fabsf(s.motion.linear_device_acceleration[axis]);
                        if (magnitude > s.linear_peak[axis]) {
                            s.linear_peak[axis] = magnitude;
                        }
                        float target = fabsf(s.motion.translation_target[axis] *
                                             s.motion.translation_scale[axis]);
                        if (target > s.translation_peak[axis]) {
                            s.translation_peak[axis] = target;
                        }
                    }
                    ++s.samples;
                    s.sample_us = now;
                }
            } else {
                ++s.errors;
            }
        }
        if (last && esp_timer_get_time() - last > 100000) {
            s.imu_ok = false;
        }
        xQueueOverwrite(status_queue, &s);
        xTaskDelayUntil(&wake, pdMS_TO_TICKS(1000 / IMU_HZ));
    }
}
void board_init(void)
{
    display_open();
    i2c_master_bus_config_t bus = {.i2c_port = I2C_NUM_0,
                                   .sda_io_num = 40,
                                   .scl_io_num = 39,
                                   .clk_source = I2C_CLK_SRC_DEFAULT,
                                   .glitch_ignore_cnt = 7,
                                   .flags.enable_internal_pullup = true};
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus, &i2c));
    /* This board's QMI8658 is wired at 0x6b (verified against its WhoAmI
       register).  Avoid an address scan here: an absent-address probe can
       hold the driver in the hardware clock-stretch timeout during boot and
       starve IDLE0 long enough to trip the task watchdog. */
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = 0x6b, .scl_speed_hz = 400000};
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c, &cfg, &imu));
    /* The QMI8658 may still be coming out of reset immediately after an
       ESP32 hard reset.  Retry for 500 ms, yielding between attempts so this
       cannot starve the watchdog. */
    uint8_t id = 0;
    for (int attempt = 0; attempt < 25; ++attempt) {
        if (read_reg(0, &id, 1) == ESP_OK && id == 5) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    ESP_ERROR_CHECK(id == 5 ? ESP_OK : ESP_ERR_NOT_FOUND);
    /* QMI8658: 8g, 1024 dps, ODR setting 5 (about 224.2 Hz in 6DOF mode).
       Poll fresh accel+gyro samples at 125 Hz, independently of frame rate. */
    write_reg(8, 0);
    write_reg(2, 0x60);
    write_reg(3, 0x25);
    write_reg(4, 0x65);
    write_reg(6, 0);
    write_reg(8, 3);
    vTaskDelay(pdMS_TO_TICKS(80));
    status_queue = xQueueCreate(1, sizeof(board_status_t));
    ESP_ERROR_CHECK(status_queue ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(sensor_task, "imu", 4096, NULL, 4, NULL, 0) == pdPASS
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "RM67162 via Waveshare SH8601 init, landscape 536x240, QSPI DMA, IMU %dHz",
             IMU_HZ);
}
void board_status(board_status_t *out)
{
    xQueuePeek(status_queue, out, 0);
}
