// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define FLOW_HOT IRAM_ATTR
#else
#define FLOW_HOT
#endif
