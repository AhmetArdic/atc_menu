/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "input/nav.h"
#include "render/render.h"
#include "render/row.h"
#include "widgets/widget.h"

#include <string.h>

static void render(int zebra_idx, const atc_menu_item_t *it) {
    char key_buf[2] = { it->key ? it->key : ' ', 0 };

    row_t r;
    row_begin(&r, &ROW_LAYOUT_SUBMENU, zebra_idx);
    row_set(&r, 0, NULL, key_buf);
    row_set(&r, 1, NULL, SYM_SUBMENU);
    row_set(&r, 2, NULL, it->label);
    row_end(&r);
}

static void validate(const atc_menu_item_t *it) {
    if (!it->submenu || !it->submenu->items || it->submenu->count == 0)
        menu_printf("WARN: ATC_ROW_SUBMENU '%c' missing submenu\r\n", it->key);
    if (it->label && strlen(it->label) > MENU_SUBMENU_LABEL_W)
        menu_printf("WARN: SUBMENU label '%s' exceeds %d cols\r\n",
                        it->label, MENU_SUBMENU_LABEL_W);
    if (it->submenu)
        render_validate_notes(it->submenu->notes, it->submenu->note_count);
}

static void on_key(const atc_menu_item_t *it, size_t index) {
    (void)index;
    nav_push(it);
    atc_menu_render();
}

const widget_ops_t widget_submenu_ops = {
    .render   = render,
    .validate = validate,
    .on_key   = on_key,
};
