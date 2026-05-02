/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render.h"
#include "widget.h"

#include <stdio.h>
#include <string.h>

#define WIDGET_CHOICE_STR_MAX  (MENU_VALUE_COL - 4)

static const char *current_choice(const atc_menu_item_t *it) {
    if (!it->choices || !it->choice_idx || it->choice_count == 0) return "";
    if (*it->choice_idx >= it->choice_count) return "";
    const char *s = it->choices[*it->choice_idx];
    return s ? s : "";
}

static void format_choice_box(const char *sel, char *out, size_t cap) {
    int sl = (int)strlen(sel);
    if (sl > WIDGET_CHOICE_STR_MAX) sl = WIDGET_CHOICE_STR_MAX;
    int lpad = (WIDGET_CHOICE_STR_MAX - sl) / 2;
    int rpad = WIDGET_CHOICE_STR_MAX - sl - lpad;
    snprintf(out, cap, "%s %*s%.*s%*s %s",
             SYM_CHOICE_LBR, lpad, "", sl, sel, rpad, "", SYM_CHOICE_RBR);
}

static void render(int zebra_idx, const atc_menu_item_t *it) {
    atc_status_t st = ATC_ST_OK;

    if (it->read) {
        char tmp[MENU_BUF_SIZE] = {0};
        it->read(tmp, MENU_BUF_SIZE, &st);
    }

    char box[MENU_VALUE_COL + 1];
    format_choice_box(current_choice(it), box, sizeof box);

    render_cells_t cells = {
        .key    = it->key,
        .label  = it->label,
        .value  = box,
        .unit   = it->unit,
        .status = st,
    };
    render_row_cells(zebra_idx, &cells);
}

static void validate(const atc_menu_item_t *it) {
    if (!it->choices || it->choice_count == 0 || !it->choice_idx) {
        atc_menu_printf("WARN: ATC_ROW_CHOICE '%c' missing choices/idx\r\n", it->key);
        return;
    }
    for (uint8_t c = 0; c < it->choice_count; c++) {
        if (it->choices[c] && strlen(it->choices[c]) > WIDGET_CHOICE_STR_MAX)
            atc_menu_printf("WARN: CHOICE '%c' choice '%s' exceeds %d cols\r\n",
                            it->key, it->choices[c], WIDGET_CHOICE_STR_MAX);
    }
}

static void on_key(const atc_menu_item_t *it, size_t index) {
    if (it->choice_idx && it->choice_count > 0)
        *it->choice_idx = (uint8_t)((*it->choice_idx + 1) % it->choice_count);
    menu_render_row_at(index);
}

const widget_ops_t widget_choice_ops = {
    .render   = render,
    .validate = validate,
    .on_key   = on_key,
};
