#include "board.h"
#include "paint.h"
#include <string.h>
#include "driver/gpio.h"
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

enum { STRIP_ROWS = 40, TOUCH_INT = 41, IO_TIMEOUT_MS = 10 };
_Static_assert(DISPLAY_H % STRIP_ROWS == 0 && STRIP_ROWS % PET_SCALE == 0,
               "DMA strips must contain whole logical rows and cover the screen");
static const char *TAG = "board";
static esp_lcd_panel_handle_t panel;
static esp_lcd_panel_io_handle_t panel_io;
static SemaphoreHandle_t display_done;
static uint16_t *strips[2];
static i2c_master_bus_handle_t i2c;
static i2c_master_dev_handle_t touch, imu;
static bool touch_seen_at_boot;
static TaskHandle_t sensor_task_handle;
static QueueHandle_t events, status_queue;

// Waveshare's proven landscape RGB565 init for this exact 1.91-inch board.
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
                          esp_lcd_panel_io_event_data_t *data, void *context)
{
    (void)io; (void)data;
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)context, &wake);
    return wake == pdTRUE;
}
static void display_open(void)
{
    display_done = xSemaphoreCreateBinary();
    ESP_ERROR_CHECK(display_done ? ESP_OK : ESP_ERR_NO_MEM);
    spi_bus_config_t bus = SH8601_PANEL_BUS_QSPI_CONFIG(
        47, 18, 7, 48, 5, DISPLAY_W * STRIP_ROWS * sizeof(uint16_t) + 64);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    esp_lcd_panel_io_spi_config_t io = SH8601_PANEL_IO_QSPI_CONFIG(6, transfer_done, display_done);
    io.trans_queue_depth = 1;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io, &panel_io));
    sh8601_vendor_config_t vendor = {
        .init_cmds = panel_init, .init_cmds_size = sizeof(panel_init) / sizeof(panel_init[0]),
        .flags.use_qspi_interface = 1,
    };
    const esp_lcd_panel_dev_config_t config = {
        .reset_gpio_num = 17, .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16, .vendor_config = &vendor,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(panel_io, &config, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    for (int i = 0; i < 2; ++i) {
        strips[i] = heap_caps_malloc(DISPLAY_W * STRIP_ROWS * sizeof(uint16_t),
                                    MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        ESP_ERROR_CHECK(strips[i] ? ESP_OK : ESP_ERR_NO_MEM);
    }
    ESP_LOGI(TAG, "display=536x240 QSPI=40MHz DMA=2x42880 bytes");
}
static void wait_display(void)
{
    if (xSemaphoreTake(display_done, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "LCD DMA completion timeout");
        esp_restart();
    }
}
void board_present(const uint16_t pixels[PET_W * PET_H])
{
    bool pending = false;
    for (int y = 0; y < DISPLAY_H; y += STRIP_ROWS) {
        uint16_t *out = strips[(y / STRIP_ROWS) & 1];
        /* Fill the other strip while DMA owns the previous one. A buffer is
           never reused until its completion callback has been consumed. */
        bool ok = pet_expand_strip(pixels, y, STRIP_ROWS, out);
        ESP_ERROR_CHECK(ok ? ESP_OK : ESP_ERR_INVALID_ARG);
        if (pending) wait_display();
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, y, DISPLAY_W, y + STRIP_ROWS, out));
        pending = true;
    }
    if (pending) wait_display();
}
void board_brightness(unsigned level)
{
    static const uint8_t brightness[] = {0x45, 0x98, 0xd0};
    uint8_t b = brightness[level % 3];
    // SH8601 QSPI write-command prefix, as used by the vendor driver.
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(panel_io, (0x02 << 24) | (0x51 << 8), &b, 1));
}
static i2c_master_dev_handle_t add_device(uint8_t address)
{
    i2c_master_dev_handle_t dev;
    i2c_device_config_t cfg = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
                              .device_address = address, .scl_speed_hz = 400000};
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c, &cfg, &dev));
    return dev;
}
static esp_err_t read_reg(i2c_master_dev_handle_t dev, uint8_t reg, void *buf, size_t size)
{ return i2c_master_transmit_receive(dev, &reg, 1, buf, size, IO_TIMEOUT_MS); }
static esp_err_t write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val)
{
    uint8_t data[] = {reg, val};
    return i2c_master_transmit(dev, data, sizeof(data), IO_TIMEOUT_MS);
}
static void IRAM_ATTR touch_interrupt(void *arg)
{
    (void)arg;
    BaseType_t wake = pdFALSE;
    vTaskNotifyGiveFromISR(sensor_task_handle, &wake);
    if (wake) portYIELD_FROM_ISR();
}
static void send_event(board_status_t *s, input_event_t e)
{
    if (xQueueSend(events, &e, 0) != pdTRUE) s->dropped++;
    else s->events++;
}
static void sensor_task(void *context)
{
    (void)context;
    board_status_t s = {.touch_ok = touch_seen_at_boot, .imu_ok = imu != NULL};
    gesture_t gesture = {0};
    uint32_t last_imu = 0, last_touch_success = 0, last_log = 0, logged_errors = 0;
    while (true) {
        uint32_t interrupts = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));
        s.interrupts += interrupts;
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        /* FT3168 monitor mode cannot share I2C transactions with the IMU.
           A touch IRQ wakes it into active mode. Poll only during contact to
           catch held fingers/releases, then leave it alone until the next IRQ. */
        if (touch && (interrupts || s.touch_down)) {
            uint8_t report[5];
            if (read_reg(touch, 0x02, report, sizeof(report)) == ESP_OK) {
                s.touch_ok = true;
                s.touch_reads++;
                bool down; int x = s.touch_x, y = s.touch_y;
                if (touch_decode(report, &down, &x, &y)) {
                    input_event_t event;
                    if (down && !s.touch_down) s.touch_presses++;
                    if (!down && s.touch_down) s.touch_releases++;
                    if (down != s.touch_down)
                        ESP_LOGI(TAG, "CONTACT down=%d xy=(%d,%d)", down, x, y);
                    s.touch_down = down; s.touch_x = x; s.touch_y = y;
                    if (gesture_update(&gesture, down, x, y, now, &event)) send_event(&s, event);
                    last_touch_success = now;
                } else {
                    // Unsupported multi-contact/edge reports are not I2C faults.
                    // Retain a separate counter while rejecting their coordinates.
                    s.ignored_reports++;
                }
            } else { s.errors++; ESP_LOGW(TAG, "touch read failed"); }
            if (s.touch_down && now - last_touch_success > 150) {
                gesture = (gesture_t){0};
                s.touch_down = false; // Cancel a failed contact, never synthesize a tap.
            }
        }
        if (imu && now - last_imu >= 20) {
            uint8_t raw[6];
            if (read_reg(imu, 0x35, raw, sizeof(raw)) == ESP_OK) {
                for (int i = 0; i < 3; ++i)
                    s.accel[i] = (int16_t)((uint16_t)raw[i * 2] |
                                         ((uint16_t)raw[i * 2 + 1] << 8)) / 4096.0f;
                if (motion_update(&s.motion, s.accel, now - last_imu))
                    send_event(&s, (input_event_t){PET_SHAKE, 0, 0});
            } else { s.errors++; ESP_LOGW(TAG, "IMU read failed"); }
            last_imu = now;
        }
        if (s.errors != logged_errors && now - last_log > 10000) {
            ESP_LOGW(TAG, "sensor errors=%lu", (unsigned long)s.errors);
            last_log = now;
            logged_errors = s.errors;
        }
        xQueueOverwrite(status_queue, &s);
    }
}
void board_init(void)
{
    display_open();
    i2c_master_bus_config_t bus = {.i2c_port = I2C_NUM_0, .sda_io_num = 40,
                                  .scl_io_num = 39, .clk_source = I2C_CLK_SRC_DEFAULT,
                                  .glitch_ignore_cnt = 7, .flags.enable_internal_pullup = true};
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus, &i2c));
    /* A CPU/USB reset does not reset the FT3168. It may still be in monitor
       mode and NACK a probe. Always arm the wake IRQ on this touch board. */
    touch = add_device(0x38);
    if (i2c_master_probe(i2c, 0x38, 50) == ESP_OK && write_reg(touch, 0, 0) == ESP_OK) {
        touch_seen_at_boot = true;
        ESP_LOGI(TAG, "FT3168 ready address=0x38 interrupt=41");
    } else ESP_LOGI(TAG, "FT3168 awaiting touch wake interrupt on GPIO41");
    static const uint8_t addresses[] = {0x6b, 0x6a};
    for (unsigned i = 0; i < sizeof(addresses); ++i) {
        if (i2c_master_probe(i2c, addresses[i], 50) != ESP_OK) continue;
        i2c_master_dev_handle_t dev = add_device(addresses[i]);
        uint8_t id = 0;
        if (read_reg(dev, 0, &id, 1) == ESP_OK && id == 5) {
            // QMI8658: address auto-increment, 8 g range / 125 Hz; accel only.
            const uint8_t regs[][2] = {{8, 0}, {2, 0x60}, {3, 0x26}, {6, 0}, {8, 1}};
            bool ok = true;
            for (unsigned n = 0; n < sizeof(regs) / sizeof(regs[0]); ++n)
                if (write_reg(dev, regs[n][0], regs[n][1]) != ESP_OK) ok = false;
            if (ok) {
                imu = dev;
                vTaskDelay(pdMS_TO_TICKS(80));
                ESP_LOGI(TAG, "QMI8658 ready address=0x%02x id=%u accel=8g/125Hz", addresses[i], id);
                break;
            }
        }
        ESP_ERROR_CHECK(i2c_master_bus_rm_device(dev));
    }
    if (!imu) ESP_LOGW(TAG, "IMU absent; autonomous and touch behavior remain available");
    events = xQueueCreate(8, sizeof(input_event_t));
    status_queue = xQueueCreate(1, sizeof(board_status_t));
    ESP_ERROR_CHECK(events && status_queue ? ESP_OK : ESP_ERR_NO_MEM);
    gpio_config_t button = {.pin_bit_mask = 1ULL, .mode = GPIO_MODE_INPUT,
                            .pull_up_en = GPIO_PULLUP_ENABLE};
    ESP_ERROR_CHECK(gpio_config(&button));
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(sensor_task, "sensors", 4096, NULL, 4,
                                          &sensor_task_handle, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    if (touch) {
        gpio_config_t irq = {.pin_bit_mask = 1ULL << TOUCH_INT, .mode = GPIO_MODE_INPUT,
                             .pull_up_en = GPIO_PULLUP_ENABLE, .intr_type = GPIO_INTR_NEGEDGE};
        ESP_ERROR_CHECK(gpio_config(&irq));
        ESP_ERROR_CHECK(gpio_install_isr_service(0));
        ESP_ERROR_CHECK(gpio_isr_handler_add(TOUCH_INT, touch_interrupt, NULL));
    }
}
bool board_event(input_event_t *e) { return xQueueReceive(events, e, 0) == pdTRUE; }
void board_status(board_status_t *s) { xQueuePeek(status_queue, s, 0); }
bool board_button(void) { return gpio_get_level(0) == 0; }
