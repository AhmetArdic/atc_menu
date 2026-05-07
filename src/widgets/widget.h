/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * Adding a new row type:
 *   1. Add an enum value in atc_menu.h
 *   2. Add widget_<name>.c with a static const widget_ops_t and any
 *      type-specific helpers
 *   3. Wire it into widget_ops() (widget.c)
 */

#ifndef ATC_MENU_WIDGET_H
#define ATC_MENU_WIDGET_H

#include "core/internal.h"

typedef struct {
    void (*render)(int zebra_idx, const atc_menu_item_t *it);

    /* Optional — partial repaint for "data only" updates: re-emit the
     * regions that change at runtime (value/status, edit cursor) and
     * skip the static columns (key/label/unit). NULL falls back to a
     * full row render. */
    void (*render_data)(int zebra_idx, const atc_menu_item_t *it,
                        size_t index);

    /* Optional — NULL means no type-specific validation. */
    void (*validate)(const atc_menu_item_t *it);

    /* Optional — NULL means "run item->action then repaint the row". */
    void (*on_key)(const atc_menu_item_t *it, size_t index);
} widget_ops_t;

const widget_ops_t *widget_ops(atc_row_type_t type);

/* Shared scalar render: read() into MENU_REGION_VALUE_W right-aligned cell.
 * Suitable for VALUE / STATE / ACTION (read==NULL) / INPUT in normal mode. */
void widget_render_scalar(int zebra_idx, const atc_menu_item_t *it);

/* Shared scalar partial repaint: updates only the VALUE and STATUS regions
 * of ROW_LAYOUT_SCALAR. Used by VALUE/STATE/ACTION default on_key. */
void widget_render_scalar_data(int zebra_idx, const atc_menu_item_t *it,
                               size_t index);

/* Emit overflow warnings for label > MENU_REGION_LABEL_W and unit > MENU_REGION_UNIT_W. */
void widget_validate_label_unit(const atc_menu_item_t *it);

bool widget_input_active(void);
void widget_input_render_footer(void);
void widget_input_key(char k);
void widget_input_reset(void);

bool widget_choice_active(void);
void widget_choice_render_footer(void);
void widget_choice_key(char k);
void widget_choice_reset(void);

#endif
