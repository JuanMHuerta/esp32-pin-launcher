// SPDX-License-Identifier: GPL-3.0-only
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_lcd_sh8601.h"
#include "app_switcher.h"
#include "renderer.h"
#include "motion.h"

#define STRIP_ROWS 60
#define FRAME_MS 30
#define LCD_HOST SPI2_HOST
#define LCD_CS 6
#define LCD_CLK 47
#define LCD_D0 18
#define LCD_D1 7
#define LCD_D2 48
#define LCD_D3 5
#define LCD_RST 17
#define I2C_SDA 40
#define I2C_SCL 39
#define TOUCH_INT 41
#define TOUCH_ADDR 0x38
#define QMI_WHO_AM_I 0x00
#define QMI_CTRL1 0x02
#define QMI_CTRL2 0x03
#define QMI_CTRL3 0x04
#define QMI_CTRL5 0x06
#define QMI_CTRL7 0x08
#define QMI_ACCEL_X 0x35

_Static_assert(PIN_HEIGHT % STRIP_ROWS == 0, "strip rows must cover display");

static const char *TAG = "lumen_pin";
static SemaphoreHandle_t display_done;
static i2c_master_bus_handle_t i2c_bus;
static i2c_master_dev_handle_t touch_device;
static i2c_master_dev_handle_t imu_device;
static TaskHandle_t touch_task_handle;
static QueueHandle_t touch_events;
static renderer_t scene;

typedef struct {
    uint16_t x, y;
} touch_t;
typedef struct {
    motion_filter_t filter;
    int64_t last_sample_us;
    uint32_t samples;
} motion_t;

/* Exact 536 x 240 landscape sequence from Waveshare's 1.91-inch IDF demo. */
static const sh8601_lcd_init_cmd_t panel_init[] = {
    {0x11, NULL, 0, 120},
    {0x36, (uint8_t[]){0xF0}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x02, 0x17}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x00, 0xEF}, 4, 0},
    {0x51, (uint8_t[]){0x00}, 1, 10},
    {0x29, NULL, 0, 10},
    {0x51, (uint8_t[]){0xB8}, 1, 0},
};

static bool transfer_finished(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *event,
                              void *context)
{
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)context, &wake);
    return wake == pdTRUE;
}

static esp_lcd_panel_handle_t open_display(void)
{
    display_done = xSemaphoreCreateBinary();
    ESP_ERROR_CHECK(display_done ? ESP_OK : ESP_ERR_NO_MEM);
    const spi_bus_config_t bus = SH8601_PANEL_BUS_QSPI_CONFIG(
        LCD_CLK, LCD_D0, LCD_D1, LCD_D2, LCD_D3, PIN_WIDTH * STRIP_ROWS * sizeof(uint16_t) + 64);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_config =
        SH8601_PANEL_IO_QSPI_CONFIG(LCD_CS, transfer_finished, display_done);
    io_config.trans_queue_depth = 1;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io));
    sh8601_vendor_config_t vendor = {
        .init_cmds = panel_init,
        .init_cmds_size = sizeof(panel_init) / sizeof(panel_init[0]),
        .flags.use_qspi_interface = 1,
    };
    const esp_lcd_panel_dev_config_t config = {
        .reset_gpio_num = LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor,
    };
    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io, &config, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    return panel;
}

static void open_i2c(void)
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));
}

static i2c_master_dev_handle_t add_i2c_device(uint8_t address)
{
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 400000,
    };
    i2c_master_dev_handle_t device = NULL;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &config, &device));
    return device;
}

static esp_err_t read_regs(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *data, size_t count)
{
    return i2c_master_transmit_receive(dev, &reg, 1, data, count, 20);
}

static esp_err_t write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    return i2c_master_transmit(dev, data, sizeof(data), 20);
}

static void open_imu(void)
{
    static const uint8_t addresses[] = {0x6b, 0x6a};
    for (int i = 0; i < 2; ++i) {
        i2c_master_dev_handle_t candidate = add_i2c_device(addresses[i]);
        uint8_t id = 0;
        if (read_regs(candidate, QMI_WHO_AM_I, &id, 1) == ESP_OK && id == 0x05) {
            imu_device = candidate;
            /* 8 g accelerometer at 125 Hz; 1024 dps gyro at 250 Hz. */
            ESP_ERROR_CHECK(write_reg(imu_device, QMI_CTRL7, 0x00));
            ESP_ERROR_CHECK(write_reg(imu_device, QMI_CTRL1, 0x60));
            ESP_ERROR_CHECK(write_reg(imu_device, QMI_CTRL2, 0x26));
            ESP_ERROR_CHECK(write_reg(imu_device, QMI_CTRL3, 0x65));
            ESP_ERROR_CHECK(write_reg(imu_device, QMI_CTRL5, 0x00));
            ESP_ERROR_CHECK(write_reg(imu_device, QMI_CTRL7, 0x03));
            /* Let fresh samples arrive before capturing the neutral pose. */
            vTaskDelay(pdMS_TO_TICKS(40));
            ESP_LOGI(TAG, "QMI8658 ready at 0x%02x", addresses[i]);
            return;
        }
        ESP_ERROR_CHECK(i2c_master_bus_rm_device(candidate));
    }
    ESP_LOGW(TAG, "QMI8658 absent; animation will run without motion input");
}

static void sample_imu(motion_t *motion, int64_t now_us)
{
    if (!imu_device) {
        return;
    }
    uint8_t sample[12];
    if (read_regs(imu_device, QMI_ACCEL_X, sample, sizeof(sample)) != ESP_OK) {
        return;
    }
    float accel[3];
    for (int axis = 0; axis < 3; ++axis) {
        int16_t raw = (int16_t)((uint16_t)sample[axis * 2] | ((uint16_t)sample[axis * 2 + 1] << 8));
        accel[axis] = raw / 4096.0f;
    }
    float gyro[3];
    for (int axis = 0; axis < 3; ++axis) {
        int offset = 6 + axis * 2;
        int16_t raw = (int16_t)((uint16_t)sample[offset] | ((uint16_t)sample[offset + 1] << 8));
        gyro[axis] = raw / 32.0f;
    }
    float dt = motion->last_sample_us ? (now_us - motion->last_sample_us) / 1000000.0f : 0.04f;
    motion_update(&motion->filter, accel, gyro, dt);
    motion->last_sample_us = now_us;
    motion->samples++;
}

static void IRAM_ATTR touch_interrupt(void *context)
{
    BaseType_t wake = pdFALSE;
    vTaskNotifyGiveFromISR(touch_task_handle, &wake);
    if (wake == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void touch_task(void *context)
{
    bool was_down = false;
    int64_t last_report_us = 0;
    int64_t last_error_us = -1000000;
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        uint8_t reg = 0x02;
        uint8_t report[5];
        esp_err_t result = read_regs(touch_device, reg, report, sizeof(report));
        if (result != ESP_OK) {
            int64_t now = esp_timer_get_time();
            if (now - last_error_us >= 1000000) {
                ESP_LOGW(TAG, "FT3168 report read failed: %s", esp_err_to_name(result));
                last_error_us = now;
            }
            was_down = false;
            continue;
        }
        int event = report[1] >> 6;
        if (!(report[0] & 0x0f) || event == 1) {
            was_down = false;
            continue;
        }
        if (event != 0 && event != 2) {
            continue;
        }
        int64_t now = esp_timer_get_time();
        if (!was_down || (event == 0 && now - last_report_us > 300000)) {
            /* First FT3168 coordinate is screen Y; second is screen X. */
            int raw_y = ((report[1] & 0x0f) << 8) | report[2];
            int raw_x = ((report[3] & 0x0f) << 8) | report[4];
            if (raw_x >= 0 && raw_x < PIN_WIDTH && raw_y >= 0 && raw_y < PIN_HEIGHT) {
                touch_t touch = {(uint16_t)raw_x, (uint16_t)(PIN_HEIGHT - 1 - raw_y)};
                xQueueOverwrite(touch_events, &touch);
            }
        }
        was_down = true;
        last_report_us = now;
    }
}

static void open_touch(void)
{
    touch_device = add_i2c_device(TOUCH_ADDR);
    touch_events = xQueueCreate(1, sizeof(touch_t));
    ESP_ERROR_CHECK(touch_events ? ESP_OK : ESP_ERR_NO_MEM);
    const gpio_config_t gpio = {
        .pin_bit_mask = 1ULL << TOUCH_INT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&gpio));
    ESP_ERROR_CHECK(xTaskCreate(touch_task, "ft3168_touch", 3072, NULL, 5, &touch_task_handle) ==
                            pdPASS
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(TOUCH_INT, touch_interrupt, NULL));
    ESP_LOGI(TAG, "FT3168 interrupt input installed; tap to change palette");
}

void app_main(void)
{
    app_switcher_init();
    renderer_init();
    esp_lcd_panel_handle_t panel = open_display();
    uint16_t *strips[2];
    for (int i = 0; i < 2; ++i) {
        strips[i] = heap_caps_malloc(PIN_WIDTH * STRIP_ROWS * sizeof(uint16_t),
                                     MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    }
    ESP_ERROR_CHECK(strips[0] ? ESP_OK : ESP_ERR_NO_MEM);
    if (!strips[1]) {
        ESP_LOGW(TAG, "second DMA strip unavailable; using serial display transfers");
    }
    open_i2c();
    open_imu();
    open_touch();

    motion_t motion = {0};
    uint8_t mood = 0;
    int pulse_x = PIN_WIDTH / 2, pulse_y = PIN_HEIGHT / 2;
    int64_t start_us = esp_timer_get_time();
    int64_t pulse_start_us = -2000000;
    TickType_t frame_tick = xTaskGetTickCount();
    uint32_t frame_count = 0;
    uint32_t work_time_ms = 0;
    uint32_t render_time_ms = 0;
    uint32_t wait_time_ms = 0;
    int64_t report_start_us = start_us;
    ESP_LOGI(TAG, "Lumen pin: %dx%d, %d fps target, %d-row DMA strips, pipeline=%s", PIN_WIDTH,
             PIN_HEIGHT, 1000 / FRAME_MS, STRIP_ROWS, strips[1] ? "on" : "off");
    while (true) {
        int64_t now = esp_timer_get_time();
        sample_imu(&motion, now);
        touch_t touch;
        if (touch_events && xQueueReceive(touch_events, &touch, 0) == pdTRUE) {
            mood = (mood + 1) % 3;
            pulse_x = touch.x;
            pulse_y = touch.y;
            pulse_start_us = now;
            ESP_LOGI(TAG, "palette=%u touch=(%u,%u)", mood, touch.x, touch.y);
        }
        int32_t age_ms = (int32_t)((now - pulse_start_us) / 1000);
        if (age_ms > 1900) {
            age_ms = -1;
        }
        renderer_prepare(&scene, (int32_t)((now - start_us) / 1000), motion.filter.view_x,
                         motion.filter.view_y, motion.filter.roll, motion.filter.energy, mood,
                         pulse_x, pulse_y, age_ms);
        int64_t draw_start = esp_timer_get_time();
        bool transfer_pending = false;
        for (int y = 0; y < PIN_HEIGHT; y += STRIP_ROWS) {
            if (transfer_pending && !strips[1]) {
                int64_t wait_start = esp_timer_get_time();
                if (xSemaphoreTake(display_done, pdMS_TO_TICKS(1000)) != pdTRUE) {
                    ESP_LOGE(TAG, "display transfer timed out; restarting");
                    esp_restart();
                }
                wait_time_ms += (uint32_t)((esp_timer_get_time() - wait_start) / 1000);
                transfer_pending = false;
            }
            uint16_t *strip = strips[strips[1] ? (y / STRIP_ROWS) & 1 : 0];
            int64_t render_start = esp_timer_get_time();
            renderer_strip(&scene, y, STRIP_ROWS, strip);
            render_time_ms += (uint32_t)((esp_timer_get_time() - render_start) / 1000);
            if (transfer_pending) {
                int64_t wait_start = esp_timer_get_time();
                if (xSemaphoreTake(display_done, pdMS_TO_TICKS(1000)) != pdTRUE) {
                    ESP_LOGE(TAG, "display transfer timed out; restarting");
                    esp_restart();
                }
                wait_time_ms += (uint32_t)((esp_timer_get_time() - wait_start) / 1000);
            }
            ESP_ERROR_CHECK(
                esp_lcd_panel_draw_bitmap(panel, 0, y, PIN_WIDTH, y + STRIP_ROWS, strip));
            transfer_pending = true;
        }
        if (transfer_pending) {
            int64_t wait_start = esp_timer_get_time();
            if (xSemaphoreTake(display_done, pdMS_TO_TICKS(1000)) != pdTRUE) {
                ESP_LOGE(TAG, "display transfer timed out; restarting");
                esp_restart();
            }
            wait_time_ms += (uint32_t)((esp_timer_get_time() - wait_start) / 1000);
        }
        work_time_ms += (uint32_t)((esp_timer_get_time() - draw_start) / 1000);
        vTaskDelayUntil(&frame_tick, pdMS_TO_TICKS(FRAME_MS));
        if (++frame_count == 100) {
            int64_t elapsed_us = esp_timer_get_time() - report_start_us;
            ESP_LOGI(TAG,
                     "fps=%lu work=%lu ms render=%lu ms wait=%lu ms heap=%lu imu=%lu/100 "
                     "view=(%.2f,%.2f) roll=%.1f",
                     (unsigned long)(100000000LL / elapsed_us),
                     (unsigned long)(work_time_ms / frame_count),
                     (unsigned long)(render_time_ms / frame_count),
                     (unsigned long)(wait_time_ms / frame_count),
                     (unsigned long)esp_get_free_heap_size(), (unsigned long)motion.samples,
                     motion.filter.view_x, motion.filter.view_y, motion.filter.roll);
            frame_count = 0;
            work_time_ms = 0;
            render_time_ms = 0;
            wait_time_ms = 0;
            motion.samples = 0;
            report_start_us = esp_timer_get_time();
        }
    }
}
