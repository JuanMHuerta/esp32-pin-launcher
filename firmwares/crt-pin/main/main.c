// SPDX-License-Identifier: GPL-3.0-only
#include "scene.h"
#include "pin_board.h"
#include "app_switcher.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static uint16_t pixels[PIN_W * PIN_H];
static crt_t scene;
void app_main(void)
{
    app_switcher_init();
    pin_board_init();
    crt_init(&scene, 0, esp_random());
    ESP_LOGI("crt_pin", "READY: simulated CRT console; BOOT next sequence; hold BOOT for launcher");
    int64_t previous = esp_timer_get_time(), stats = previous, pressed = 0;
    bool was_down = pin_board_button();
    unsigned frames = 0;
    while (true) {
        int64_t now = esp_timer_get_time();
        bool down = pin_board_button();
        if (down && !was_down) {
            pressed = now;
        }
        if (!down && was_down && pressed && now - pressed >= 30000 && now - pressed < 700000) {
            crt_next(&scene, esp_random());
        }
        was_down = down;
        crt_step(&scene, (uint32_t)((now - previous) / 1000));
        previous = now;
        crt_paint(&scene, pixels);
        pin_board_present(pixels);
        frames++;
        if (now - stats >= 5000000) {
            ESP_LOGI(
                "crt_pin",
                "RUN profile=%u event=%u cycle=%u phosphor=%s boot=%u fps=%.1f heap=%lu stack=%lu",
                scene.profile, scene.event, scene.generation,
                scene.phosphor == CRT_AMBER ? "amber" : "green", (unsigned)scene.booting,
                frames * 1000000.0 / (now - stats), (unsigned long)esp_get_free_heap_size(),
                (unsigned long)uxTaskGetStackHighWaterMark(NULL));
            frames = 0;
            stats = now;
        }
        int64_t rest = 25000 - (esp_timer_get_time() - now);
        vTaskDelay(rest > 1000 ? pdMS_TO_TICKS((uint32_t)(rest / 1000)) : 1);
    }
}
