// SPDX-License-Identifier: GPL-3.0-only
#include "app_switcher.h"
#include "board.h"
#include "maze.h"
#include "paint.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static uint16_t pixels[MAZE_W * MAZE_H];
static const char *TAG = "maze_pin";

void app_main(void)
{
    app_switcher_init();
    board_init();
    maze_t maze;
    maze_init(&maze, (uint32_t)esp_timer_get_time() ^ (uint32_t)esp_random());
    ESP_LOGI(TAG, "3D MAZE %s: autoplay; hold BOOT 1.5s for launcher",
             esp_app_get_description()->version);
    int64_t previous = esp_timer_get_time(), deadline = previous;
    int64_t stats_at = previous;
    uint64_t paint_total = 0, present_total = 0;
    unsigned frames = 0;
    uint32_t generation = maze.generation;
    while (true) {
        int64_t now = esp_timer_get_time();
        uint32_t dt = (uint32_t)((now - previous) / 1000);
        previous = now;
        maze_step(&maze, dt);
        if (maze.generation != generation) {
            generation = maze.generation;
            ESP_LOGI(TAG, "Generated maze %lu, exit=(%d,%d)", (unsigned long)generation,
                     maze.exit_x, maze.exit_y);
        }
        int64_t paint_at = esp_timer_get_time();
        maze_paint(&maze, pixels);
        int64_t present_at = esp_timer_get_time();
        board_present(pixels);
        int64_t finished_at = esp_timer_get_time();
        paint_total += (uint64_t)(present_at - paint_at);
        present_total += (uint64_t)(finished_at - present_at);
        ++frames;
        if (finished_at - stats_at >= 5000000) {
            ESP_LOGI(
                TAG, "PERF fps=%lu paint=%luus display=%luus stack=%lu",
                (unsigned long)((uint64_t)frames * 1000000u / (uint64_t)(finished_at - stats_at)),
                (unsigned long)(paint_total / frames), (unsigned long)(present_total / frames),
                (unsigned long)uxTaskGetStackHighWaterMark(NULL));
            stats_at = finished_at;
            paint_total = present_total = 0;
            frames = 0;
        }
        deadline += 25000;
        int64_t remain = deadline - esp_timer_get_time();
        if (remain > 1000) {
            vTaskDelay(pdMS_TO_TICKS((uint32_t)(remain / 1000)));
        } else if (remain <= 0) {
            deadline = esp_timer_get_time();
            vTaskDelay(1);
        }
    }
}
