/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Layout configuration.
 *
 * Four knobs you may want to tune. Pass via CMake (-DLABEL_W=32) or
 * edit this file. Everything else derives from these; in particular,
 * INPUT_BUF (max chars an INPUT row accepts) scales with VALUE_W and
 * UNIT_W so a wider edit area automatically allows longer typed input.
 */

#ifndef ATC_LAYOUT_H
#define ATC_LAYOUT_H

#ifndef LABEL_W
#define LABEL_W    24   /* label column width (cols) */
#endif

#ifndef VALUE_W
#define VALUE_W    10   /* value column width (cols) — INPUT typing capacity scales with this */
#endif

#ifndef UNIT_W
#define UNIT_W      5   /* unit column width (cols) */
#endif

#ifndef NAV_DEPTH
#define NAV_DEPTH   4   /* max submenu nesting */
#endif

/* Fixed internal dimensions (rarely need tuning). */

#define KEY_W             1
#define STATUS_W          1
#define GAP_W             3
#define SUBMENU_PROMPT_W  2
#define HEADER_LINES      3

/* Derived row widths. */

#define INNER_W          (KEY_W + GAP_W + LABEL_W + GAP_W + VALUE_W + \
                          GAP_W + UNIT_W + GAP_W + STATUS_W)
#define GROUP_LABEL_W    (INNER_W - KEY_W - GAP_W)
#define SUBMENU_LABEL_W  (INNER_W - KEY_W - GAP_W - SUBMENU_PROMPT_W)
#define INPUT_EDIT_W     (VALUE_W + GAP_W + UNIT_W + GAP_W + STATUS_W)
#define NOTE_W            INNER_W
#define CHOICE_STR_MAX   (VALUE_W - 4)

/* Buffers — derived so they scale with the column widths above. */

#define INPUT_BUF        (INPUT_EDIT_W - 2)    /* user-typable chars + null (auto-scales with VALUE_W) */
#define INPUT_EDIT_BUF   (INPUT_BUF + 16)      /* displayed editor text (prompt + buf + cursor + margin) */
#define READ_BUF         (VALUE_W + 8)         /* read() callback stack buffer */
#define VALUE_BUF        (VALUE_W * 4 + 24)    /* composed bar/choice text (UTF-8 worst-case + brackets) */
#define ROW_BUF          (INNER_W * 4 + 128)   /* row composition (cells * UTF-8 + ANSI overhead) */

#define CMD_BUF          64                    /* command line input (independent of row widths) */

#endif
