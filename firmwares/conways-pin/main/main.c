// SPDX-License-Identifier: GPL-3.0-only
#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_lcd_sh8601.h"
#include "app_switcher.h"
#include "life.h"
#include "palette.h"
#include "touch_map.h"

#define LCD_WIDTH LIFE_DISPLAY_WIDTH
#define LCD_HEIGHT LIFE_DISPLAY_HEIGHT
#define STRIP_HEIGHT 60
#define FRAME_MS 100
#define COUNTER_TOP 3
#define COUNTER_HEIGHT 12
#define COUNTER_RIGHT_MARGIN 4

#define LCD_HOST SPI2_HOST
#define LCD_CS 6
#define LCD_CLK 47
#define LCD_D0 18
#define LCD_D1 7
#define LCD_D2 48
#define LCD_D3 5
#define LCD_RST 17

#define TOUCH_I2C_PORT I2C_NUM_0
#define TOUCH_SCL 39
#define TOUCH_SDA 40
#define TOUCH_INT 41
#define TOUCH_I2C_ADDRESS 0x38
#define TOUCH_I2C_HZ 400000
#define TOUCH_I2C_TIMEOUT_MS 20
#define TOUCH_NEW_PRESS_GAP_US 300000
#define FT3168_REG_TD_STATUS 0x02

_Static_assert((LCD_WIDTH - LIFE_WIDTH * LIFE_CELL_PIXELS) == 2 * LIFE_OFFSET_X,
               "Horizontal cell grid must be centered on the display");
_Static_assert(LCD_HEIGHT == LIFE_HEIGHT * LIFE_CELL_PIXELS,
               "Life cells must fill the display height exactly");
_Static_assert(LCD_HEIGHT % STRIP_HEIGHT == 0, "Strips must cover the whole display");
_Static_assert(STRIP_HEIGHT % LIFE_CELL_PIXELS == 0,
               "Each DMA strip must contain complete Life rows");
_Static_assert(COUNTER_TOP + COUNTER_HEIGHT <= STRIP_HEIGHT,
               "The generation counter must fit in the first DMA strip");

static const char *TAG = "conways_pin";
static uint8_t grid_a[LIFE_CELLS];
static uint8_t grid_b[LIFE_CELLS];
static uint16_t palette[LIFE_MAX_AGE + 1][LIFE_CELL_PIXELS * LIFE_CELL_PIXELS];
static SemaphoreHandle_t display_done;
static i2c_master_bus_handle_t touch_bus;
static i2c_master_dev_handle_t touch_controller;
static QueueHandle_t touch_events;
static TaskHandle_t touch_task_handle;

typedef struct {
    uint16_t raw_x;
    uint16_t raw_y;
    uint16_t cell_x;
    uint16_t cell_y;
} touch_event_t;

typedef enum {
    TOUCH_REPORT_DOWN,
    TOUCH_REPORT_CONTACT,
    TOUCH_REPORT_UP,
} touch_report_t;

// Official Waveshare 1.91-inch ESP-IDF example panel sequence, RGB565.
static const sh8601_lcd_init_cmd_t panel_init[] = {
    {0x11, NULL, 0, 120},
    {0x36, (uint8_t[]){0xF0}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x02, 0x17}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x00, 0xEF}, 4, 0},
    {0x51, (uint8_t[]){0x00}, 1, 10},
    {0x29, NULL, 0, 10},
    {0x51, (uint8_t[]){0xB0}, 1, 0},
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
        LCD_CLK, LCD_D0, LCD_D1, LCD_D2, LCD_D3, LCD_WIDTH * STRIP_HEIGHT * sizeof(uint16_t) + 64);
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

static touch_event_t map_touch(uint16_t raw_x, uint16_t raw_y)
{
    const touch_cell_t cell = touch_cell_from_raw(raw_x, raw_y);
    return (touch_event_t){
        .raw_x = raw_x,
        .raw_y = raw_y,
        .cell_x = cell.x,
        .cell_y = cell.y,
    };
}

static esp_err_t ft3168_read_touch(touch_event_t *event, touch_report_t *touch_report)
{
    const uint8_t start = FT3168_REG_TD_STATUS;
    uint8_t report[5];
    const esp_err_t result = i2c_master_transmit_receive(
        touch_controller, &start, sizeof(start), report, sizeof(report), TOUCH_I2C_TIMEOUT_MS);
    if (result != ESP_OK) {
        return result;
    }
    const int touches = report[0] & 0x0f;
    const int touch_event = report[1] >> 6;
    // The touch count is authoritative; some reports have no event flag even
    // while a finger is down.
    *touch_report = !touches || touch_event == 1 ? TOUCH_REPORT_UP
                    : touch_event == 0           ? TOUCH_REPORT_DOWN
                                                 : TOUCH_REPORT_CONTACT;
    if (*touch_report == TOUCH_REPORT_UP) {
        return ESP_OK;
    }
    // The board's FT3168 reports Y in the first coordinate pair and X in
    // the second, as in Waveshare's touch_bsp.c for this exact display.
    const uint16_t raw_y = ((uint16_t)(report[1] & 0x0f) << 8) | report[2];
    const uint16_t raw_x = ((uint16_t)(report[3] & 0x0f) << 8) | report[4];
    *event = map_touch(raw_x, raw_y);
    return ESP_OK;
}

static void IRAM_ATTR touch_interrupt(void *context)
{
    (void)context;
    BaseType_t wake = pdFALSE;
    vTaskNotifyGiveFromISR(touch_task_handle, &wake);
    if (wake == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void touch_task(void *context)
{
    (void)context;
    bool was_down = false;
    int64_t last_report_us = 0;
    while (true) {
        // The interrupt handles quick taps; a periodic status read also catches
        // reports and releases when the controller does not pulse INT.
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));
        touch_event_t event;
        touch_report_t report;
        const esp_err_t result = ft3168_read_touch(&event, &report);
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "FT3168 interrupt, but report read failed: %s", esp_err_to_name(result));
            continue;
        }
        if (report == TOUCH_REPORT_UP) {
            was_down = false;
            continue;
        }
        const int64_t now = esp_timer_get_time();
        // A long gap before a new DOWN also recovers a missed lift report.
        // Repeated reports from a moving or held finger remain one stamp.
        if (!was_down ||
            (report == TOUCH_REPORT_DOWN && now - last_report_us > TOUCH_NEW_PRESS_GAP_US)) {
            xQueueSend(touch_events, &event, 0);
        }
        was_down = true;
        last_report_us = now;
    }
}

static void open_touch(void)
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = TOUCH_I2C_PORT,
        .sda_io_num = TOUCH_SDA,
        .scl_io_num = TOUCH_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &touch_bus));
    const gpio_config_t interrupt_config = {
        .pin_bit_mask = 1ULL << TOUCH_INT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&interrupt_config));
    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TOUCH_I2C_ADDRESS,
        .scl_speed_hz = TOUCH_I2C_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(touch_bus, &device_config, &touch_controller));
    const uint8_t normal_mode[] = {0x00, 0x00};
    const esp_err_t wake_result = i2c_master_transmit(touch_controller, normal_mode,
                                                      sizeof(normal_mode), TOUCH_I2C_TIMEOUT_MS);
    if (wake_result != ESP_OK) {
        // The FT3168 can still be waking after an ESP32-only reset. Its interrupt
        // stays armed, and a first touch restores normal reporting.
        ESP_LOGW(TAG, "FT3168 normal-mode write deferred: %s", esp_err_to_name(wake_result));
    }
    touch_events = xQueueCreate(4, sizeof(touch_event_t));
    ESP_ERROR_CHECK(touch_events ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(touch_task, "ft3168_touch", 3072, NULL, 5, &touch_task_handle) ==
                            pdPASS
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(TOUCH_INT, touch_interrupt, NULL));
    ESP_LOGI(TAG, "FT3168 touch ready on I2C GPIO%d/%d, interrupt GPIO%d", TOUCH_SDA, TOUCH_SCL,
             TOUCH_INT);
}

static const uint8_t counter_digits[10][7] = {
    {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e}, {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e},
    {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f}, {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e},
    {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02}, {0x1f, 0x10, 0x1e, 0x01, 0x01, 0x11, 0x0e},
    {0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e}, {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e}, {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x1c},
};

static const uint8_t *counter_glyph(char character)
{
    static const uint8_t g[7] = {0x0f, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0f};
    if (character >= '0' && character <= '9') {
        return counter_digits[character - '0'];
    }
    return g;
}

static void draw_generation_counter(uint16_t *pixels, uint32_t generation)
{
    char reversed[10];
    int digit_count = 0;
    do {
        reversed[digit_count++] = '0' + generation % 10;
        generation /= 10;
    } while (generation != 0);

    char label[11] = {'G'};
    for (int i = 0; i < digit_count; ++i) {
        label[1 + i] = reversed[digit_count - 1 - i];
    }
    const int characters = 1 + digit_count;
    const int advance = 7;
    const int width = characters * advance + 4;
    const int left = LCD_WIDTH - COUNTER_RIGHT_MARGIN - width;

    // A small black backing preserves legibility over active Life cells.
    for (int y = COUNTER_TOP; y < COUNTER_TOP + COUNTER_HEIGHT; ++y) {
        uint16_t *row = pixels + y * LCD_WIDTH + left;
        for (int x = 0; x < width; ++x) {
            row[x] = 0;
        }
    }

    for (int character = 0; character < characters; ++character) {
        const uint8_t *glyph = counter_glyph(label[character]);
        const uint16_t color = __builtin_bswap16(0xffff);
        const int x0 = left + 2 + character * advance;
        const int y0 = COUNTER_TOP + 2;
        // Stretch 5x7 glyphs to 6x8 by repeating only the middle row/column.
        for (int gy = 0; gy < 8; ++gy) {
            const int source_y = (gy * 6 + 3) / 7;
            for (int gx = 0; gx < 6; ++gx) {
                const int source_x = (gx * 4 + 2) / 5;
                if (glyph[source_y] & (1 << (4 - source_x))) {
                    pixels[(y0 + gy) * LCD_WIDTH + x0 + gx] = color;
                }
            }
        }
    }
}

static int show_frame(esp_lcd_panel_handle_t panel, const life_t *life, uint16_t *pixels,
                      bool force_full_refresh)
{
    int sent = 0;
    for (int y0 = 0; y0 < LCD_HEIGHT; y0 += STRIP_HEIGHT) {
        // The counter changes every generation, even if this strip's cells do not.
        bool changed = force_full_refresh || y0 == 0;
        for (int gy = y0 / LIFE_CELL_PIXELS;
             !changed && gy < (y0 + STRIP_HEIGHT) / LIFE_CELL_PIXELS; ++gy) {
            for (int x = 0; x < LIFE_WIDTH; ++x) {
                const int index = gy * LIFE_WIDTH + x;
                if (life->current[index] != life->next[index]) {
                    changed = true;
                    break;
                }
            }
        }
        if (!changed) {
            continue;
        }
        for (int cell_row = 0; cell_row < STRIP_HEIGHT / LIFE_CELL_PIXELS; ++cell_row) {
            const int gy = y0 / LIFE_CELL_PIXELS + cell_row;
            for (int pixel_row = 0; pixel_row < LIFE_CELL_PIXELS; ++pixel_row) {
                uint16_t *row = pixels + (cell_row * LIFE_CELL_PIXELS + pixel_row) * LCD_WIDTH;
                row[0] = 0;
                row[LCD_WIDTH - 1] = 0;
                for (int x = 0; x < LIFE_WIDTH; ++x) {
                    uint8_t state = life->current[gy * LIFE_WIDTH + x];
                    for (int pixel = 0; pixel < LIFE_CELL_PIXELS; ++pixel) {
                        // ESP-IDF panel IO sends byte order as stored in RAM.
                        row[LIFE_OFFSET_X + LIFE_CELL_PIXELS * x + pixel] =
                            __builtin_bswap16(palette[state][pixel_row * LIFE_CELL_PIXELS + pixel]);
                    }
                }
            }
        }
        if (y0 == 0) {
            draw_generation_counter(pixels, life->generation);
        }
        ESP_ERROR_CHECK(
            esp_lcd_panel_draw_bitmap(panel, 0, y0, LCD_WIDTH, y0 + STRIP_HEIGHT, pixels));
        if (xSemaphoreTake(display_done, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "LCD transfer timed out; restarting display");
            esp_restart();
        }
        sent++;
    }
    return sent;
}

void app_main(void)
{
    app_switcher_init();
    esp_lcd_panel_handle_t panel = open_display();
    uint16_t *strip = heap_caps_malloc(LCD_WIDTH * STRIP_HEIGHT * sizeof(uint16_t),
                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    ESP_ERROR_CHECK(strip ? ESP_OK : ESP_ERR_NO_MEM);
    palette_build(palette);
    open_touch();

    life_t life;
    life_init(&life, grid_a, grid_b, esp_random() ^ (uint32_t)esp_timer_get_time());
    life_seed(&life);
    ESP_LOGI(TAG, "Life on %dx%d cells, %dx%d AMOLED pixels, %d ms per generation", LIFE_WIDTH,
             LIFE_HEIGHT, LIFE_CELL_PIXELS, LIFE_CELL_PIXELS, FRAME_MS);

    TickType_t frame_start = xTaskGetTickCount();
    int64_t window_start_us = esp_timer_get_time();
    uint32_t work_ms_total = 0;
    uint32_t simulation_ms_total = 0;
    uint32_t display_ms_total = 0;
    uint32_t strips_total = 0;
    uint32_t frames_in_window = 0;
    bool force_full_refresh = true;
    while (true) {
        int64_t work_start_us = esp_timer_get_time();
        life_stats_t stats = life_step(&life);
        touch_event_t touch;
        while (xQueueReceive(touch_events, &touch, 0) == pdTRUE) {
            const int added = life_stamp(&life, touch.cell_x, touch.cell_y);
            ESP_LOGI(TAG, "touch raw=(%u,%u) cell=(%u,%u) added=%d", touch.raw_x, touch.raw_y,
                     touch.cell_x, touch.cell_y, added);
        }
        const bool still = life.quiet_generations >= LIFE_STILL_GENERATIONS;
        if (life.generation % LIFE_EDGE_INTERVAL == 0 || still) {
            int gliders = life_inject_edge(&life);
            if (gliders) {
                life.quiet_generations = 0;
                ESP_LOGI(TAG, "generation=%lu edge=%lu gliders=%d reason=%s",
                         (unsigned long)life.generation, (unsigned long)((life.launches - 1) & 3),
                         gliders, still ? "still" : "interval");
            }
        }
        int64_t display_start_us = esp_timer_get_time();
        strips_total += show_frame(panel, &life, strip, force_full_refresh);
        force_full_refresh = false;
        int64_t work_end_us = esp_timer_get_time();
        simulation_ms_total += (uint32_t)((display_start_us - work_start_us) / 1000);
        display_ms_total += (uint32_t)((work_end_us - display_start_us) / 1000);
        work_ms_total += (uint32_t)((work_end_us - work_start_us) / 1000);
        vTaskDelayUntil(&frame_start, pdMS_TO_TICKS(FRAME_MS));
        if (++frames_in_window == 100) {
            int64_t elapsed_us = esp_timer_get_time() - window_start_us;
            ESP_LOGI(TAG,
                     "generation=%lu living=%lu births=%lu avg_work=%lu ms sim=%lu ms display=%lu "
                     "ms strips=%lu/%d fps=%lu",
                     (unsigned long)life.generation, (unsigned long)stats.living,
                     (unsigned long)stats.births, (unsigned long)(work_ms_total / frames_in_window),
                     (unsigned long)(simulation_ms_total / frames_in_window),
                     (unsigned long)(display_ms_total / frames_in_window),
                     (unsigned long)(strips_total / frames_in_window), LCD_HEIGHT / STRIP_HEIGHT,
                     (unsigned long)(100000000LL / elapsed_us));
            frames_in_window = 0;
            work_ms_total = 0;
            simulation_ms_total = 0;
            display_ms_total = 0;
            strips_total = 0;
            window_start_us = esp_timer_get_time();
        }
    }
}
