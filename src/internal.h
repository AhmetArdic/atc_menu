/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ATC_MENU_INTERNAL_H
#define ATC_MENU_INTERNAL_H

#include "atc_menu/atc_menu.h"

#include "ansi.h"
#include "layout.h"
#include "symbols.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    KEY_BS  = 8,
    KEY_ESC = 27,
    KEY_DEL = 127,
};

#define KEY_REFRESH 'r'
#define KEY_BACK    'b'
#define KEY_PATH    '?'
#define KEY_CMD     ':'

const atc_menu_port_t *menu_port(void);
const atc_menu_info_t *menu_info(void);
int  menu_printf(const char *fmt, ...);
void menu_park_cursor(void);

#endif
