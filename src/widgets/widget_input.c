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

static struct {
    bool                          active;
    uint8_t                       pos;
    char                          buf[MENU_INPUT_BUF];
    const atc_menu_item_t        *item;
} S;

bool widget_input_active(void) { return S.active; }

void widget_input_reset(void) {
    S.active = false;
    S.pos    = 0;
    S.buf[0] = '\0';
    S.item   = NULL;
}

static void render_active(int zebra_idx, const atc_menu_item_t *it) {
    char   edit[MENU_INPUT_EDIT_BUF];
    size_t ep  = 0;
    size_t cap = sizeof edit;

    int n = snprintf(edit + ep, cap - ep, "%s", SYM_INPUT_PROMPT);
    if (n > 0 && (size_t)n < cap - ep) ep += (size_t)n;

    size_t copy = S.pos;
    if (copy > cap - ep - sizeof SYM_INPUT_CURSOR - 1)
        copy = cap - ep - sizeof SYM_INPUT_CURSOR - 1;
    memcpy(edit + ep, S.buf, copy);
    ep += copy;

    n = snprintf(edit + ep, cap - ep, "%s", SYM_INPUT_CURSOR);
    if (n > 0 && (size_t)n < cap - ep) ep += (size_t)n;
    edit[ep] = '\0';

    row_t r;
    row_open(&r, zebra_idx);
    row_key(&r, it->key);
    row_gap(&r);
    row_cell(&r, MENU_LABEL_COL,    NULL,         it->label);
    row_gap(&r);
    row_cell(&r, MENU_INPUT_EDIT_W, ANSI_FG_VAL,  edit);
    row_close(&r);
}

static void render(int zebra_idx, const atc_menu_item_t *it) {
    if (S.active && S.item == it) render_active(zebra_idx, it);
    else                          widget_render_scalar(zebra_idx, it);
}

void widget_input_render_footer(void) {
    if (!S.item) return;
    if (S.item->input_type == ATC_INPUT_INT
     || S.item->input_type == ATC_INPUT_HEX) {
        menu_printf("\r\n" ANSI_DIM
            "[Enter] commit  [Esc] cancel  [BS] erase    range: %ld..%ld"
            ANSI_RESET ANSI_EOL "\r\n",
            (long)S.item->input_min, (long)S.item->input_max);
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
#if ATC_MENU_INPUT_FLOAT
        case ATC_INPUT_FLOAT:
            return (k >= '0' && k <= '9') || k == '-' || k == '.';
#endif
        case ATC_INPUT_HEX:
            return (k >= '0' && k <= '9') || (k >= 'a' && k <= 'f')
                || (k >= 'A' && k <= 'F') || k == 'x' || k == 'X';
        case ATC_INPUT_STR:
            return k >= 32 && k <= 126;
        default:
            return false;
    }
}

static bool validate_buffer(void) {
    if (S.pos == 0) {
        atc_menu_status("invalid: empty");
        return false;
    }
    S.buf[S.pos] = '\0';

    if (S.item->input_type == ATC_INPUT_STR) return true;

    char *end = NULL;
    long  v   = 0;
    errno = 0;
    if (S.item->input_type == ATC_INPUT_HEX) {
        v = strtol(S.buf, &end, 16);
    } else if (S.item->input_type == ATC_INPUT_INT) {
        v = strtol(S.buf, &end, 10);
    } else {
#if ATC_MENU_INPUT_FLOAT
        v = (long)strtod(S.buf, &end);
#else
        atc_menu_status("invalid: FLOAT input not enabled");
        return false;
#endif
    }

    if (errno == ERANGE || end == S.buf || (end && *end != '\0')) {
        atc_menu_status("invalid: parse error");
        return false;
    }
    if (S.item->input_type == ATC_INPUT_INT
     || S.item->input_type == ATC_INPUT_HEX) {
        if (v < S.item->input_min || v > S.item->input_max) {
            char msg[64];
            snprintf(msg, sizeof msg, "out of range: %ld..%ld",
                     (long)S.item->input_min, (long)S.item->input_max);
            atc_menu_status(msg);
            return false;
        }
    }
    return true;
}

void widget_input_key(char k) {
    if (!S.active || !S.item) return;

    if (k == '\r' || k == '\n') {
        if (!validate_buffer()) return;
        bool accepted = !S.item->input_commit || S.item->input_commit(S.buf);
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
        if (S.pos) {
            S.pos--;
            S.buf[S.pos] = '\0';
            atc_menu_render();
        }
        return;
    }
    if (S.pos < sizeof S.buf - 1
        && char_acceptable(S.item->input_type, k)) {
        S.buf[S.pos++] = k;
        S.buf[S.pos]   = '\0';
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
    widget_validate_label_unit(it);
}

static void on_key(const atc_menu_item_t *it, size_t index) {
    (void)index;
    S.active = true;
    S.pos    = 0;
    S.buf[0] = '\0';
    S.item   = it;
    atc_menu_render();
}

const widget_ops_t widget_input_ops = {
    .render   = render,
    .validate = validate,
    .on_key   = on_key,
};
