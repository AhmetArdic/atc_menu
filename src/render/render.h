/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ATC_MENU_RENDER_H
#define ATC_MENU_RENDER_H

#include "core/internal.h"
#include "render/row.h"

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
