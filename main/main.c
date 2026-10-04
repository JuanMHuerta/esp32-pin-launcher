// SPDX-License-Identifier: GPL-3.0-only
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/usb_serial_jtag.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_lcd_sh8601.h"
#include "menu_render.h"
#include "sdcard.h"

enum {
    LCD_WIDTH = LAUNCHER_MENU_WIDTH,
    LCD_HEIGHT = LAUNCHER_MENU_HEIGHT,
    STRIP_ROWS = LAUNCHER_MENU_STRIP_ROWS,
    LCD_CS = 6,
    LCD_CLK = 47,
    LCD_D0 = 18,
    LCD_D1 = 7,
    LCD_D2 = 48,
    LCD_D3 = 5,
    LCD_RST = 17,
    BOOT_BUTTON = 0,
    APP_COUNT = LAUNCHER_MENU_APP_COUNT,
};

static const char *DEMO_NVS_NAMESPACE = "pin_demo";
static const char *DEMO_NVS_KEY = "active";

static const char *TAG = "pin_launcher";
static SemaphoreHandle_t display_done;

static launcher_menu_app_t apps[APP_COUNT];
static unsigned app_count;

static void discover_apps(void)
{
    for (unsigned i = 0; i < APP_COUNT; ++i) {
        const esp_partition_t *partition = esp_partition_find_first(
            ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, launcher_catalog[i].label);
        if (partition && partition->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_0 &&
            partition->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_MAX) {
            apps[app_count++] = launcher_catalog[i];
        }
    }
    ESP_LOGI(TAG, "Found %u installed apps", app_count);
}

// Waveshare landscape initialization: RGB565 over QSPI.
static const sh8601_lcd_init_cmd_t panel_init[] = {
    {0x11, NULL, 0, 120},
    {0x36, (uint8_t[]){0xf0}, 1, 0},
    {0x3a, (uint8_t[]){0x55}, 1, 0},
    {0x2a, (uint8_t[]){0x00, 0x00, 0x02, 0x17}, 4, 0},
    {0x2b, (uint8_t[]){0x00, 0x00, 0x00, 0xef}, 4, 0},
    {0x51, (uint8_t[]){0x00}, 1, 10},
    {0x29, NULL, 0, 10},
    {0x51, (uint8_t[]){0x98}, 1, 0},
};

static bool transfer_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *event,
                          void *context)
{
    (void)io;
    (void)event;
    BaseType_t woke = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)context, &woke);
    return woke == pdTRUE;
}

static esp_lcd_panel_handle_t display_open(void)
{
    display_done = xSemaphoreCreateBinary();
    ESP_ERROR_CHECK(display_done ? ESP_OK : ESP_ERR_NO_MEM);
    const spi_bus_config_t bus = SH8601_PANEL_BUS_QSPI_CONFIG(
        LCD_CLK, LCD_D0, LCD_D1, LCD_D2, LCD_D3, LCD_WIDTH * STRIP_ROWS * sizeof(uint16_t) + 64);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_config =
        SH8601_PANEL_IO_QSPI_CONFIG(LCD_CS, transfer_done, display_done);
    io_config.trans_queue_depth = 1;
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
    return panel;
}

static void draw_menu(esp_lcd_panel_handle_t panel, uint16_t *strip, unsigned selected)
{
    for (int y0 = 0; y0 < LCD_HEIGHT; y0 += STRIP_ROWS) {
        launcher_menu_render_strip(strip, y0, selected, apps, app_count);
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, y0, LCD_WIDTH, y0 + STRIP_ROWS, strip));
        ESP_ERROR_CHECK(
            xSemaphoreTake(display_done, pdMS_TO_TICKS(1000)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT);
    }
}

static void wait_for_boot_release(void)
{
    while (gpio_get_level(BOOT_BUTTON) == 0) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void launch(unsigned selected, bool from_button, bool demo_mode)
{
    if (selected >= app_count) {
        return;
    }
    if (from_button) {
        wait_for_boot_release();
    }
    // A normal launch must clear a demo selection left in NVS by an earlier boot.
    ESP_ERROR_CHECK(nvs_flash_init());
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(DEMO_NVS_NAMESPACE, NVS_READWRITE, &handle));
    ESP_ERROR_CHECK(nvs_set_u8(handle, DEMO_NVS_KEY, demo_mode ? 1 : 0));
    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, apps[selected].label);
    ESP_ERROR_CHECK(partition ? ESP_OK : ESP_ERR_NOT_FOUND);
    ESP_LOGI(TAG, "Starting %s from %s at 0x%lx", apps[selected].name, partition->label,
             (unsigned long)partition->address);
    ESP_ERROR_CHECK(esp_ota_set_boot_partition(partition));
    vTaskDelay(pdMS_TO_TICKS(80));
    esp_restart();
}

static void launch_demo(void)
{
    ESP_LOGI(TAG, "Starting demo mode: each app runs for five minutes");
    launch(0, true, true);
}

void app_main(void)
{
    const gpio_config_t button = {
        .pin_bit_mask = 1ULL << BOOT_BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&button));
    usb_serial_jtag_driver_config_t usb = {.tx_buffer_size = 8192, .rx_buffer_size = 8192};
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb));
    discover_apps();
    sdcard_init();
    esp_lcd_panel_handle_t panel = display_open();
    uint16_t *strip = heap_caps_malloc(LCD_WIDTH * STRIP_ROWS * sizeof(uint16_t),
                                       MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_ERROR_CHECK(strip ? ESP_OK : ESP_ERR_NO_MEM);

    unsigned selected = 0;
    int64_t down_at = 0;
    bool was_down = false;
    ESP_LOGI(TAG, "READY: press 1-9, D for demo, or short-press BOOT; "
                  "hold BOOT to start");
    while (true) {
        uint8_t input[256];
        int input_count = usb_serial_jtag_read_bytes(input, sizeof(input), 0);
        for (int i = 0; i < input_count; ++i) {
            if (sdcard_protocol_feed_byte(input[i])) {
                continue;
            }
            if (input[i] >= '1' && input[i] <= '9') {
                for (unsigned j = 0; j < app_count; ++j) {
                    if (apps[j].shortcut == input[i]) {
                        selected = j;
                        draw_menu(panel, strip, selected);
                        launch(selected, false, false);
                    }
                }
            } else if (app_count && (input[i] == 'd' || input[i] == 'D')) {
                selected = app_count;
                draw_menu(panel, strip, selected);
                launch_demo();
            }
        }
        const bool down = gpio_get_level(BOOT_BUTTON) == 0;
        const int64_t now = esp_timer_get_time();
        if (down && !was_down) {
            down_at = now;
        }
        if (app_count && down && down_at && now - down_at >= 700000) {
            if (selected == app_count) {
                launch_demo();
            } else {
                launch(selected, true, false);
            }
        }
        if (!down && was_down && down_at && now - down_at >= 30000 && now - down_at < 700000) {
            selected = app_count ? (selected + 1) % (app_count + 1) : 0;
            draw_menu(panel, strip, selected);
        }
        was_down = down;
        if (!sdcard_protocol_busy()) {
            draw_menu(panel, strip, selected);
        }
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}
