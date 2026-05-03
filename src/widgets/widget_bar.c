/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/render.h"
#include "widgets/widget.h"

#include <stdio.h>
#include <stdlib.h>

#define WIDGET_BAR_PCT_MIN    0
#define WIDGET_BAR_PCT_MAX  100
#define WIDGET_BAR_CELLS   (MENU_VALUE_COL - 2)
#define WIDGET_BAR_SUB      8
#define WIDGET_BAR_LEVELS  (WIDGET_BAR_CELLS * WIDGET_BAR_SUB)

/* Left-aligned partial blocks 1/8 .. 7/8, indexed by sub-cell remainder. */
static const char *const BAR_PARTIAL[WIDGET_BAR_SUB] = {
    "",  "▏", "▎", "▍", "▌", "▋", "▊", "▉",
};

static int clamp_pct(long pct) {
    if (pct < WIDGET_BAR_PCT_MIN) return WIDGET_BAR_PCT_MIN;
    if (pct > WIDGET_BAR_PCT_MAX) return WIDGET_BAR_PCT_MAX;
    return (int)pct;
}

static const char *fill_color_for(atc_status_t st) {
    switch (st) {
        case ATC_ST_WARN: return ANSI_FG_WARN;
        case ATC_ST_ERR:  return ANSI_FG_ERR;
        default:          return ANSI_FG_OK;
    }
}

static size_t bar_append(char *buf, size_t cap, size_t pos, const char *s) {
    int n = snprintf(buf + pos, cap - pos, "%s", s);
    return (n > 0 && (size_t)n < cap - pos) ? pos + (size_t)n : pos;
}

static void render(int zebra_idx, const atc_menu_item_t *it) {
    char         raw[MENU_BUF_SIZE] = {0};
    atc_status_t st                 = ATC_ST_NONE;
    if (it->read) it->read(raw, MENU_BUF_SIZE, &st);

    int pct  = clamp_pct(strtol(raw, NULL, 10));
    int subs = (pct * WIDGET_BAR_LEVELS + WIDGET_BAR_PCT_MAX / 2)
             /  WIDGET_BAR_PCT_MAX;
    int full = subs / WIDGET_BAR_SUB;
    int part = subs % WIDGET_BAR_SUB;

    char   bar[MENU_VALUE_BUF];
    size_t bp = 0;

    bp = bar_append(bar, sizeof bar, bp, SYM_BAR_LBR);
    for (int i = 0; i < full; i++)
        bp = bar_append(bar, sizeof bar, bp, SYM_BAR_FILL);
    if (part)
        bp = bar_append(bar, sizeof bar, bp, BAR_PARTIAL[part]);
    for (int i = full + (part > 0); i < WIDGET_BAR_CELLS; i++)
        bp = bar_append(bar, sizeof bar, bp, SYM_BAR_EMPTY);
    bp = bar_append(bar, sizeof bar, bp, SYM_BAR_RBR);
    bar[bp] = '\0';

    char unit[MENU_UNIT_COL + 1];
    snprintf(unit, sizeof unit, "%d %%", pct);

    render_cells_t cells = {
        .key         = it->key,
        .label       = it->label,
        .value       = bar,
        .value_color = fill_color_for(st),
        .unit        = unit,
        .status      = st,
    };
    render_row_cells(zebra_idx, &cells);
}

static void validate(const atc_menu_item_t *it) {
    if (!it->read)
        menu_printf("WARN: ATC_ROW_BAR '%c' missing read\r\n", it->key);
}

const widget_ops_t widget_bar_ops = {
    .render   = render,
    .validate = validate,
    .on_key   = NULL,
};
