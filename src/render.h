/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ATC_MENU_RENDER_H
#define ATC_MENU_RENDER_H

#include "internal.h"

void render_all(void);
void render_row(size_t index);
void render_row_input(size_t index, const char *edit_text);
void render_row_choice(size_t index, const char *pending_label);
void render_group_span(size_t start);

void render_default_footer(bool show_back);
int  render_footer_anchor_line(void);

void render_validate_item(const atc_menu_item_t *it);
void render_validate_notes(const char *const *notes, size_t count);

#endif
