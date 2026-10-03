// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Mount the board's microSD card and make the USB file protocol available. */
void sdcard_init(void);

/* Feed one byte from USB Serial/JTAG to the file protocol.
 * Returns true when the protocol consumed the byte.  Bytes which are not the
 * start of a protocol frame are left for the launcher's one-byte commands.
 */
bool sdcard_protocol_feed_byte(uint8_t byte);

/* The launcher can pause display refresh while a file operation is active. */
bool sdcard_protocol_busy(void);
