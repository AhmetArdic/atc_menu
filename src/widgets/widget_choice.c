/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/render.h"
#include "render/row.h"
#include "widgets/widget.h"

#include <stdio.h>
#include <string.h>

#define WIDGET_CHOICE_STR_MAX  (MENU_REGION_VALUE_W - 4)

static struct {
    bool                          active;
    uint8_t                       pending_idx;
    size_t                        index;
    const atc_menu_item_t        *item;
} S;

bool widget_choice_active(void) { return S.active; }

void widget_choice_reset(void) {
    S.active      = false;
    S.pending_idx = 0;
    S.index       = 0;
    S.item        = NULL;
}

static const char *choice_str(const atc_menu_item_t *it, uint8_t idx) {
    if (!it->choices || it->choice_count == 0) return "";
    if (idx >= it->choice_count) return "";
    const char *s = it->choices[idx];
    return s ? s : "";
}

static const char *current_choice(const atc_menu_item_t *it) {
    if (!it->choice_idx) return "";
    return choice_str(it, *it->choice_idx);
}

static void format_choice_box(const char *sel, char *out, size_t cap) {
    int sl = (int)strlen(sel);
    if (sl > WIDGET_CHOICE_STR_MAX) sl = WIDGET_CHOICE_STR_MAX;
    int lpad = (WIDGET_CHOICE_STR_MAX - sl) / 2;
    int rpad = WIDGET_CHOICE_STR_MAX - sl - lpad;
    snprintf(out, cap, "%s %*s%.*s%*s %s",
             SYM_CHOICE_LBR, lpad, "", sl, sel, rpad, "", SYM_CHOICE_RBR);
}

static void compose(const atc_menu_item_t *it,
                    char *box, size_t box_cap,
                    const char **out_style,
                    atc_status_t *out_st) {
    atc_status_t st = ATC_ST_OK;
    if (it->read) {
        char tmp[MENU_BUF_SIZE] = {0};
        it->read(tmp, MENU_BUF_SIZE, &st);
    }

    bool        editing = (S.active && S.item == it);
    const char *sel     = editing ? choice_str(it, S.pending_idx)
                                  : current_choice(it);

    format_choice_box(sel, box, box_cap);
    *out_style = editing ? ANSI_BOLD ANSI_FG_VAL : NULL;
    *out_st    = st;
}

static void render(int zebra_idx, const atc_menu_item_t *it) {
    char         box[MENU_REGION_VALUE_BUF];
    const char  *value_style = NULL;
    atc_status_t st;
    compose(it, box, sizeof box, &value_style, &st);

    char key_buf[2] = { it->key ? it->key : ' ', 0 };
    const status_disp_t *sd = status_disp(st);

    row_t r;
    row_begin(&r, &ROW_LAYOUT_SCALAR, zebra_idx);
    row_set(&r, 0, NULL,        key_buf);
    row_set(&r, 1, NULL,        it->label);
    row_set(&r, 2, value_style, box);
    row_set(&r, 3, NULL,        it->unit);
    row_set(&r, 4, sd->color,   sd->text);
    row_end(&r);
}

static void render_data(int zebra_idx, const atc_menu_item_t *it,
                        size_t index) {
    (void)zebra_idx;
    char         box[MENU_REGION_VALUE_BUF];
    const char  *value_style = NULL;
    atc_status_t st;
    compose(it, box, sizeof box, &value_style, &st);

    const status_disp_t *sd = status_disp(st);

    menu_render_region_at(index, &ROW_LAYOUT_SCALAR, 2, value_style, box);
    menu_render_region_at(index, &ROW_LAYOUT_SCALAR, 4, sd->color,   sd->text);
}

void widget_choice_render_footer(void) {
    if (!S.item) return;
    menu_printf("\r\n" ANSI_DIM
        "[%c] cycle  [Enter] commit  [Esc] cancel"
        ANSI_RESET ANSI_EOL "\r\n", S.item->key);
}

static void validate(const atc_menu_item_t *it) {
    if (!it->choices || it->choice_count == 0 || !it->choice_idx) {
        menu_printf("WARN: ATC_ROW_CHOICE '%c' missing choices/idx\r\n", it->key);
        return;
    }
    for (uint8_t c = 0; c < it->choice_count; c++) {
        if (it->choices[c] && strlen(it->choices[c]) > WIDGET_CHOICE_STR_MAX)
            menu_printf("WARN: CHOICE '%c' choice '%s' exceeds %d cols\r\n",
                            it->key, it->choices[c], WIDGET_CHOICE_STR_MAX);
    }
}

static void on_key(const atc_menu_item_t *it, size_t index) {
    if (!it->choice_idx || it->choice_count == 0) return;

    if (it->choice_commit) {
        S.active      = true;
        S.item        = it;
        S.index       = index;
        S.pending_idx = *it->choice_idx;
        menu_render_data_at(index);
        menu_render_footer();
        return;
    }

    *it->choice_idx = (uint8_t)((*it->choice_idx + 1) % it->choice_count);
    menu_render_data_at(index);
}

void widget_choice_key(char k) {
    if (!S.active || !S.item) return;

    if (k == '\r' || k == '\n') {
        *S.item->choice_idx = S.pending_idx;
        atc_action_fn_t cb  = S.item->choice_commit;
        size_t          idx = S.index;
        widget_choice_reset();
        if (cb) cb();
        menu_render_data_at(idx);
        menu_render_footer();
        return;
    }
    if (k == KEY_ESC) {
        size_t idx = S.index;
        widget_choice_reset();
        menu_render_data_at(idx);
        menu_render_footer();
        return;
    }
    if (k == S.item->key) {
        S.pending_idx = (uint8_t)((S.pending_idx + 1) % S.item->choice_count);
        menu_render_data_at(S.index);
    }
}

const widget_ops_t widget_choice_ops = {
    .render      = render,
    .render_data = render_data,
    .validate    = validate,
    .on_key      = on_key,
};
