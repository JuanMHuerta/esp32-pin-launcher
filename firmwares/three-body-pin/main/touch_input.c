// SPDX-License-Identifier: GPL-3.0-only
#include "touch_input.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

enum {
    TOUCH_SDA = 40,
    TOUCH_SCL = 39,
    TOUCH_INT = 41,
    TOUCH_ADDRESS = 0x38,
    TOUCH_REPORT_MS = 20,
    TOUCH_LOST_MS = 150,
    TOUCH_WAKE_RETRY_MS = 750,
};

static const char *TAG = "three_body_touch";
static i2c_master_dev_handle_t touch_device;
static QueueHandle_t touch_events;
static TaskHandle_t touch_task_handle;

static esp_err_t read_registers(uint8_t reg, uint8_t *data, size_t count)
{
    return i2c_master_transmit_receive(touch_device, &reg, 1, data, count, 20);
}

static void publish(bool down)
{
    xQueueOverwrite(touch_events, &down);
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
    int64_t last_success_us = 0;
    int64_t retry_until_us = 0;
    while (true) {
        int64_t now_us = esp_timer_get_time();
        if (was_down || now_us < retry_until_us) {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(TOUCH_REPORT_MS));
        } else {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            retry_until_us = esp_timer_get_time() + TOUCH_WAKE_RETRY_MS * 1000LL;
        }

        uint8_t reg = 0x02, report[5];
        esp_err_t result = read_registers(reg, report, sizeof(report));
        now_us = esp_timer_get_time();
        if (result != ESP_OK) {
            if (!retry_until_us) {
                retry_until_us = now_us + TOUCH_WAKE_RETRY_MS * 1000LL;
            }
            if (was_down && now_us - last_success_us >= TOUCH_LOST_MS * 1000LL) {
                publish(false);
                was_down = false;
                retry_until_us = now_us + TOUCH_WAKE_RETRY_MS * 1000LL;
            }
            continue;
        }
        last_success_us = now_us;

        unsigned count = report[0] & 0x0f;
        unsigned event = report[1] >> 6;
        if (!count || event == 1) {
            if (was_down) {
                publish(false);
                was_down = false;
                retry_until_us = 0;
            }
            continue;
        }
        if (count > 1 || (event != 0 && event != 2)) {
            continue;
        }

        /* FT3168 reports Y first and X second; validate the board's full landscape range. */
        unsigned raw_y = ((report[1] & 0x0f) << 8) | report[2];
        unsigned raw_x = ((report[3] & 0x0f) << 8) | report[4];
        if (raw_x < 536 && raw_y < 240) {
            was_down = true;
            retry_until_us = 0;
            publish(true);
        }
    }
}

void touch_input_init(void)
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = TOUCH_SDA,
        .scl_io_num = TOUCH_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus));
    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TOUCH_ADDRESS,
        .scl_speed_hz = 300000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &device_config, &touch_device));

    touch_events = xQueueCreate(1, sizeof(bool));
    ESP_ERROR_CHECK(touch_events ? ESP_OK : ESP_ERR_NO_MEM);
    const gpio_config_t gpio = {
        .pin_bit_mask = 1ULL << TOUCH_INT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&gpio));
    ESP_ERROR_CHECK(
        xTaskCreate(touch_task, "three_body_touch", 3072, NULL, 5, &touch_task_handle) == pdPASS
            ? ESP_OK
            : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(TOUCH_INT, touch_interrupt, NULL));
    ESP_LOGI(TAG, "FT3168 interrupt input ready; hold the screen to toggle diagnostics");
}

bool touch_input_read(bool *down)
{
    return xQueueReceive(touch_events, down, 0) == pdTRUE;
}
