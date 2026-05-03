/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/render.h"
#include "widgets/widget.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool                   g_active;
static char                   g_buf[MENU_INPUT_BUF];
static uint8_t                g_pos;
static const atc_menu_item_t *g_item;

bool widget_input_active(void) { return g_active; }

void widget_input_reset(void) {
    g_active = false;
    g_pos    = 0;
    g_buf[0] = '\0';
    g_item   = NULL;
}

static void render_normal(int zebra_idx, const atc_menu_item_t *it) {
    char         val[MENU_BUF_SIZE] = {0};
    atc_status_t st                 = ATC_ST_NONE;
    if (it->read) it->read(val, MENU_BUF_SIZE, &st);

    char val_padded[MENU_VALUE_COL + 1];
    snprintf(val_padded, sizeof val_padded, "%*.*s",
             MENU_VALUE_COL, MENU_VALUE_COL, val);

    render_cells_t cells = {
        .key    = it->key,
        .label  = it->label,
        .value  = val_padded,
        .unit   = it->unit,
        .status = st,
    };
    render_row_cells(zebra_idx, &cells);
}

static void render_active(int zebra_idx, const atc_menu_item_t *it) {
    char   edit[MENU_INPUT_EDIT_BUF];
    size_t ep  = 0;
    size_t cap = sizeof edit;

    int n = snprintf(edit + ep, cap - ep, "%s", SYM_INPUT_PROMPT);
    if (n > 0 && (size_t)n < cap - ep) ep += (size_t)n;

    size_t copy = g_pos;
    if (copy > cap - ep - sizeof SYM_INPUT_CURSOR - 1)
        copy = cap - ep - sizeof SYM_INPUT_CURSOR - 1;
    memcpy(edit + ep, g_buf, copy);
    ep += copy;

    n = snprintf(edit + ep, cap - ep, "%s", SYM_INPUT_CURSOR);
    if (n > 0 && (size_t)n < cap - ep) ep += (size_t)n;
    edit[ep] = '\0';

    row_t r;
    row_open(&r, zebra_idx);
    row_pad(&r);
    row_key(&r, it->key);
    row_gap(&r);
    row_cell(&r, MENU_LABEL_COL,    NULL,         it->label);
    row_gap(&r);
    row_cell(&r, MENU_INPUT_EDIT_W, ANSI_FG_VAL,  edit);
    row_pad(&r);
    row_close(&r);
}

static void render(int zebra_idx, const atc_menu_item_t *it) {
    if (g_active && g_item == it) render_active(zebra_idx, it);
    else                          render_normal(zebra_idx, it);
}

void widget_input_render_footer(void) {
    if (!g_item) return;
    if (g_item->input_type == ATC_INPUT_INT
     || g_item->input_type == ATC_INPUT_HEX) {
        menu_printf("\r\n" ANSI_DIM
            "[Enter] commit  [Esc] cancel  [BS] erase    range: %ld..%ld"
            ANSI_RESET ANSI_EOL "\r\n",
            (long)g_item->input_min, (long)g_item->input_max);
    } else {
        menu_printf("\r\n" ANSI_DIM
            "[Enter] commit  [Esc] cancel  [BS] erase"
            ANSI_RESET ANSI_EOL "\r\n");
    }
}

static bool char_acceptable(atc_input_type_t t, char k) {
    switch (t) {
        case ATC_INPUT_INT:
            return (k >= '0' && k <= '9') || k == '-';
        case ATC_INPUT_FLOAT:
            return (k >= '0' && k <= '9') || k == '-' || k == '.';
        case ATC_INPUT_HEX:
            return (k >= '0' && k <= '9') || (k >= 'a' && k <= 'f')
                || (k >= 'A' && k <= 'F') || k == 'x' || k == 'X';
        case ATC_INPUT_STR:
            return k >= 32 && k <= 126;
    }
    return false;
}

static bool validate_buffer(void) {
    if (g_pos == 0) {
        atc_menu_status("invalid: empty");
        return false;
    }
    g_buf[g_pos] = '\0';

    if (g_item->input_type == ATC_INPUT_STR) return true;

    char *end = NULL;
    long  v   = 0;
    errno = 0;
    if (g_item->input_type == ATC_INPUT_HEX) {
        v = strtol(g_buf, &end, 16);
    } else if (g_item->input_type == ATC_INPUT_INT) {
        v = strtol(g_buf, &end, 10);
    } else {
        v = (long)strtod(g_buf, &end);
    }

    if (errno == ERANGE || end == g_buf || (end && *end != '\0')) {
        atc_menu_status("invalid: parse error");
        return false;
    }
    if (g_item->input_type == ATC_INPUT_INT
     || g_item->input_type == ATC_INPUT_HEX) {
        if (v < g_item->input_min || v > g_item->input_max) {
            char msg[64];
            snprintf(msg, sizeof msg, "out of range: %ld..%ld",
                     (long)g_item->input_min, (long)g_item->input_max);
            atc_menu_status(msg);
            return false;
        }
    }
    return true;
}

void widget_input_key(char k) {
    if (!g_active || !g_item) return;

    if (k == '\r' || k == '\n') {
        if (!validate_buffer()) return;
        bool accepted = !g_item->input_commit || g_item->input_commit(g_buf);
        if (accepted) widget_input_reset();
        atc_menu_render();
        return;
    }
    if (k == KEY_ESC) {
        widget_input_reset();
        atc_menu_render();
        return;
    }
    if (k == KEY_BS || k == KEY_DEL) {
        if (g_pos) {
            g_pos--;
            g_buf[g_pos] = '\0';
            atc_menu_render();
        }
        return;
    }
    if (g_pos < sizeof g_buf - 1
        && char_acceptable(g_item->input_type, k)) {
        g_buf[g_pos++] = k;
        g_buf[g_pos]   = '\0';
        atc_menu_render();
    }
}

static void validate(const atc_menu_item_t *it) {
    if (!it->read || !it->input_commit)
        menu_printf("WARN: ATC_ROW_INPUT '%c' missing read/input_commit\r\n",
                        it->key);
    if ((it->input_type == ATC_INPUT_INT || it->input_type == ATC_INPUT_HEX)
        && it->input_min > it->input_max)
        menu_printf("WARN: ATC_ROW_INPUT '%c' min > max\r\n", it->key);
}

static void on_key(const atc_menu_item_t *it, size_t index) {
    (void)index;
    g_active = true;
    g_pos    = 0;
    g_buf[0] = '\0';
    g_item   = it;
    atc_menu_render();
}

const widget_ops_t widget_input_ops = {
    .render   = render,
    .validate = validate,
    .on_key   = on_key,
};
