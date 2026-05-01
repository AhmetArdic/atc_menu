/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file atc_menu.h
 * @brief UART debug menu framework — public API.
 *
 * Table-driven, allocation-free, transport-agnostic. The framework owns
 * neither the RX nor TX path: callers feed received bytes via
 * atc_menu_handle_key() and provide a TX callback via ::atc_menu_port_t.
 */

#ifndef ATC_MENU_H
#define ATC_MENU_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Health/state indicator for a row's value.
 *
 * Drives the color of the status field rendered at the end of each row.
 */
typedef enum {
    ATC_ST_NONE = 0, /**< Neutral, no color applied. */
    ATC_ST_OK,       /**< Nominal value (green). */
    ATC_ST_WARN,     /**< Warning threshold reached (yellow). */
    ATC_ST_ERR,      /**< Error / out of range (red). */
    ATC_ST_ON,       /**< Output asserted / pin high. */
    ATC_ST_OFF,      /**< Output deasserted / pin low. */
} atc_status_t;

/**
 * @brief Row type. Determines how the row is rendered and which callbacks
 *        are required.
 *
 * | Type           | read     | action     | Visual                    |
 * |----------------|----------|------------|---------------------------|
 * | ATC_ROW_GROUP  | unused   | unused     | `m  MPU9250 IMU`          |
 * | ATC_ROW_VALUE  | required | optional   | `t  Temp   45.3 C   OK`   |
 * | ATC_ROW_STATE  | required | required   | `L  LED    ON`            |
 * | ATC_ROW_ACTION | unused   | required   | `1  Flash CRC`            |
 *
 * Groups may carry a `key`; pressing it bulk-refreshes the group label
 * and every row up to the next group.
 */
typedef enum {
    ATC_ROW_GROUP,  /**< Section header. Optional key bulk-refreshes the span. */
    ATC_ROW_VALUE,  /**< Read-only scalar (sensor, voltage). */
    ATC_ROW_STATE,  /**< Two-state output (GPIO, fan). Toggled by action. */
    ATC_ROW_ACTION, /**< Command bound to a hotkey (test, reset). */
} atc_row_type_t;

/**
 * @brief Reader callback. Formats the current value into @p buf and reports
 *        the value's health via @p st.
 *
 * @param[out] buf  Destination buffer for the value string.
 * @param[in]  n    Size of @p buf in bytes (typically 16).
 * @param[out] st   Status to be displayed alongside the value.
 */
typedef void (*atc_read_fn_t)(char *buf, size_t n, atc_status_t *st);

/**
 * @brief Action callback. Invoked when the user presses the row's hotkey.
 */
typedef void (*atc_action_fn_t)(void);

/**
 * @brief A single menu row. Tables of these drive the entire UI.
 *
 * Build the table as a `static const atc_menu_item_t[]` using designated
 * initializers and pass it to atc_menu_init().
 */
typedef struct {
    atc_row_type_t  type;   /**< Row kind. See ::atc_row_type_t. */
    char            key;    /**< Hotkey (printable char). 0 for groups. */
    const char     *label;  /**< Group title or row label. */
    const char     *unit;   /**< Unit string ("V", "C"). Use "" if none. */
    atc_read_fn_t   read;   /**< Reader; required for VALUE/STATE. */
    atc_action_fn_t action; /**< Action; required for STATE/ACTION. */
} atc_menu_item_t;

/**
 * @brief Project metadata shown in the menu header.
 *
 * Each field is optional; pass NULL to omit. Define as `static const` so
 * it lives in flash. The pointed-to strings must outlive the menu.
 */
typedef struct {
    const char *project; /**< Application name. NULL falls back to "atc menu". */
    const char *version; /**< Version string ("1.0.0", "v2.1-rc1"). */
    const char *author;  /**< Author or team. */
    const char *build;   /**< Build date or commit hash. */
} atc_menu_info_t;

/**
 * @brief Port vtable. Decouples the framework from the underlying transport.
 *
 * Provide one instance per transport (UART, USB CDC, telnet, mock, ...).
 * Define as `static const` so it lives in flash.
 */
typedef struct {
    /**
     * @brief Transmit @p len bytes from @p buf. Must be blocking or buffered;
     *        the framework assumes the bytes are committed on return.
     */
    void (*write)(const char *buf, size_t len);

    /**
     * @brief Optional command-line handler. Invoked when the user submits
     *        a line entered after pressing ':'. Set to NULL to disable
     *        command mode.
     *
     * @param[in] line  Null-terminated command line, e.g. "pwm 2 50".
     */
    void (*cmd)(const char *line);
} atc_menu_port_t;

/**
 * @brief Initialize the menu with a row table and a transport port.
 *
 * Validates the table at startup and emits warnings via atc_menu_printf()
 * for duplicate hotkeys and missing required callbacks.
 *
 * @param[in] items  Pointer to the row table (must outlive the menu).
 * @param[in] count  Number of entries in @p items.
 * @param[in] port   Port vtable (must outlive the menu).
 */
void atc_menu_init(const atc_menu_item_t *items, size_t count,
                   const atc_menu_port_t *port);

/**
 * @brief Attach project metadata rendered in the header.
 *
 * Optional. Call once at startup; pass NULL to clear. The pointed-to
 * struct and its strings must outlive the menu.
 *
 * @param[in] info  Metadata to display, or NULL to fall back to defaults.
 */
void atc_menu_set_info(const atc_menu_info_t *info);

/**
 * @brief Repaint the entire menu in place (flicker-free).
 *
 * Uses cursor-home + line-erase rather than full clear, so terminals do
 * not flash. Call after any state change that should be reflected on
 * screen, or in response to a manual refresh.
 */
void atc_menu_render(void);

/**
 * @brief Feed a received character to the menu.
 *
 * Built-in keys: 'r' refreshes the whole menu, ':' enters command mode
 * (if port.cmd is set). Any other key is matched against the table:
 * STATE/ACTION rows run their action, then the matched row is repainted
 * in place (single-line update — much smaller than a full repaint).
 *
 * @param[in] k  Received character.
 */
void atc_menu_handle_key(char k);

/**
 * @brief Print a transient status line above the footer.
 *
 * @param[in] msg  Null-terminated message.
 */
void atc_menu_status(const char *msg);

/**
 * @brief printf-style output routed through the configured port.
 *
 * Output is bounded to ~128 bytes per call; longer strings are truncated.
 *
 * @param[in] fmt  printf format string.
 * @return Number of bytes written, or 0 if no port is configured.
 */
int atc_menu_printf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* ATC_MENU_H */
