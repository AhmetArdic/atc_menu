/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Layered row rendering for atc_menu.
 *
 *   Layer 1 — row_buf_t: byte buffer that flushes through the port.
 *   Layer 2 — region:    a fixed-width slot inside a row.
 *   Layer 3 — row_t:     ordered set of regions sharing one buffer/border.
 *
 * Higher layers (widgets, screen chrome) compose on top of these.
 */

#ifndef ATC_MENU_ROW_H
#define ATC_MENU_ROW_H

#include "core/internal.h"

/* ---------------------------------------------------------------- Layer 1 */

typedef struct {
    char buf[MENU_ROW_BUF];
    int  len;
} row_buf_t;

void row_buf_reset (row_buf_t *b);
void row_buf_flush (row_buf_t *b);
void row_buf_printf(row_buf_t *b, const char *fmt, ...);

/* ---------------------------------------------------------------- Layer 2 */

typedef enum {
    REGION_ALIGN_LEFT,
    REGION_ALIGN_RIGHT,
} region_align_t;

typedef struct {
    int             width;          /* visible columns */
    region_align_t  align;
    const char     *default_style;  /* applied if content style is NULL/empty */
    int             gap_after;      /* spaces emitted after region (last is ignored) */
} region_def_t;

/* ---------------------------------------------------------------- Layer 3 */

#ifndef ATC_ROW_MAX_REGIONS
#define ATC_ROW_MAX_REGIONS 8
#endif

typedef struct {
    const region_def_t *regions;
    size_t              count;
} row_layout_t;

typedef struct {
    const char *style;
    const char *text;
} region_content_t;

typedef struct {
    const row_layout_t *layout;
    int                 zebra_idx;
    const char         *bg;
    region_content_t    content[ATC_ROW_MAX_REGIONS];
    row_buf_t           out;
} row_t;

void row_begin(row_t *r, const row_layout_t *layout, int zebra_idx);
void row_set  (row_t *r, size_t region_idx,
               const char *style, const char *text);
void row_end  (row_t *r);

/* Shared layouts. Definitions live in row.c. */
extern const row_layout_t ROW_LAYOUT_SCALAR;       /* 5 regions: KEY LABEL VALUE UNIT STATUS */
extern const row_layout_t ROW_LAYOUT_GROUP;        /* 2 regions: KEY WIDE_LABEL */
extern const row_layout_t ROW_LAYOUT_SUBMENU;      /* 3 regions: KEY PROMPT LABEL */
extern const row_layout_t ROW_LAYOUT_INPUT_EDIT;   /* 3 regions: KEY LABEL EDIT */
extern const row_layout_t ROW_LAYOUT_NOTE;         /* 1 region:  full-width dim */

/* Status glyph + colour lookup (kept here because widgets read it directly). */
typedef struct { const char *color, *text; } status_disp_t;
const status_disp_t *status_disp(atc_status_t st);

#endif
