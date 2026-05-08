/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "atc_menu/atc_menu.h"

#include "internal.h"
#include "nav.h"
#include "render.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================ globals */

static const atc_menu_port_t *g_port;
static const atc_menu_info_t *g_info;
static bool                   g_status_dirty;

const atc_menu_port_t *menu_port(void)         { return g_port; }
const atc_menu_info_t *menu_info(void)         { return g_info; }
bool                   menu_status_dirty(void) { return g_status_dirty; }
void menu_set_status_dirty(bool dirty)         { g_status_dirty = dirty; }

int menu_printf(const char *fmt, ...) {
    if (!g_port || !g_port->write) return 0;
    char buf[MENU_ROW_BUF];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0) return n;
    size_t len = (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1;
    g_port->write(buf, len);
    return (int)len;
}

/* ================================================================ modal state */

static struct {
    bool   active;
    char   buf[MENU_CMD_BUF];
    size_t len;
} g_cmd;

#if ATC_MENU_WIDGET_INPUT
static struct {
    bool                   active;
    const atc_menu_item_t *item;
    size_t                 index;
    char                   buf[MENU_INPUT_BUF];
    uint8_t                pos;
} g_input;
#endif

#if ATC_MENU_WIDGET_CHOICE
static struct {
    bool                   active;
    const atc_menu_item_t *item;
    size_t                 index;
    uint8_t                pending;
} g_choice;
#endif

/* ================================================================ footer */

/* Line one below the box bottom — where mode-specific footer hints land. */
static int footer_anchor_line(void) {
    int notes = nav_note_count() ? 1 + (int)nav_note_count() : 0;
    return MENU_HEADER_LINES + (int)nav_count() + notes + 2;
}

static void emit_footer(void) {
    render_park_cursor(footer_anchor_line(), false);
    menu_printf("%s", ANSI_CLR_BELOW);

    if (g_cmd.active) {
        menu_printf("%s", "\r\n" SYM_CMD_PROMPT);
    }
#if ATC_MENU_WIDGET_INPUT
    else if (g_input.active && g_input.item) {
        const atc_menu_item_t *it = g_input.item;
        if (it->input_type == ATC_INPUT_INT || it->input_type == ATC_INPUT_HEX)
            menu_printf("\r\n" ANSI_DIM
                "[Enter] commit  [Esc] cancel  [BS] erase    range: %ld..%ld"
                ANSI_RESET ANSI_EOL "\r\n",
                (long)it->input_min, (long)it->input_max);
        else
            menu_printf("\r\n" ANSI_DIM
                "[Enter] commit  [Esc] cancel  [BS] erase"
                ANSI_RESET ANSI_EOL "\r\n");
    }
#endif
#if ATC_MENU_WIDGET_CHOICE
    else if (g_choice.active && g_choice.item) {
        menu_printf("\r\n" ANSI_DIM
            "[%c] cycle  [Enter] commit  [Esc] cancel"
            ANSI_RESET ANSI_EOL "\r\n", g_choice.item->key);
    }
#endif
    else {
        render_default_footer(nav_depth() > 0);
    }

    menu_printf("%s", ANSI_CLR_BELOW);
    g_status_dirty = false;
}

/* ================================================================ public render */

void atc_menu_render(void) {
    if (!nav_items() || !g_port) return;
    render_all();
    emit_footer();
}

void atc_menu_status(const char *msg) {
    render_status_line(msg);
}

/* ================================================================ cmd mode */

static void cmd_reset(void) {
    g_cmd.active = false;
    g_cmd.len    = 0;
}

static void cmd_enter(void) {
    g_cmd.active = true;
    g_cmd.len    = 0;
    menu_printf("%s", "\r\n" SYM_CMD_PROMPT);
}

static void cmd_key(char k) {
    if (k == '\r' || k == '\n') {
        g_cmd.buf[g_cmd.len] = '\0';
        cmd_reset();
        if (g_port->cmd) g_port->cmd(g_cmd.buf);
        atc_menu_render();
        return;
    }
    if (k == KEY_ESC) {
        cmd_reset();
        menu_park_cursor();
        return;
    }
    if (k == KEY_BS || k == KEY_DEL) {
        if (g_cmd.len) {
            g_cmd.len--;
            menu_printf("%s", "\b \b");
        }
        return;
    }
    if (g_cmd.len < sizeof g_cmd.buf - 1) {
        g_cmd.buf[g_cmd.len++] = k;
        g_port->write(&k, 1);
    }
}

/* ================================================================ INPUT mode */

#if ATC_MENU_WIDGET_INPUT

static void input_reset(void) {
    g_input.active = false;
    g_input.pos    = 0;
    g_input.buf[0] = '\0';
    g_input.index  = 0;
    g_input.item   = NULL;
}

static void input_compose(char *out, size_t cap) {
    size_t pos = 0;
    int n = snprintf(out + pos, cap - pos, "%s", SYM_INPUT_PROMPT);
    if (n > 0 && (size_t)n < cap - pos) pos += (size_t)n;

    size_t copy = g_input.pos;
    if (copy > cap - pos - sizeof SYM_INPUT_CURSOR - 1)
        copy = cap - pos - sizeof SYM_INPUT_CURSOR - 1;
    memcpy(out + pos, g_input.buf, copy);
    pos += copy;

    n = snprintf(out + pos, cap - pos, "%s", SYM_INPUT_CURSOR);
    if (n > 0 && (size_t)n < cap - pos) pos += (size_t)n;
    out[pos] = '\0';
}

static void input_paint(void) {
    char edit[MENU_REGION_INPUT_EDIT_BUF];
    input_compose(edit, sizeof edit);
    render_row_input(g_input.index, edit);
}

static bool input_char_acceptable(atc_input_type_t t, char k) {
    switch (t) {
        case ATC_INPUT_INT:
            return (k >= '0' && k <= '9') || k == '-';
#if ATC_MENU_INPUT_FLOAT
        case ATC_INPUT_FLOAT:
            return (k >= '0' && k <= '9') || k == '-' || k == '.';
#endif
        case ATC_INPUT_HEX:
            return (k >= '0' && k <= '9') || (k >= 'a' && k <= 'f')
                || (k >= 'A' && k <= 'F') || k == 'x' || k == 'X';
        case ATC_INPUT_STR:
            return k >= 32 && k <= 126;
        default:
            return false;
    }
}

static bool input_validate(void) {
    if (g_input.pos == 0) {
        atc_menu_status("invalid: empty");
        return false;
    }
    g_input.buf[g_input.pos] = '\0';

    atc_input_type_t t = g_input.item->input_type;
    if (t == ATC_INPUT_STR) return true;

    char *end = NULL;
    long  v   = 0;
    errno = 0;
    if (t == ATC_INPUT_HEX)        v = strtol(g_input.buf, &end, 16);
    else if (t == ATC_INPUT_INT)   v = strtol(g_input.buf, &end, 10);
    else {
#if ATC_MENU_INPUT_FLOAT
        v = (long)strtod(g_input.buf, &end);
#else
        atc_menu_status("invalid: FLOAT input not enabled");
        return false;
#endif
    }

    if (errno == ERANGE || end == g_input.buf || (end && *end != '\0')) {
        atc_menu_status("invalid: parse error");
        return false;
    }
    if ((t == ATC_INPUT_INT || t == ATC_INPUT_HEX)
        && (v < g_input.item->input_min || v > g_input.item->input_max)) {
        char msg[64];
        snprintf(msg, sizeof msg, "out of range: %ld..%ld",
                 (long)g_input.item->input_min, (long)g_input.item->input_max);
        atc_menu_status(msg);
        return false;
    }
    return true;
}

static void input_enter(const atc_menu_item_t *it, size_t index) {
    g_input.active = true;
    g_input.pos    = 0;
    g_input.buf[0] = '\0';
    g_input.index  = index;
    g_input.item   = it;
    input_paint();
    emit_footer();
}

static void input_key(char k) {
    if (k == '\r' || k == '\n') {
        if (!input_validate()) return;
        bool   ok  = !g_input.item->input_commit
                  || g_input.item->input_commit(g_input.buf);
        size_t idx = g_input.index;
        if (ok) {
            input_reset();
            render_row(idx);
            emit_footer();
        } else {
            input_paint();
        }
        return;
    }
    if (k == KEY_ESC) {
        size_t idx = g_input.index;
        input_reset();
        render_row(idx);
        emit_footer();
        return;
    }
    if (k == KEY_BS || k == KEY_DEL) {
        if (g_input.pos) {
            g_input.pos--;
            g_input.buf[g_input.pos] = '\0';
            input_paint();
        }
        return;
    }
    if (g_input.pos < sizeof g_input.buf - 1
        && input_char_acceptable(g_input.item->input_type, k)) {
        g_input.buf[g_input.pos++] = k;
        g_input.buf[g_input.pos]   = '\0';
        input_paint();
    }
}

#endif /* ATC_MENU_WIDGET_INPUT */

/* ================================================================ CHOICE mode */

#if ATC_MENU_WIDGET_CHOICE

static void choice_reset(void) {
    g_choice.active  = false;
    g_choice.pending = 0;
    g_choice.index   = 0;
    g_choice.item    = NULL;
}

static void choice_paint(void) {
    const char *label = g_choice.item->choices[g_choice.pending]
                      ? g_choice.item->choices[g_choice.pending] : "";
    render_row_choice(g_choice.index, label);
}

static void choice_press(const atc_menu_item_t *it, size_t index) {
    if (!it->choice_idx || it->choice_count == 0) return;

    if (it->choice_commit) {
        g_choice.active  = true;
        g_choice.item    = it;
        g_choice.index   = index;
        g_choice.pending = *it->choice_idx;
        choice_paint();
        emit_footer();
    } else {
        *it->choice_idx = (uint8_t)((*it->choice_idx + 1) % it->choice_count);
        render_row(index);
    }
}

static void choice_key(char k) {
    if (k == '\r' || k == '\n') {
        *g_choice.item->choice_idx = g_choice.pending;
        atc_action_fn_t cb  = g_choice.item->choice_commit;
        size_t          idx = g_choice.index;
        choice_reset();
        if (cb) cb();
        render_row(idx);
        emit_footer();
        return;
    }
    if (k == KEY_ESC) {
        size_t idx = g_choice.index;
        choice_reset();
        render_row(idx);
        emit_footer();
        return;
    }
    if (k == g_choice.item->key) {
        g_choice.pending = (uint8_t)((g_choice.pending + 1) % g_choice.item->choice_count);
        choice_paint();
    }
}

#endif /* ATC_MENU_WIDGET_CHOICE */

/* ================================================================ row dispatch */

static void handle_row_key(const atc_menu_item_t *it, size_t index) {
    switch (it->type) {
        case ATC_ROW_SUBMENU:
            nav_push(it);
            atc_menu_render();
            return;
        case ATC_ROW_GROUP:
            if (it->action) it->action();
            render_group_span(index);
            return;
#if ATC_MENU_WIDGET_INPUT
        case ATC_ROW_INPUT:
            input_enter(it, index);
            return;
#endif
#if ATC_MENU_WIDGET_CHOICE
        case ATC_ROW_CHOICE:
            choice_press(it, index);
            return;
#endif
        default:
            if (it->action) it->action();
            render_row(index);
            return;
    }
}

/* ================================================================ init */

static const struct { char key; const char *role; } RESERVED[] = {
    { ATC_KEY_REFRESH, "built-in refresh" },
    { ATC_KEY_BACK,    "built-in back"    },
    { ATC_KEY_PATH,    "built-in path"    },
    { ATC_KEY_CMD,     "built-in cmd"     },
};

static const char *reserved_role(char k) {
    for (size_t i = 0; i < sizeof RESERVED / sizeof RESERVED[0]; i++)
        if (RESERVED[i].key == k) return RESERVED[i].role;
    return NULL;
}

void atc_menu_init(const atc_menu_table_t *table,
                   const atc_menu_port_t  *port,
                   const atc_menu_info_t  *info) {
    g_port         = port;
    g_info         = info;
    g_status_dirty = false;

    cmd_reset();
#if ATC_MENU_WIDGET_INPUT
    input_reset();
#endif
#if ATC_MENU_WIDGET_CHOICE
    choice_reset();
#endif
    nav_reset(table);

    if (!table || !table->items || !port) return;

    render_validate_notes(table->notes, table->note_count);

    const atc_menu_item_t *items = table->items;
    size_t                 count = table->count;
    for (size_t i = 0; i < count; i++) {
        const atc_menu_item_t *a = &items[i];

        if (a->key) {
            const char *role = reserved_role(a->key);
            if (role)
                menu_printf("WARN: key '%c' collides with %s\r\n", a->key, role);
        }
        render_validate_item(a);

        if (!a->key) continue;
        for (size_t j = i + 1; j < count; j++) {
            if (a->key == items[j].key)
                menu_printf("WARN: dup key '%c'\r\n", a->key);
        }
    }
}

/* ================================================================ handle_key */

void atc_menu_handle_key(char k) {
    if (!nav_items() || !g_port) return;

    if (g_cmd.active)    { cmd_key(k);    return; }
#if ATC_MENU_WIDGET_INPUT
    if (g_input.active)  { input_key(k);  return; }
#endif
#if ATC_MENU_WIDGET_CHOICE
    if (g_choice.active) { choice_key(k); return; }
#endif

    switch (k) {
        case ATC_KEY_REFRESH: atc_menu_render(); return;
        case ATC_KEY_CMD:
            if (g_port->cmd) {
                atc_menu_render();
                cmd_enter();
                return;
            }
            break;
        case ATC_KEY_BACK:
            if (nav_depth() > 0) { nav_pop(); atc_menu_render(); return; }
            break;
        case ATC_KEY_PATH:
            if (nav_depth() > 0) { nav_show_path(); return; }
            break;
    }

    g_status_dirty = false;
    const atc_menu_item_t *items = nav_items();
    size_t                 count = nav_count();
    for (size_t i = 0; i < count; i++) {
        if (items[i].key == k) { handle_row_key(&items[i], i); return; }
    }
}
