// SPDX-License-Identifier: GPL-3.0-only
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
    ESP_LOGI(TAG, "DUNGEON//SEED %s reset=%d", esp_app_get_description()->version,
             esp_reset_reason());
    board_init();
    static dungeon_t dungeon;
    dungeon_init(&dungeon, (uint32_t)esp_timer_get_time());
    ESP_LOGI(TAG, "READY 30fps: autonomous crawl; hold BOOT 1.5s to return to library");
    int64_t previous = esp_timer_get_time(), deadline = previous;
    uint32_t report_ms = 0, frames = 0;
    int64_t paint_total = 0;
    while (true) {
        const int64_t now = esp_timer_get_time();
        uint32_t dt = (uint32_t)((now - previous) / 1000);
        if (dt > 100) {
            dt = 100;
        }
        previous = now;
        dungeon_step(&dungeon, dt);
        int64_t paint_start = esp_timer_get_time();
        dungeon_paint(&dungeon, pixels);
        paint_total += esp_timer_get_time() - paint_start;
        frames++;
        board_present(pixels);
        if (dungeon.uptime_ms - report_ms >= 10000u) {
            const dungeon_node_t *node = &dungeon.nodes[dungeon.current_node];
            ESP_LOGI(TAG,
                     "CRAWL run=%lu state=%s biome=%s node=%u hp=%u mp=%u foe=%u/%u hits=%u "
                     "fps=%lu paint=%lldus",
                     (unsigned long)dungeon.run, dungeon_state_name(dungeon.state),
                     dungeon_biome_name(dungeon.biome), dungeon.current_node, dungeon.hp,
                     dungeon.mp, node->hp, node->max_hp, node->hits,
                     (unsigned long)(frames * 1000u / (dungeon.uptime_ms - report_ms)),
                     (long long)(paint_total / frames));
            report_ms = dungeon.uptime_ms;
            frames = 0;
            paint_total = 0;
        }
        deadline += 33333;
        int64_t remain = deadline - esp_timer_get_time();
        if (remain > 1000) {
            vTaskDelay(pdMS_TO_TICKS((uint32_t)(remain / 1000)));
        } else if (remain <= 0) {
            deadline = esp_timer_get_time();
            vTaskDelay(1);
        }
    }
}
