#include "board.h"
#include "paint.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_sh8601.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

enum { STRIP_ROWS = 40, BOOT_BUTTON = 0 };
_Static_assert(DISPLAY_H % STRIP_ROWS == 0 && STRIP_ROWS % MECH_SCALE == 0,
               "DMA strips must hold complete logical rows");

static esp_lcd_panel_handle_t panel;
static SemaphoreHandle_t display_done;
static uint16_t *strips[2];

static const sh8601_lcd_init_cmd_t panel_init[] = {
    {0x11, NULL, 0, 120}, {0x36, (uint8_t[]){0xf0}, 1, 0},
    {0x3a, (uint8_t[]){0x55}, 1, 0},
    {0x2a, (uint8_t[]){0x00, 0x00, 0x02, 0x17}, 4, 0},
    {0x2b, (uint8_t[]){0x00, 0x00, 0x00, 0xef}, 4, 0},
    {0x51, (uint8_t[]){0x00}, 1, 10}, {0x29, NULL, 0, 10},
    {0x51, (uint8_t[]){0x98}, 1, 0},
};

static bool transfer_done(esp_lcd_panel_io_handle_t io,
                          esp_lcd_panel_io_event_data_t *data, void *context)
{
    (void)io; (void)data;
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)context, &wake);
    return wake == pdTRUE;
}

void board_init(void)
{
    display_done = xSemaphoreCreateBinary();
    ESP_ERROR_CHECK(display_done ? ESP_OK : ESP_ERR_NO_MEM);
    const spi_bus_config_t bus = SH8601_PANEL_BUS_QSPI_CONFIG(
        47, 18, 7, 48, 5, DISPLAY_W * STRIP_ROWS * sizeof(uint16_t) + 64);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(6, transfer_done, display_done);
    io_config.trans_queue_depth = 1;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io));
    sh8601_vendor_config_t vendor = {.init_cmds = panel_init,
        .init_cmds_size = sizeof(panel_init) / sizeof(panel_init[0]), .flags.use_qspi_interface = 1};
    const esp_lcd_panel_dev_config_t config = {.reset_gpio_num = 17,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, .bits_per_pixel = 16, .vendor_config = &vendor};
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io, &config, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    for (int i = 0; i < 2; ++i) {
        strips[i] = heap_caps_malloc(DISPLAY_W * STRIP_ROWS * sizeof(uint16_t),
                                     MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        ESP_ERROR_CHECK(strips[i] ? ESP_OK : ESP_ERR_NO_MEM);
    }
    const gpio_config_t button = {.pin_bit_mask = 1ULL << BOOT_BUTTON, .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&button));
}

void board_present(const uint16_t pixels[MECH_W * MECH_H])
{
    bool pending = false;
    for (int y = 0; y < DISPLAY_H; y += STRIP_ROWS) {
        uint16_t *out = strips[(y / STRIP_ROWS) & 1];
        ESP_ERROR_CHECK(mech_expand_strip(pixels, y, STRIP_ROWS, out) ? ESP_OK : ESP_ERR_INVALID_ARG);
        if (pending) ESP_ERROR_CHECK(xSemaphoreTake(display_done, pdMS_TO_TICKS(1000)) == pdTRUE
                                     ? ESP_OK : ESP_ERR_TIMEOUT);
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, y, DISPLAY_W, y + STRIP_ROWS, out));
        pending = true;
    }
    if (pending) ESP_ERROR_CHECK(xSemaphoreTake(display_done, pdMS_TO_TICKS(1000)) == pdTRUE
                                 ? ESP_OK : ESP_ERR_TIMEOUT);
}

bool board_button(void) { return gpio_get_level(BOOT_BUTTON) == 0; }
