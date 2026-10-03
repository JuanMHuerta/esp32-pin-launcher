// SPDX-License-Identifier: GPL-3.0-only
#include <stdbool.h>
#include <stdint.h>

#include "app_switcher.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_lcd_sh8601.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "scene.h"

enum {
    LCD_WIDTH = WAYFARER_WIDTH,
    LCD_HEIGHT = WAYFARER_HEIGHT,
    STRIP_ROWS = 30,
    FRAME_US = 33333,
    LCD_CS = 6,
    LCD_CLK = 47,
    LCD_D0 = 18,
    LCD_D1 = 7,
    LCD_D2 = 48,
    LCD_D3 = 5,
    LCD_RST = 17,
    BOOT_BUTTON = 0,
};

_Static_assert(LCD_HEIGHT % STRIP_ROWS == 0, "DMA strips must cover the panel evenly");

static const char *TAG = "wayfarer";
static SemaphoreHandle_t display_done;
static uint16_t *strips[2];
static uint32_t render_cpu_us;
static uint32_t swap_cpu_us;

extern const uint8_t cockpit_rgb565_start[] asm("_binary_cockpit_rgb565_start");
extern const uint8_t cockpit_rgb565_end[] asm("_binary_cockpit_rgb565_end");

/* Waveshare SKU 28596 landscape RGB565/QSPI initialization sequence. */
static const sh8601_lcd_init_cmd_t panel_init[] = {
    {0x11, NULL, 0, 120},
    {0x36, (uint8_t[]){0xf0}, 1, 0},
    {0x3a, (uint8_t[]){0x55}, 1, 0},
    {0x2a, (uint8_t[]){0x00, 0x00, 0x02, 0x17}, 4, 0},
    {0x2b, (uint8_t[]){0x00, 0x00, 0x00, 0xef}, 4, 0},
    {0x51, (uint8_t[]){0x00}, 1, 10},
    {0x29, NULL, 0, 10},
    {0x51, (uint8_t[]){0xa8}, 1, 0},
};

static bool transfer_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *event,
                          void *context)
{
    (void)io;
    (void)event;
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)context, &wake);
    return wake == pdTRUE;
}

static esp_lcd_panel_handle_t display_open(void)
{
    display_done = xSemaphoreCreateCounting(2, 0);
    ESP_ERROR_CHECK(display_done ? ESP_OK : ESP_ERR_NO_MEM);
    const spi_bus_config_t bus = SH8601_PANEL_BUS_QSPI_CONFIG(
        LCD_CLK, LCD_D0, LCD_D1, LCD_D2, LCD_D3, LCD_WIDTH * STRIP_ROWS * sizeof(uint16_t) + 64);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_config =
        SH8601_PANEL_IO_QSPI_CONFIG(LCD_CS, transfer_done, display_done);
    io_config.trans_queue_depth = 2;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io));
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
    for (int i = 0; i < 2; ++i) {
        strips[i] = heap_caps_malloc(LCD_WIDTH * STRIP_ROWS * sizeof(uint16_t),
                                     MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        ESP_ERROR_CHECK(strips[i] ? ESP_OK : ESP_ERR_NO_MEM);
    }
    return panel;
}

static void present_frame(esp_lcd_panel_handle_t panel, wayfarer_scene_t *scene, uint32_t now_ms)
{
    const uint16_t *background = (const uint16_t *)cockpit_rgb565_start;
    unsigned pending = 0;
    render_cpu_us = swap_cpu_us = 0;
    for (int y = 0, strip_index = 0; y < LCD_HEIGHT; y += STRIP_ROWS, ++strip_index) {
        if (pending == 2) {
            ESP_ERROR_CHECK(xSemaphoreTake(display_done, pdMS_TO_TICKS(1000)) == pdTRUE
                                ? ESP_OK
                                : ESP_ERR_TIMEOUT);
            --pending;
        }
        uint16_t *out = strips[strip_index & 1];
        int64_t render_at = esp_timer_get_time();
        wayfarer_scene_render_strip(scene, background, now_ms, y, STRIP_ROWS, out);
        int64_t swap_at = esp_timer_get_time();
        render_cpu_us += (uint32_t)(swap_at - render_at);
        /* SH8601 QSPI expects the RGB565 byte swap exactly once. */
        for (int i = 0; i < LCD_WIDTH * STRIP_ROWS; ++i) {
            out[i] = __builtin_bswap16(out[i]);
        }
        swap_cpu_us += (uint32_t)(esp_timer_get_time() - swap_at);
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, y, LCD_WIDTH, y + STRIP_ROWS, out));
        ++pending;
    }
    while (pending) {
        ESP_ERROR_CHECK(
            xSemaphoreTake(display_done, pdMS_TO_TICKS(1000)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT);
        --pending;
    }
}

void app_main(void)
{
    const size_t background_size = (size_t)(cockpit_rgb565_end - cockpit_rgb565_start);
    ESP_ERROR_CHECK(background_size == LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t)
                        ? ESP_OK
                        : ESP_ERR_INVALID_SIZE);
    app_switcher_init();
    esp_lcd_panel_handle_t panel = display_open();

    uint32_t seed;
    esp_fill_random(&seed, sizeof(seed));
    wayfarer_scene_t scene;
    wayfarer_scene_init(&scene, seed);
    ESP_LOGI(TAG, "WAYFARER PIXEL %s reset=%d", esp_app_get_description()->version,
             esp_reset_reason());
    ESP_LOGI(TAG, "READY 30fps; tap BOOT for a nav ping; hold BOOT 1.5s for library");

    bool was_down = false;
    int64_t down_at = 0;
    int64_t deadline = esp_timer_get_time();
    int64_t perf_window = deadline;
    uint32_t frames = 0;
    uint32_t max_render_us = 0;
    uint8_t previous_event = WAYFARER_EVENT_NONE;
    while (true) {
        int64_t now = esp_timer_get_time();
        bool down = gpio_get_level(BOOT_BUTTON) == 0;
        bool tap = false;
        if (down && !was_down) {
            down_at = now;
        }
        if (!down && was_down && down_at) {
            int64_t held = now - down_at;
            tap = held >= 35000 && held < 1200000;
            down_at = 0;
        }
        was_down = down;

        uint32_t frame_ms = (uint32_t)(now / 1000);
        wayfarer_scene_update(&scene, frame_ms, tap);
        if (scene.event != previous_event) {
            ESP_LOGI(TAG, "EVENT %u variant=%u direction=%d", scene.event, scene.variant,
                     scene.direction);
            previous_event = scene.event;
        }
        present_frame(panel, &scene, frame_ms);
        uint32_t render_us = (uint32_t)(esp_timer_get_time() - now);
        if (render_us > max_render_us) {
            max_render_us = render_us;
        }
        ++frames;

        deadline += FRAME_US;
        int64_t remaining = deadline - esp_timer_get_time();
        if (remaining > 1000) {
            vTaskDelay(pdMS_TO_TICKS((uint32_t)(remaining / 1000)));
        } else if (remaining <= 0) {
            deadline = esp_timer_get_time();
            vTaskDelay(1);
        }
        int64_t after = esp_timer_get_time();
        if (after - perf_window >= 5000000) {
            uint32_t elapsed = (uint32_t)(after - perf_window);
            uint32_t fps_tenths = (uint32_t)((uint64_t)frames * 10000000U / elapsed);
            ESP_LOGI(TAG, "PERF %u.%u fps, max render %u.%ums", fps_tenths / 10, fps_tenths % 10,
                     max_render_us / 1000, (max_render_us % 1000) / 100);
            ESP_LOGI(TAG, "CPU paint=%u.%ums swap=%u.%ums", render_cpu_us / 1000,
                     (render_cpu_us % 1000) / 100, swap_cpu_us / 1000, (swap_cpu_us % 1000) / 100);
            perf_window = after;
            frames = 0;
            max_render_us = 0;
        }
    }
}
