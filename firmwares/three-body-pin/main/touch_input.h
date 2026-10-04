// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <stdbool.h>

void touch_input_init(void);
bool touch_input_read(bool *down);
