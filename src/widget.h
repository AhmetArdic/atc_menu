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

#include "internal.h"

typedef struct {
    void (*render)(int zebra_idx, const atc_menu_item_t *it);

    /* Optional — NULL means no type-specific validation. */
    void (*validate)(const atc_menu_item_t *it);

    /* Optional — NULL means "run item->action then repaint the row". */
    void (*on_key)(const atc_menu_item_t *it, size_t index);
} widget_ops_t;

const widget_ops_t *widget_ops(atc_row_type_t type);

bool widget_input_active(void);
void widget_input_render_footer(void);
void widget_input_key(char k);
void widget_input_reset(void);

#endif
