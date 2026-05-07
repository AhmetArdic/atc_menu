/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/render.h"
#include "render/row.h"
#include "widgets/widget.h"

#include <string.h>

static void render(int zebra_idx, const atc_menu_item_t *it) {
    char key_buf[2] = { it->key ? it->key : ' ', 0 };

    row_t r;
    row_begin(&r, &ROW_LAYOUT_GROUP, zebra_idx);
    row_set(&r, 0, NULL,      key_buf);
    row_set(&r, 1, ANSI_BOLD, it->label);
    row_end(&r);
}

static void validate(const atc_menu_item_t *it) {
    if (it->label && strlen(it->label) > MENU_REGION_GROUP_LABEL_W)
        menu_printf("WARN: GROUP label '%s' exceeds %d cols\r\n",
                        it->label, MENU_REGION_GROUP_LABEL_W);
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
