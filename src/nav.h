/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ATC_MENU_NAV_H
#define ATC_MENU_NAV_H

#include "internal.h"

#ifndef ATC_MENU_STACK_DEPTH
#define ATC_MENU_STACK_DEPTH 4
#endif

void nav_reset(const atc_menu_table_t *root);

const atc_menu_item_t *nav_items(void);
size_t                 nav_count(void);
size_t                 nav_depth(void);

const char *const *nav_notes(void);
size_t             nav_note_count(void);

void nav_push(const atc_menu_item_t *opener);
void nav_pop(void);
void nav_show_path(void);

#endif
