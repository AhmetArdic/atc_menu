/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/render.h"
#include "widgets/widget.h"

#include <stdio.h>
#include <string.h>

#define WIDGET_CHOICE_STR_MAX  (MENU_VALUE_COL - 4)

static bool                   g_active;
static uint8_t                g_pending_idx;
static size_t                 g_index;
static const atc_menu_item_t *g_item;

bool widget_choice_active(void) { return g_active; }

void widget_choice_reset(void) {
    g_active      = false;
    g_pending_idx = 0;
    g_index       = 0;
    g_item        = NULL;
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

static void render(int zebra_idx, const atc_menu_item_t *it) {
    atc_status_t st = ATC_ST_OK;

    if (it->read) {
        char tmp[MENU_BUF_SIZE] = {0};
        it->read(tmp, MENU_BUF_SIZE, &st);
    }

    bool        editing = (g_active && g_item == it);
    const char *sel     = editing ? choice_str(it, g_pending_idx) : current_choice(it);

    char box[MENU_VALUE_BUF];
    format_choice_box(sel, box, sizeof box);

    render_cells_t cells = {
        .key         = it->key,
        .label       = it->label,
        .value       = box,
        .value_color = editing ? ANSI_BOLD ANSI_FG_VAL : NULL,
        .unit        = it->unit,
        .status      = st,
    };
    render_row_cells(zebra_idx, &cells);
}

void widget_choice_render_footer(void) {
    if (!g_item) return;
    menu_printf("\r\n" ANSI_DIM
        "[%c] cycle  [Enter] commit  [Esc] cancel"
        ANSI_RESET ANSI_EOL "\r\n", g_item->key);
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
        g_active      = true;
        g_item        = it;
        g_index       = index;
        g_pending_idx = *it->choice_idx;
        atc_menu_render();
        return;
    }

    *it->choice_idx = (uint8_t)((*it->choice_idx + 1) % it->choice_count);
    menu_render_row_at(index);
}

void widget_choice_key(char k) {
    if (!g_active || !g_item) return;

    if (k == '\r' || k == '\n') {
        *g_item->choice_idx = g_pending_idx;
        atc_action_fn_t cb  = g_item->choice_commit;
        widget_choice_reset();
        if (cb) cb();
        atc_menu_render();
        return;
    }
    if (k == KEY_ESC) {
        widget_choice_reset();
        atc_menu_render();
        return;
    }
    if (k == g_item->key) {
        g_pending_idx = (uint8_t)((g_pending_idx + 1) % g_item->choice_count);
        menu_render_row_at(g_index);
    }
}

const widget_ops_t widget_choice_ops = {
    .render   = render,
    .validate = validate,
    .on_key   = on_key,
};
