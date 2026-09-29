#include "board.h"
#include "app_switcher.h"
#include "paint.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "driver/usb_serial_jtag.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#if CONFIG_HEAP_TRACING_STANDALONE
#include "esp_heap_trace.h"
static heap_trace_record_t trace_records[32];
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "miso";
static uint16_t pixels[PET_W * PET_H];
static pet_t pet;
static bool capture_requested, color_bars;
static unsigned brightness = 1;
static int log_to_usb_host(const char *format, va_list args)
{
    // The default USB console can wait 50 ms for a host that is not present.
    // Battery/power-bank operation must never pay that delay in an animation.
    return usb_serial_jtag_is_connected() ? vprintf(format, args) : 0;
}
static void status_print(void)
{
    board_status_t s = {0}; board_status(&s);
    /* Integer diagnostics avoid newlib's persistent dtoa/Balloc caches. */
    ESP_LOGI(TAG, "STATUS state=%s x_milli=%d uptime=%llu visits=0x%lx interactions=%lu "
             "heap=%lu min_heap=%lu touch=%d imu=%d reads=%lu presses=%lu releases=%lu "
             "events=%lu errors=%lu dropped=%lu ignored_reports=%lu irq=%lu xy=(%d,%d) down=%d "
             "accel_mg=(%d,%d,%d) tilt_milli=%d samples=%lu stack=%u",
             pet_state_name(pet.state), (int)(pet.x * 1000), (unsigned long long)pet.uptime_ms,
             (unsigned long)pet.visited, (unsigned long)pet.interactions,
             (unsigned long)esp_get_free_heap_size(), (unsigned long)esp_get_minimum_free_heap_size(),
             s.touch_ok, s.imu_ok, (unsigned long)s.touch_reads, (unsigned long)s.touch_presses,
             (unsigned long)s.touch_releases, (unsigned long)s.events, (unsigned long)s.errors,
             (unsigned long)s.dropped, (unsigned long)s.ignored_reports,
             (unsigned long)s.interrupts, s.touch_x, s.touch_y,
             s.touch_down, (int)(s.accel[0] * 1000), (int)(s.accel[1] * 1000),
             (int)(s.accel[2] * 1000), (int)(s.motion.tilt * 1000),
             (unsigned long)s.motion.samples, (unsigned)uxTaskGetStackHighWaterMark(NULL));
}
static void command(const char *line)
{
    char name[20]; int x, y;
    if (!strcmp(line, "status")) status_print();
#if CONFIG_HEAP_TRACING_STANDALONE
    else if (!strcmp(line, "heap")) heap_trace_dump();
#endif
    else if (!strcmp(line, "capture")) capture_requested = true;
    else if (!strcmp(line, "bars")) color_bars = true;
    else if (!strcmp(line, "auto")) {
        color_bars = false; pet.manual_sleep = false; pet.food = false;
        pet_set_state(&pet, PET_IDLE, 2000);
    } else if (sscanf(line, "state %19s", name) == 1) {
        bool found = false;
        for (int i = 0; i < PET_STATE_COUNT; ++i)
            if (!strcmp(name, pet_state_name(i))) {
                pet.manual_sleep = false; pet.food = false;
                pet_set_state(&pet, i, 10000); found = true;
            }
        if (!found) { ESP_LOGW(TAG, "unknown state"); return; }
    } else if (sscanf(line, "tap %d %d", &x, &y) == 2 && x >= 0 && x < PET_W && y >= 0 && y < PET_H)
        pet_event(&pet, PET_TAP, x, y);
    else if (sscanf(line, "swipe %d %d", &x, &y) == 2 && x >= 0 && x < PET_W && y >= 0 && y < PET_H)
        pet_event(&pet, PET_SWIPE, x, y);
    else if (!strcmp(line, "hold")) pet_event(&pet, PET_HOLD, 67, 30);
    else if (!strcmp(line, "shake")) pet_event(&pet, PET_SHAKE, 0, 0);
    else if (sscanf(line, "brightness %d", &x) == 1 && x >= 0 && x < 3) {
        brightness = (unsigned)x; board_brightness(brightness);
    } else { ESP_LOGW(TAG, "commands: status capture bars auto state NAME tap X Y swipe X Y hold shake brightness 0..2"); return; }
    ESP_LOGI(TAG, "COMMAND %s -> %s", line, pet_state_name(pet.state));
}
static void console_poll(void)
{
    static char line[80]; static size_t used; static bool overflow;
    unsigned char data[64];
    int count = usb_serial_jtag_read_bytes(data, sizeof(data), 0);
    for (int i = 0; i < count; ++i) {
        if (data[i] == '\n' || data[i] == '\r') {
            if (used && !overflow) { line[used] = 0; command(line); }
            used = 0; overflow = false;
        } else if (data[i] >= 32 && data[i] < 127) {
            if (used + 1 < sizeof(line)) line[used++] = data[i];
            else overflow = true;
        }
    }
}
static void capture_frame(void)
{
    // Diagnostic snapshot of the exact logical frame last sent to the panel.
    printf("FRAME %d %d\n", PET_W, PET_H);
    for (int y = 0; y < PET_H; ++y) {
        char row[PET_W * 4 + 2];
        for (int x = 0; x < PET_W; ++x) snprintf(row + x * 4, 5, "%04x", pixels[y * PET_W + x]);
        row[PET_W * 4] = '\n'; row[PET_W * 4 + 1] = 0;
        fputs(row, stdout);
    }
    printf("END_FRAME\n");
}
void app_main(void)
{
    app_switcher_init();
    ESP_LOGI(TAG, "Miso %s reset=%d", esp_app_get_description()->version, esp_reset_reason());
    usb_serial_jtag_driver_config_t usb = {.rx_buffer_size = 512, .tx_buffer_size = 2048};
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb));
    esp_log_set_vprintf(log_to_usb_host);
    board_init();
#if CONFIG_HEAP_TRACING_STANDALONE
    ESP_ERROR_CHECK(heap_trace_init_standalone(trace_records, 32));
    ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));
#endif
    pet_init(&pet, esp_random());
    ESP_LOGI(TAG, "READY target=30fps logical=134x60 scale=4 brightness=1");
    uint32_t frames = 0, late = 0;
    uint32_t interval_min = UINT32_MAX, interval_max = 0, remainder_us = 0;
    int64_t work_sum = 0, work_max = 0, window = esp_timer_get_time();
    int64_t deadline = window, previous = window;
    bool button_was_down = false;
    uint32_t button_since = 0;
    while (true) {
        int64_t start = esp_timer_get_time();
        uint32_t interval = (uint32_t)(start - previous);
        if (interval > 1000) {
            if (interval < interval_min) interval_min = interval;
            if (interval > interval_max) interval_max = interval;
        }
        remainder_us += interval;
        uint32_t dt = remainder_us / 1000;
        remainder_us %= 1000;
        previous = start;
        board_status_t s = {0}; board_status(&s);
        pet_set_tilt(&pet, s.motion.tilt);
        input_event_t e;
        while (board_event(&e)) {
            pet_event(&pet, e.type, e.x, e.y);
            ESP_LOGI(TAG, "INPUT type=%d xy=(%d,%d) -> %s", e.type, e.x, e.y, pet_state_name(pet.state));
        }
        console_poll();
        bool down = board_button();
        uint32_t now_ms = (uint32_t)(start / 1000);
        if (down && !button_was_down) button_since = now_ms;
        if (!down && button_was_down && now_ms - button_since >= 40) {
            brightness = (brightness + 1) % 3;
            board_brightness(brightness);
            ESP_LOGI(TAG, "brightness=%u", brightness);
        }
        button_was_down = down;
        pet_step(&pet, dt);
        pet_paint(&pet, pixels);
        if (color_bars) {
            static const unsigned colors[] = {0xff0000, 0x00ff00, 0x0000ff, 0xffffff, 0x000000};
            for (int y = 0; y < PET_H; ++y)
                for (int x = 0; x < PET_W; ++x) pixels[y * PET_W + x] = pet_rgb(colors[x * 5 / PET_W]);
        }
        board_present(pixels);
        int64_t work = esp_timer_get_time() - start;
        work_sum += work;
        if (work > work_max) work_max = work;
        if (work > 33333) late++;
        if (capture_requested) {
            capture_frame(); capture_requested = false;
            // A requested serial dump intentionally pauses rendering. Start a
            // fresh scheduling window; never count it as normal display timing.
            deadline = previous = window = esp_timer_get_time();
            frames = late = 0; work_sum = work_max = 0;
            interval_min = UINT32_MAX; interval_max = remainder_us = 0;
        }
        if (++frames >= 300) {
            ESP_ERROR_CHECK(heap_caps_check_integrity_all(false) ? ESP_OK : ESP_FAIL);
            uint32_t fps_milli = (uint32_t)(frames * 1000000000LL / (esp_timer_get_time() - window));
            ESP_LOGI(TAG, "PERF fps=%lu.%03lu avg_us=%lld max_us=%lld late=%lu "
                     "period_us=%lu..%lu heap=%lu state=%s",
                     (unsigned long)(fps_milli / 1000), (unsigned long)(fps_milli % 1000),
                     (long long)(work_sum / frames), (long long)work_max,
                     (unsigned long)late, (unsigned long)interval_min, (unsigned long)interval_max,
                     (unsigned long)esp_get_free_heap_size(), pet_state_name(pet.state));
            status_print();
            window = esp_timer_get_time(); frames = late = 0; work_sum = work_max = 0;
            interval_min = UINT32_MAX; interval_max = 0;
        }
        deadline += 33333;
        int64_t remaining = deadline - esp_timer_get_time();
        if (remaining > 1000) vTaskDelay(pdMS_TO_TICKS((uint32_t)(remaining / 1000)));
        else if (remaining <= 0) { deadline = esp_timer_get_time(); vTaskDelay(1); }
    }
}
