/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ATC_MENU_RENDER_H
#define ATC_MENU_RENDER_H

#include "core/internal.h"

typedef struct { const char *color, *text; } status_disp_t;

const status_disp_t *status_disp(atc_status_t st);

typedef struct {
    const char *bg;
    char        buf[MENU_ROW_BUF];
    int         len;
} row_t;

void row_reset (row_t *r);
void row_flush (row_t *r);
void row_printf(row_t *r, const char *fmt, ...);

void row_open       (row_t *r, int zebra_idx);
void row_gap        (row_t *r);
void row_key        (row_t *r, char k);
void row_cell       (row_t *r, int width, const char *style, const char *text);
void row_cell_right (row_t *r, int width, const char *style, const char *text);
void row_text       (row_t *r, const char *style, const char *text);
void row_close      (row_t *r);

typedef struct {
    char         key;
    const char  *label;
    const char  *value;
    const char  *value_color;
    const char  *unit;
    atc_status_t status;
} render_cells_t;

void render_row_cells(int zebra_idx, const render_cells_t *cells);

void render_box_top(const char *title, const char *version);
void render_box_separator(void);
void render_box_bottom(void);
void render_info_row(const char *l, const char *r,
                     const char *l_color, const char *r_color);

void render_notes_block(const char *const *notes, size_t count);
int  render_notes_lines(size_t count);
void render_validate_notes(const char *const *notes, size_t count);

void render_header(const atc_menu_info_t *info);
int  render_header_lines(const atc_menu_info_t *info);

void render_default_footer(bool show_back);

void render_status_line(const char *msg);

void render_park_cursor(int y, bool clear_below);

#endif
