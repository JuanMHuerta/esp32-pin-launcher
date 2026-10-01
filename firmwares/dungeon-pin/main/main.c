#include "app_switcher.h"
#include "board.h"
#include "dungeon.h"
#include "paint.h"

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "dungeon_seed";
static uint16_t pixels[DUNGEON_W * DUNGEON_H];

void app_main(void)
{
    app_switcher_init();
    ESP_LOGI(TAG, "DUNGEON//SEED %s reset=%d", esp_app_get_description()->version, esp_reset_reason());
    board_init();
    dungeon_t dungeon;
    dungeon_init(&dungeon, (uint32_t)esp_timer_get_time());
    ESP_LOGI(TAG, "READY 30fps: autonomous crawl; hold BOOT 1.5s to return to library");
    int64_t previous = esp_timer_get_time(), deadline = previous;
    while (true) {
        const int64_t now = esp_timer_get_time();
        uint32_t dt = (uint32_t)((now - previous) / 1000);
        if (dt > 100) dt = 100;
        previous = now;
        dungeon_step(&dungeon, dt);
        dungeon_paint(&dungeon, pixels);
        board_present(pixels);
        deadline += 33333;
        int64_t remain = deadline - esp_timer_get_time();
        if (remain > 1000) vTaskDelay(pdMS_TO_TICKS((uint32_t)(remain / 1000)));
        else if (remain <= 0) { deadline = esp_timer_get_time(); vTaskDelay(1); }
    }
}
