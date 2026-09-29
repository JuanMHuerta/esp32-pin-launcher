#pragma once
#include "input.h"

typedef struct {
    motion_t motion;
    uint32_t touch_reads, touch_presses, touch_releases, events, errors, dropped;
    uint32_t ignored_reports;
    uint32_t interrupts;
    float accel[3];
    int touch_x, touch_y;
    bool touch_down, touch_ok, imu_ok;
} board_status_t;

void board_init(void);
void board_present(const uint16_t pixels[PET_W * PET_H]);
bool board_event(input_event_t *event);
void board_status(board_status_t *status);
void board_brightness(unsigned level);
bool board_button(void);
