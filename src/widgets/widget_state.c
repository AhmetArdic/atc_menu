/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "widgets/widget.h"

static void validate(const atc_menu_item_t *it) {
    if (!it->read || !it->action)
        menu_printf("WARN: ATC_ROW_STATE '%c' missing read/action\r\n", it->key);
    widget_validate_label_unit(it);
}

const widget_ops_t widget_state_ops = {
    .render   = widget_render_scalar,
    .validate = validate,
    .on_key   = NULL,
};
