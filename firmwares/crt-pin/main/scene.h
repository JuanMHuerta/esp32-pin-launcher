// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "pin_gfx.h"
enum { CRT_COLS = 39, CRT_ROWS = 12, CRT_PROFILES = 6 };
enum { CRT_TEXT_X = 16, CRT_TEXT_Y = 6, CRT_ROW_PITCH = 9 };
enum { CRT_GREEN, CRT_AMBER, CRT_WARMUP_MS = 900 };
typedef enum { CRT_OUTPUT, CRT_COMMAND, CRT_WARN, CRT_PROGRESS, CRT_TITLE, CRT_TRACE } crt_kind_t;
typedef struct {
    const char *text;
    uint32_t duration;
    crt_kind_t kind;
} crt_event_t;
typedef struct {
    char text[CRT_COLS + 1];
    crt_kind_t kind;
} crt_line_t;
typedef struct {
    uint64_t elapsed_ms;
    uint32_t cycle_ms, event_ms, seed;
    unsigned profile, generation, event, visible, phosphor;
    bool cursor, booting;
    crt_line_t lines[CRT_ROWS];
} crt_t;
void crt_init(crt_t *s, unsigned profile, uint32_t seed);
void crt_next(crt_t *s, uint32_t seed);
void crt_step(crt_t *s, uint32_t ms);
const crt_event_t *crt_script(unsigned profile, unsigned *count);
const crt_event_t *crt_boot_script(unsigned *count);
void crt_paint(const crt_t *s, uint16_t *pixels);
