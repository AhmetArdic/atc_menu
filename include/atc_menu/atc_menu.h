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
#include <stdint.h>

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
 * @brief Row type. Drives rendering and which fields are required.
 *
 * | Type            | read     | action   | Visual                          |
 * |-----------------|----------|----------|---------------------------------|
 * | ATC_ROW_GROUP   | unused   | unused   | `   MPU9250 IMU`                |
 * | ATC_ROW_VALUE   | required | optional | `t  Temp   45.3 C   OK`         |
 * | ATC_ROW_STATE   | required | required | `L  LED    ON`                  |
 * | ATC_ROW_ACTION  | unused   | required | `1  Flash CRC`                  |
 * | ATC_ROW_SUBMENU | unused   | unused   | `i  ❯ MPU9250 IMU`              |
 * | ATC_ROW_BAR     | required | unused   | `b  Battery ▕████▎  ▏ 78 % OK`  |
 * | ATC_ROW_CHOICE  | optional | unused   | `p  Mode    ❮ NORMAL ❯      OK` |
 * | ATC_ROW_INPUT   | required | unused   | `d  PWM Duty       50 %     OK` |
 *
 * SUBMENU additionally requires `submenu`. CHOICE requires `choices`
 * and `choice_idx`. INPUT requires `input_commit`. See the per-field
 * comments on ::atc_menu_item for usage details and the optional hooks
 * (`choice_commit` for browse-then-commit, etc.).
 */
typedef enum {
    ATC_ROW_GROUP,   /**< Section header. Optional key bulk-refreshes the span. */
    ATC_ROW_VALUE,   /**< Read-only scalar (sensor, voltage). */
    ATC_ROW_STATE,   /**< Two-state output (GPIO, fan). Toggled by action. */
    ATC_ROW_ACTION,  /**< Command bound to a hotkey (test, reset). */
    ATC_ROW_SUBMENU, /**< Drills into another items table. */
    ATC_ROW_BAR,     /**< Horizontal level bar (0..100 %). */
    ATC_ROW_CHOICE,  /**< N-state cycle (e.g., ECO/NORMAL/TURBO). */
    ATC_ROW_INPUT,   /**< Runtime parameter entry (int/float/hex/string). */
} atc_row_type_t;

/**
 * @brief Data type accepted by an INPUT row's editor.
 */
typedef enum {
    ATC_INPUT_INT,   /**< Signed decimal integer; bounded by input_min/input_max. */
    ATC_INPUT_FLOAT, /**< Decimal fraction; bounded by input_min/input_max. */
    ATC_INPUT_HEX,   /**< Hex digits, optional 0x prefix; bounded by input_min/input_max. */
    ATC_INPUT_STR,   /**< Printable ASCII string; bounded by buffer length. */
} atc_input_type_t;

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
 * @brief Commit callback for INPUT rows. Invoked when the user submits an
 *        edited value via Enter and the framework's range/format check passes.
 *
 * @param[in] s  Null-terminated string the user entered (e.g., "50", "0x1F").
 * @return true to accept and exit input mode; false to keep the editor open.
 */
typedef bool (*atc_input_fn_t)(const char *s);

struct atc_menu_table; /* fwd decl */

/**
 * @brief A single menu row. Tables of these drive the entire UI.
 *
 * Build the table as a `static const atc_menu_item_t[]` using designated
 * initializers, wrap it in an ::atc_menu_table_t, and pass it to
 * atc_menu_init().
 */
typedef struct atc_menu_item {
    atc_row_type_t  type;   /**< Row kind. See ::atc_row_type_t. */
    char            key;    /**< Hotkey (printable char). 0 for groups. */
    const char     *label;  /**< Group title or row label. */
    const char     *unit;   /**< Unit string ("V", "C"). Use "" if none. */
    atc_read_fn_t   read;   /**< Reader; required for VALUE/STATE/BAR/INPUT. */
    atc_action_fn_t action; /**< Action; required for STATE/ACTION. */
    const struct atc_menu_table *submenu; /**< Sub-menu table. Required for SUBMENU. */

    /* CHOICE-specific (used when type == ATC_ROW_CHOICE; ignored otherwise). */
    const char    **choices;       /**< Array of choice strings (each <= 6 chars). */
    uint8_t         choice_count;  /**< Number of entries in @ref choices. */
    uint8_t        *choice_idx;    /**< Pointer to current selection (mutable, app-owned). */
    atc_action_fn_t choice_commit; /**< Optional. NULL: key cycles+writes immediately.
                                        Non-NULL: key opens edit mode; Enter commits
                                        and fires this callback, Esc reverts. */

    /* INPUT-specific (used when type == ATC_ROW_INPUT; ignored otherwise). */
    atc_input_type_t input_type;   /**< Editor data type. */
    int32_t          input_min;    /**< Lower bound (INT/HEX). 0 if unused. */
    int32_t          input_max;    /**< Upper bound (INT/HEX). 0 if unused. */
    atc_input_fn_t   input_commit; /**< Validated-buffer commit callback. */
} atc_menu_item_t;

/**
 * @brief A menu screen: row table plus optional static notes.
 *
 * One table per screen. Notes (if any) are rendered as dim text inside
 * the box, between the last row and the bottom border, separated by a
 * dotted line.
 *
 * Define as `static const`; the struct and its referenced arrays must
 * outlive the menu.
 */
typedef struct atc_menu_table {
    const atc_menu_item_t *items;      /**< Row table. */
    size_t                 count;      /**< Number of rows in @ref items. */
    const char *const     *notes;      /**< Optional. Array of note strings. */
    size_t                 note_count; /**< 0 to omit the notes block. */
} atc_menu_table_t;

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
 * @brief Initialize the menu with a root table and a transport port.
 *
 * Validates the table at startup and emits warnings to the configured
 * port for duplicate hotkeys and missing required callbacks.
 *
 * @param[in] table  Root menu table (must outlive the menu).
 * @param[in] port   Port vtable (must outlive the menu).
 */
void atc_menu_init(const atc_menu_table_t *table,
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
 * Built-in keys: `r` refresh, `b` back, `?` path, `:` command mode.
 * Any other key is matched against the active table; per-row behavior
 * is documented on ::atc_row_type_t.
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

#ifdef __cplusplus
}
#endif

#endif /* ATC_MENU_H */
