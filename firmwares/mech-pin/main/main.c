#include "app_switcher.h"
#include "board.h"
#include "mech.h"
#include "paint.h"

#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "mech_bay_07";
static uint16_t pixels[MECH_W * MECH_H];

void app_main(void)
{
    app_switcher_init();
    ESP_LOGI(TAG, "MECH//BAY-07 %s reset=%d", esp_app_get_description()->version, esp_reset_reason());
    board_init();
    mech_t mech;
    mech_init(&mech);
    ESP_LOGI(TAG, "READY 30fps: short BOOT launches; hold BOOT 1.5s returns to library");
    bool was_down = false;
    uint32_t down_at = 0;
    int64_t previous = esp_timer_get_time(), deadline = previous;
    while (true) {
        const int64_t now = esp_timer_get_time();
        uint32_t dt = (uint32_t)((now - previous) / 1000);
        if (dt > 100) dt = 100;
        previous = now;
        bool down = board_button();
        if (down && !was_down) down_at = (uint32_t)(now / 1000);
        if (!down && was_down) {
            uint32_t held = (uint32_t)(now / 1000) - down_at;
            if (held >= 35 && held < 1200) {
                mech_activate(&mech);
                ESP_LOGI(TAG, "ACTIVATE state=%s interactions=%lu", mech_state_name(mech.state),
                         (unsigned long)mech.interactions);
            }
        }
        was_down = down;
        mech_step(&mech, dt);
        mech_paint(&mech, pixels);
        board_present(pixels);
        deadline += 33333;
        int64_t remain = deadline - esp_timer_get_time();
        if (remain > 1000) vTaskDelay(pdMS_TO_TICKS((uint32_t)(remain / 1000)));
        else if (remain <= 0) { deadline = esp_timer_get_time(); vTaskDelay(1); }
    }
}
