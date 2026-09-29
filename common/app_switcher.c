#include "app_switcher.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

enum { BOOT_BUTTON = 0, RETURN_HOLD_US = 1500000 };
static const char *TAG = "app_switcher";

static void return_to_launcher(void *context)
{
    (void)context;
    int64_t pressed_at = 0;
    bool was_down = false;
    while (true) {
        const bool down = gpio_get_level(BOOT_BUTTON) == 0;
        const int64_t now = esp_timer_get_time();
        if (down && !was_down) pressed_at = now;
        if (down && pressed_at && now - pressed_at >= RETURN_HOLD_US) {
            const esp_partition_t *factory = esp_partition_find_first(
                ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "factory");
            if (factory && esp_ota_set_boot_partition(factory) == ESP_OK) {
                ESP_LOGW(TAG, "Long BOOT press: returning to launcher");
                // GPIO0 is a boot strapping pin; do not reset while it is low.
                while (gpio_get_level(BOOT_BUTTON) == 0) vTaskDelay(pdMS_TO_TICKS(20));
                vTaskDelay(pdMS_TO_TICKS(80));
                esp_restart();
            }
            ESP_LOGE(TAG, "Cannot select factory launcher");
            pressed_at = now;
        }
        was_down = down;
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}

void app_switcher_init(void)
{
    const gpio_config_t button = {
        .pin_bit_mask = 1ULL << BOOT_BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&button));
    ESP_ERROR_CHECK(xTaskCreate(return_to_launcher, "pin_menu", 3072, NULL, 1, NULL) == pdPASS
                        ? ESP_OK : ESP_ERR_NO_MEM);
}
