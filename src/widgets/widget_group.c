/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/render.h"
#include "widgets/widget.h"

#include <string.h>

static void render(int zebra_idx, const atc_menu_item_t *it) {
    row_t r;
    row_open(&r, zebra_idx);
    row_pad(&r);
    row_key(&r, it->key);
    row_gap(&r);
    row_cell(&r, MENU_GROUP_LABEL_W, ANSI_BOLD, it->label);
    row_pad(&r);
    row_close(&r);
}

static void validate(const atc_menu_item_t *it) {
    if (it->label && strlen(it->label) > MENU_GROUP_LABEL_W)
        menu_printf("WARN: GROUP label '%s' exceeds %d cols\r\n",
                        it->label, MENU_GROUP_LABEL_W);
}

static void on_key(const atc_menu_item_t *it, size_t index) {
    if (it->action) it->action();
    menu_render_group_span(index);
}

const widget_ops_t widget_group_ops = {
    .render   = render,
    .validate = validate,
    .on_key   = on_key,
};
