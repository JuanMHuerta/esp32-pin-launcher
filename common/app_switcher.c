// SPDX-License-Identifier: GPL-3.0-only
#include "app_switcher.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

enum {
    BOOT_BUTTON = 0,
    RETURN_HOLD_US = 1500000,
    DEMO_DURATION_US = 5 * 60 * 1000000,
};
static const char *TAG = "app_switcher";
static const char *DEMO_NVS_NAMESPACE = "pin_demo";
static const char *DEMO_NVS_KEY = "active";

static bool demo_mode_read(void)
{
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Cannot initialize NVS for demo mode: %s", esp_err_to_name(err));
        return false;
    }
    nvs_handle_t handle;
    err = nvs_open(DEMO_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return false;
    }
    uint8_t active = 0;
    err = nvs_get_u8(handle, DEMO_NVS_KEY, &active);
    nvs_close(handle);
    return err == ESP_OK && active != 0;
}

static void demo_mode_clear(void)
{
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Cannot initialize NVS to stop demo mode: %s", esp_err_to_name(err));
        return;
    }
    nvs_handle_t handle;
    err = nvs_open(DEMO_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, DEMO_NVS_KEY, 0);
        if (err == ESP_OK) {
            err = nvs_commit(handle);
        }
        nvs_close(handle);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Cannot stop demo mode: %s", esp_err_to_name(err));
    }
}

static bool demo_launch_next(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running || running->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_0 ||
        running->subtype > ESP_PARTITION_SUBTYPE_APP_OTA_8) {
        ESP_LOGE(TAG, "Cannot find current app for demo rotation");
        demo_mode_clear();
        return false;
    }
    const unsigned count = esp_ota_get_app_partition_count();
    const unsigned current = running->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_0;
    if (!count || current >= count) {
        ESP_LOGE(TAG, "Invalid installed app count for demo rotation");
        demo_mode_clear();
        return false;
    }
    const unsigned next = (current + 1) % count;
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0 + next, NULL);
    if (!partition || esp_ota_set_boot_partition(partition) != ESP_OK) {
        ESP_LOGE(TAG, "Cannot select next demo app");
        demo_mode_clear();
        return false;
    }
    ESP_LOGI(TAG, "Demo: %s finished; starting next app", running->label);
    vTaskDelay(pdMS_TO_TICKS(80));
    esp_restart();
    return true;
}

static void return_to_launcher(void *context)
{
    (void)context;
    int64_t pressed_at = 0;
    bool was_down = false;
    bool demo_mode = demo_mode_read();
    const int64_t demo_started_at = esp_timer_get_time();
    if (demo_mode) {
        ESP_LOGI(TAG, "Demo mode active; this app will run for five minutes");
    }
    while (true) {
        const bool down = gpio_get_level(BOOT_BUTTON) == 0;
        const int64_t now = esp_timer_get_time();
        if (down && !was_down) {
            pressed_at = now;
        }
        if (down && pressed_at && now - pressed_at >= RETURN_HOLD_US) {
            if (demo_mode) {
                demo_mode_clear();
            }
            const esp_partition_t *factory = esp_partition_find_first(
                ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "factory");
            if (factory && esp_ota_set_boot_partition(factory) == ESP_OK) {
                ESP_LOGW(TAG, "Long BOOT press: returning to launcher");
                // GPIO0 is a boot strapping pin; do not reset while it is low.
                while (gpio_get_level(BOOT_BUTTON) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
                vTaskDelay(pdMS_TO_TICKS(80));
                esp_restart();
            }
            ESP_LOGE(TAG, "Cannot select factory launcher");
            pressed_at = now;
        }
        if (demo_mode && !down && now - demo_started_at >= DEMO_DURATION_US &&
            !demo_launch_next()) {
            demo_mode = false;
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
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
}
