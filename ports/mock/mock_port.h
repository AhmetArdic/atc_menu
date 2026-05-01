/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file mock_port.h
 * @brief In-memory port for unit tests.
 *
 * Captures every TX byte and command invocation in process-local buffers
 * so tests can assert on the rendered output without an actual UART.
 */

#ifndef MOCK_PORT_H
#define MOCK_PORT_H

#include "atc_menu/atc_menu.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Port instance to pass to atc_menu_init(). */
extern const atc_menu_port_t mock_port;

/** Drop captured TX bytes and command history. */
void mock_reset(void);

/** Null-terminated TX capture buffer. Valid until next mock_reset(). */
const char *mock_buffer(void);

/** Bytes currently held in the capture buffer. */
size_t mock_len(void);

/** Most recent command line submitted via ':'. Empty if none. */
const char *mock_last_cmd(void);

/** Number of command callbacks invoked since last mock_reset(). */
size_t mock_cmd_calls(void);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_PORT_H */
