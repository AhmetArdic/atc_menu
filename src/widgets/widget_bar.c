/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/render.h"
#include "widgets/widget.h"

#include <stdio.h>
#include <stdlib.h>

#define WIDGET_BAR_PCT_MIN     0
#define WIDGET_BAR_PCT_MAX   100
#define WIDGET_BAR_CELLS  (MENU_VALUE_COL - 2)

static int clamp_pct(long pct) {
    if (pct < WIDGET_BAR_PCT_MIN) return WIDGET_BAR_PCT_MIN;
    if (pct > WIDGET_BAR_PCT_MAX) return WIDGET_BAR_PCT_MAX;
    return (int)pct;
}

static int fill_cells(int pct) {
    int f = (pct * WIDGET_BAR_CELLS + WIDGET_BAR_PCT_MAX / 2) / WIDGET_BAR_PCT_MAX;
    if (f < 0)                f = 0;
    if (f > WIDGET_BAR_CELLS) f = WIDGET_BAR_CELLS;
    return f;
}

static const char *fill_color_for(atc_status_t st) {
    switch (st) {
        case ATC_ST_WARN: return ANSI_FG_WARN;
        case ATC_ST_ERR:  return ANSI_FG_ERR;
        default:          return ANSI_FG_OK;
    }
}

static void render(int zebra_idx, const atc_menu_item_t *it) {
    char         buf[MENU_BUF_SIZE] = {0};
    atc_status_t st                 = ATC_ST_NONE;
    if (it->read) it->read(buf, MENU_BUF_SIZE, &st);

    int pct  = clamp_pct(strtol(buf, NULL, 10));
    int fill = fill_cells(pct);

    char bar[MENU_VALUE_COL + 1];
    int  bp = 0;
    bar[bp++] = SYM_BAR_LBR[0];
    for (int i = 0; i < fill;             i++) bar[bp++] = SYM_BAR_FILL[0];
    for (int i = fill; i < WIDGET_BAR_CELLS; i++) bar[bp++] = SYM_BAR_EMPTY[0];
    bar[bp++] = SYM_BAR_RBR[0];
    bar[bp]   = '\0';

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
        atc_menu_printf("WARN: ATC_ROW_BAR '%c' missing read\r\n", it->key);
}

const widget_ops_t widget_bar_ops = {
    .render   = render,
    .validate = validate,
    .on_key   = NULL,
};
