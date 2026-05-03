/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "input/cmdmode.h"

static bool   g_active;
static char   g_buf[MENU_CMD_BUF];
static size_t g_len;

void cmdmode_reset(void) {
    g_active = false;
    g_len    = 0;
}

bool cmdmode_active(void) { return g_active; }

void cmdmode_enter(void) {
    g_active = true;
    g_len    = 0;
    atc_menu_printf("%s", "\r\n" SYM_CMD_PROMPT);
}

void cmdmode_key(char k) {
    const atc_menu_port_t *port = menu_port();
    if (!port) return;

    if (k == '\r' || k == '\n') {
        g_buf[g_len] = '\0';
        g_active     = false;
        if (port->cmd) port->cmd(g_buf);
        g_len = 0;
        atc_menu_render();
        return;
    }
    if (k == KEY_ESC) {
        g_active = false;
        g_len    = 0;
        menu_park_cursor();
        return;
    }
    if (k == KEY_BS || k == KEY_DEL) {
        if (g_len) {
            g_len--;
            atc_menu_printf("%s", "\b \b");
        }
        return;
    }
    if (g_len < sizeof g_buf - 1) {
        g_buf[g_len++] = k;
        port->write(&k, 1);
    }
}
