// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
#include <stdint.h>
enum { PIN_W = 268, PIN_H = 120, PIN_DISPLAY_W = 536, PIN_DISPLAY_H = 240 };
uint16_t pin_rgb(int r, int g, int b);
uint16_t pin_dim(uint16_t c, int amount);
void pin_rect(uint16_t *p, int x, int y, int w, int h, uint16_t c);
void pin_line(uint16_t *p, int x0, int y0, int x1, int y1, uint16_t c);
void pin_text(uint16_t *p, int x, int y, const char *s, uint16_t c);
bool pin_expand_strip(const uint16_t *p, int y, int rows, uint16_t *out);
