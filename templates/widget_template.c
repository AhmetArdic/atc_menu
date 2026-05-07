/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Skeleton for a new widget. Not built by CMake — this is a copy-paste
 * starting point. To add a widget:
 *
 *   1. include/atc_menu/atc_menu.h
 *        - Add ATC_ROW_<NAME> to atc_row_type_t.
 *        - If the widget needs new fields on atc_menu_item_t (e.g. like
 *          CHOICE/INPUT do), extend the anonymous union.
 *
 *   2. Copy this file to src/widgets/widget_<name>.c.
 *        - Rename `widget_template_ops` to `widget_<name>_ops`.
 *        - Drop the callbacks you don't need — only `render` is required.
 *
 *   3. src/widgets/widget.c
 *        - Add `extern const widget_ops_t widget_<name>_ops;`
 *        - Add `case ATC_ROW_<NAME>: return &widget_<name>_ops;` to
 *          widget_ops().
 *
 *   4. CMakeLists.txt
 *        - Add src/widgets/widget_<name>.c to the atc_menu target.
 *        - (Optional) Gate it behind an ATC_MENU_WIDGET_<NAME> option
 *          like the BAR/CHOICE/INPUT widgets do, with a stub block in
 *          widget.c (#if !ATC_MENU_WIDGET_<NAME> ...).
 *
 *   5. include/atc_menu/atc_menu.h
 *        - Document the new row type in the table on atc_row_type_t and
 *          note any required atc_menu_item_t fields.
 */

#include "render/render.h"
#include "render/row.h"
#include "widgets/widget.h"

#include <stdio.h>

/* ----------------------------------------------------------------- render
 * Mandatory. Emits a full row. Compose with row_t + a layout from row.h
 * (ROW_LAYOUT_SCALAR matches KEY/LABEL/VALUE/UNIT/STATUS), or define
 * a private layout if the widget needs its own column shape.
 *
 * @p zebra_idx is the row's stripe index (used by row_begin to pick the
 * alternating background).
 */
static void render(int zebra_idx, const atc_menu_item_t *it) {
    char         val[MENU_BUF_SIZE] = {0};
    atc_status_t st                 = ATC_ST_NONE;
    if (it->read) it->read(val, sizeof val, &st);

    char key_buf[2] = { it->key ? it->key : ' ', 0 };
    const status_disp_t *sd = status_disp(st);

    row_t r;
    row_begin(&r, &ROW_LAYOUT_SCALAR, zebra_idx);
    row_set(&r, 0, NULL,      key_buf);
    row_set(&r, 1, NULL,      it->label);
    row_set(&r, 2, NULL,      val);
    row_set(&r, 3, NULL,      it->unit);
    row_set(&r, 4, sd->color, sd->text);
    row_end(&r);
}

/* ------------------------------------------------------------ render_data
 * Optional. Partial repaint after a key press — emit only the regions
 * whose content actually changed (typically VALUE/STATUS) and skip the
 * static columns. Called by menu_render_data_at(). Leave NULL to fall
 * back to a full-row render via menu_render_row_at().
 *
 * For the SCALAR layout case, the shared helper widget_render_scalar_data
 * already covers VALUE+STATUS updates; just point .render_data at it.
 */
static void render_data(int zebra_idx, const atc_menu_item_t *it,
                        size_t index) {
    (void)zebra_idx;
    char         val[MENU_BUF_SIZE] = {0};
    atc_status_t st                 = ATC_ST_NONE;
    if (it->read) it->read(val, sizeof val, &st);

    const status_disp_t *sd = status_disp(st);

    menu_render_region_at(index, &ROW_LAYOUT_SCALAR, 2, NULL,      val);
    menu_render_region_at(index, &ROW_LAYOUT_SCALAR, 4, sd->color, sd->text);
}

/* ------------------------------------------------------------- validate
 * Optional. Init-time check; emits warnings for misconfigured rows so
 * problems are caught at boot rather than at first key press.
 */
static void validate(const atc_menu_item_t *it) {
    if (!it->read)
        menu_printf("WARN: ATC_ROW_TEMPLATE '%c' missing read\r\n", it->key);
    widget_validate_label_unit(it);
}

/* --------------------------------------------------------------- on_key
 * Optional. Handles the row's hotkey. Leave NULL to get the default:
 *   if (it->action) it->action();
 *   menu_render_data_at(index);
 *
 * Override when the widget needs to enter an edit mode, cycle internal
 * state, or otherwise diverge from "fire action + repaint". For modal
 * widgets see widget_input.c / widget_choice.c — they install a private
 * key-handler via widget_<name>_active() / widget_<name>_key() so menu.c
 * routes subsequent keystrokes to them until they exit.
 */
static void on_key(const atc_menu_item_t *it, size_t index) {
    if (it->action) it->action();
    menu_render_data_at(index);
}

const widget_ops_t widget_template_ops = {
    .render      = render,
    .render_data = render_data,
    .validate    = validate,
    .on_key      = on_key,
};
