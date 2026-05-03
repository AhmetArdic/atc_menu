/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "input/cmdmode.h"

static struct {
    bool   active;
    char   buf[MENU_CMD_BUF];
    size_t len;
} S;

void cmdmode_reset(void) {
    S.active = false;
    S.len    = 0;
}

bool cmdmode_active(void) { return S.active; }

void cmdmode_enter(void) {
    S.active = true;
    S.len    = 0;
    menu_printf("%s", "\r\n" SYM_CMD_PROMPT);
}

void cmdmode_key(char k) {
    const atc_menu_port_t *port = menu_port();
    if (!port) return;

    if (k == '\r' || k == '\n') {
        S.buf[S.len] = '\0';
        cmdmode_reset();
        if (port->cmd) port->cmd(S.buf);
        atc_menu_render();
        return;
    }
    if (k == KEY_ESC) {
        cmdmode_reset();
        menu_park_cursor();
        return;
    }
    if (k == KEY_BS || k == KEY_DEL) {
        if (S.len) {
            S.len--;
            menu_printf("%s", "\b \b");
        }
        return;
    }
    if (S.len < sizeof S.buf - 1) {
        S.buf[S.len++] = k;
        port->write(&k, 1);
    }
}
