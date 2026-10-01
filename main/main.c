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
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_lcd_sh8601.h"

enum {
    LCD_WIDTH = 536,
    LCD_HEIGHT = 240,
    STRIP_ROWS = 30,
    LCD_CS = 6,
    LCD_CLK = 47,
    LCD_D0 = 18,
    LCD_D1 = 7,
    LCD_D2 = 48,
    LCD_D3 = 5,
    LCD_RST = 17,
    BOOT_BUTTON = 0,
    APP_COUNT = 7,
};

static const char *TAG = "pin_launcher";
static SemaphoreHandle_t display_done;

typedef struct {
    const char *name;
    const char *hint;
    esp_partition_subtype_t subtype;
    uint16_t color;
} app_entry_t;

static const app_entry_t apps[APP_COUNT] = {
    {"CONWAY", "GAME OF LIFE", ESP_PARTITION_SUBTYPE_APP_OTA_0, 0x07ff},
    {"FLUID",  "MOTION WATER", ESP_PARTITION_SUBTYPE_APP_OTA_1, 0x04ff},
    {"MISO",   "WOODLAND PET", ESP_PARTITION_SUBTYPE_APP_OTA_2, 0xffe0},
    {"LUMEN",  "Starfield", ESP_PARTITION_SUBTYPE_APP_OTA_3, 0xf81f},
    {"MECH",   "BAY HANGAR", ESP_PARTITION_SUBTYPE_APP_OTA_4, 0xfbe0},
    {"DUNGEON", "SEED CRAWLER", ESP_PARTITION_SUBTYPE_APP_OTA_5, 0x07f0},
    {"3D MAZE", "CLASSIC WALK", ESP_PARTITION_SUBTYPE_APP_OTA_6, 0xfd20},
};

// Same Waveshare landscape RGB565/QSPI sequence used by the copied apps.
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

static bool transfer_done(esp_lcd_panel_io_handle_t io,
                          esp_lcd_panel_io_event_data_t *event, void *context)
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
        LCD_CLK, LCD_D0, LCD_D1, LCD_D2, LCD_D3,
        LCD_WIDTH * STRIP_ROWS * sizeof(uint16_t) + 64);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(
        LCD_CS, transfer_done, display_done);
    io_config.trans_queue_depth = 1;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,
                                              &io_config, &io));
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

static const uint8_t *glyph(char c)
{
    static const uint8_t space[] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t letters[][7] = {
        {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30},
        {14, 17, 16, 16, 16, 17, 14}, {30, 17, 17, 17, 17, 17, 30},
        {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
        {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17},
        {14, 4, 4, 4, 4, 4, 14}, {1, 1, 1, 1, 17, 17, 14},
        {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
        {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17},
        {14, 17, 17, 17, 17, 17, 14}, {30, 17, 17, 30, 16, 16, 16},
        {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
        {15, 16, 16, 14, 1, 1, 30}, {31, 4, 4, 4, 4, 4, 4},
        {17, 17, 17, 17, 17, 17, 14}, {17, 17, 17, 17, 17, 10, 4},
        {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
        {17, 17, 10, 4, 4, 4, 4}, {31, 1, 2, 4, 8, 16, 31},
    };
    static const uint8_t digits[][7] = {
        {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
        {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
        {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
        {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
        {14,17,17,14,17,17,14}, {14,17,17,15,1,1,14},
    };
    if (c >= 'A' && c <= 'Z') return letters[c - 'A'];
    if (c >= '0' && c <= '9') return digits[c - '0'];
    return space;
}

static void text(uint16_t *pixels, int y0, int x, int y, const char *s, uint16_t color, int scale)
{
    for (; *s; ++s, x += 6 * scale) {
        const uint8_t *rows = glyph(*s);
        for (int gy = 0; gy < 7; ++gy) for (int gx = 0; gx < 5; ++gx) {
            if (!(rows[gy] & (1 << (4 - gx)))) continue;
            for (int sy = 0; sy < scale; ++sy) for (int sx = 0; sx < scale; ++sx) {
                const int px = x + gx * scale + sx;
                const int py = y + gy * scale + sy;
                if (px >= 0 && px < LCD_WIDTH && py >= y0 && py < y0 + STRIP_ROWS)
                    pixels[(py - y0) * LCD_WIDTH + px] = color;
            }
        }
    }
}

static void draw_menu(esp_lcd_panel_handle_t panel, uint16_t *strip, unsigned selected)
{
    for (int y0 = 0; y0 < LCD_HEIGHT; y0 += STRIP_ROWS) {
        for (int y = 0; y < STRIP_ROWS; ++y) for (int x = 0; x < LCD_WIDTH; ++x)
            strip[y * LCD_WIDTH + x] = ((x / 16 + (y + y0) / 16) & 1) ? 0x0841 : 0x0000;
        text(strip, y0, 28, 9, "PIN LIBRARY", 0xffff, 2);
        text(strip, y0, 30, 32, "SHORT PRESS SELECT", 0x8410, 1);
        text(strip, y0, 30, 43, "HOLD BOOT TO START", 0x8410, 1);
        for (unsigned i = 0; i < APP_COUNT; ++i) {
            const int row = 57 + (int)i * 25;
            const uint16_t ink = i == selected ? apps[i].color : 0x7bef;
            if (i == selected) for (int yy = row - 3; yy < row + 20; ++yy)
                if (yy >= y0 && yy < y0 + STRIP_ROWS) for (int xx = 18; xx < 518; ++xx)
                    strip[(yy - y0) * LCD_WIDTH + xx] = 0x2104;
            text(strip, y0, 34, row, apps[i].name, ink, 2);
            text(strip, y0, 180, row + 5, apps[i].hint, 0xbdf7, 1);
        }
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, y0, LCD_WIDTH, y0 + STRIP_ROWS, strip));
        ESP_ERROR_CHECK(xSemaphoreTake(display_done, pdMS_TO_TICKS(1000)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT);
    }
}

static void wait_for_boot_release(void)
{
    while (gpio_get_level(BOOT_BUTTON) == 0) vTaskDelay(pdMS_TO_TICKS(20));
}

static void launch(unsigned selected, bool from_button)
{
    if (from_button) wait_for_boot_release();
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, apps[selected].subtype, NULL);
    ESP_ERROR_CHECK(partition ? ESP_OK : ESP_ERR_NOT_FOUND);
    ESP_LOGI(TAG, "Starting %s from %s at 0x%lx", apps[selected].name, partition->label,
             (unsigned long)partition->address);
    ESP_ERROR_CHECK(esp_ota_set_boot_partition(partition));
    vTaskDelay(pdMS_TO_TICKS(80));
    esp_restart();
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
    usb_serial_jtag_driver_config_t usb = {.tx_buffer_size = 256, .rx_buffer_size = 256};
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb));
    esp_lcd_panel_handle_t panel = display_open();
    uint16_t *strip = heap_caps_malloc(LCD_WIDTH * STRIP_ROWS * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_ERROR_CHECK(strip ? ESP_OK : ESP_ERR_NO_MEM);

    unsigned selected = 0;
    int64_t down_at = 0;
    bool was_down = false;
    ESP_LOGI(TAG, "READY: press 1-7 over USB or short-press BOOT to select; hold BOOT to start");
    while (true) {
        uint8_t byte;
        if (usb_serial_jtag_read_bytes(&byte, 1, 0) == 1 && byte >= '1' && byte <= '7') {
            selected = byte - '1';
            draw_menu(panel, strip, selected);
            launch(selected, false);
        }
        const bool down = gpio_get_level(BOOT_BUTTON) == 0;
        const int64_t now = esp_timer_get_time();
        if (down && !was_down) down_at = now;
        if (down && down_at && now - down_at >= 700000) launch(selected, true);
        if (!down && was_down && down_at && now - down_at >= 30000 && now - down_at < 700000) {
            selected = (selected + 1) % APP_COUNT;
            draw_menu(panel, strip, selected);
        }
        was_down = down;
        draw_menu(panel, strip, selected);
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}
