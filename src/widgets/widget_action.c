/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/render.h"
#include "widgets/widget.h"

#include <stdio.h>
#include <string.h>

static void render(int zebra_idx, const atc_menu_item_t *it) {
    char val_padded[MENU_VALUE_COL + 1];
    snprintf(val_padded, sizeof val_padded, "%*s", MENU_VALUE_COL, "");

    render_cells_t cells = {
        .key    = it->key,
        .label  = it->label,
        .value  = val_padded,
        .unit   = it->unit,
        .status = ATC_ST_NONE,
    };
    render_row_cells(zebra_idx, &cells);
}

static void validate(const atc_menu_item_t *it) {
    if (!it->action)
        atc_menu_printf("WARN: ATC_ROW_ACTION '%c' missing action\r\n", it->key);
    if (it->label && strlen(it->label) > MENU_LABEL_COL)
        atc_menu_printf("WARN: label '%s' exceeds %d cols\r\n",
                        it->label, MENU_LABEL_COL);
    if (it->unit && strlen(it->unit) > MENU_UNIT_COL)
        atc_menu_printf("WARN: unit '%s' exceeds %d cols\r\n",
                        it->unit, MENU_UNIT_COL);
}

const widget_ops_t widget_action_ops = {
    .render   = render,
    .validate = validate,
    .on_key   = NULL,
};
