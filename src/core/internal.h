/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ATC_MENU_INTERNAL_H
#define ATC_MENU_INTERNAL_H

#include "atc_menu/atc_menu.h"

#include "render/ansi.h"
#include "render/layout.h"
#include "render/symbols.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    KEY_BS  = 8,
    KEY_ESC = 27,
    KEY_DEL = 127,
};

#define ATC_KEY_REFRESH 'r'
#define ATC_KEY_BACK    'b'
#define ATC_KEY_PATH    '?'
#define ATC_KEY_CMD     ':'

const char *reserved_key_role(char k);

const atc_menu_port_t *menu_port(void);
const atc_menu_info_t *menu_info(void);

bool menu_status_dirty(void);
void menu_set_status_dirty(bool dirty);

void menu_render_row_at(size_t index);
void menu_render_group_span(size_t start);
void menu_park_cursor(void);

int menu_printf(const char *fmt, ...);

#endif
