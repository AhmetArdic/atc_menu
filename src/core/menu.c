/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "atc_menu/atc_menu.h"

#include "core/internal.h"
#include "input/cmdmode.h"
#include "input/nav.h"
#include "render/ansi.h"
#include "render/render.h"
#include "widgets/widget.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const atc_menu_port_t *g_port;
static const atc_menu_info_t *g_info;
static bool                   g_status_dirty;

const atc_menu_port_t *menu_port(void) { return g_port; }
const atc_menu_info_t *menu_info(void) { return g_info; }

bool menu_status_dirty(void)            { return g_status_dirty; }
void menu_set_status_dirty(bool dirty)  { g_status_dirty = dirty; }

const char *reserved_key_role(char k) {
    switch (k) {
        case ATC_KEY_REFRESH: return "built-in refresh";
        case ATC_KEY_BACK:    return "built-in back";
        case ATC_KEY_PATH:    return "built-in path";
        case ATC_KEY_CMD:     return "built-in cmd";
    }
    return NULL;
}

int atc_menu_printf(const char *fmt, ...) {
    if (!g_port || !g_port->write) return 0;

    row_t   r;
    row_reset(&r);

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(r.buf, sizeof r.buf, fmt, ap);
    va_end(ap);

    if (n < 0) return n;
    r.len = (int)((size_t)n < sizeof r.buf ? (size_t)n : sizeof r.buf - 1);
    int written = r.len;
    row_flush(&r);
    return written;
}

void menu_park_cursor(void) {
    int below = render_header_lines(g_info) + (int)nav_count()
              + render_notes_lines(nav_note_count()) + 4;
    render_park_cursor(below, !g_status_dirty);
}

static void render_item(int zebra, const atc_menu_item_t *it) {
    const widget_ops_t *ops = widget_ops(it->type);
    if (ops && ops->render) ops->render(zebra, it);
}

void menu_render_row_at(size_t index) {
    int y = render_header_lines(g_info) + 1 + (int)index;
    render_park_cursor(y, false);
    atc_menu_printf("%s", ANSI_EOL);
    render_item((int)index, &nav_items()[index]);
    menu_park_cursor();
}

void menu_render_group_span(size_t start) {
    const atc_menu_item_t *items = nav_items();
    size_t                 count = nav_count();
    size_t                 end   = start + 1;
    while (end < count && items[end].type != ATC_ROW_GROUP) end++;

    int y = render_header_lines(g_info) + 1 + (int)start;
    render_park_cursor(y, false);
    atc_menu_printf("%s", ANSI_EOL);
    for (size_t i = start; i < end; i++) render_item((int)i, &items[i]);
    menu_park_cursor();
}

void atc_menu_set_info(const atc_menu_info_t *info) { g_info = info; }

void atc_menu_render(void) {
    if (!nav_items() || !g_port) return;

    atc_menu_printf("%s", ANSI_HOME);
    render_header(g_info);

    const atc_menu_item_t *items = nav_items();
    size_t                 count = nav_count();
    for (size_t i = 0; i < count; i++) render_item((int)i, &items[i]);

    render_notes_block(nav_notes(), nav_note_count());
    render_box_bottom();

    atc_menu_printf("%s", ANSI_CLR_BELOW);
    if (widget_input_active()) widget_input_render_footer();
    else                       render_default_footer(nav_depth() > 0);
    atc_menu_printf("%s", ANSI_CLR_BELOW);
    g_status_dirty = false;
}

void atc_menu_status(const char *msg) {
    render_status_line(msg);
}

void atc_menu_init(const atc_menu_table_t *table,
                   const atc_menu_port_t *port) {
    g_port         = port;
    g_status_dirty = false;

    cmdmode_reset();
    widget_input_reset();
    nav_reset(table);

    if (!table || !table->items || !port) return;

    render_validate_notes(table->notes, table->note_count);

    const atc_menu_item_t *items = table->items;
    size_t                 count = table->count;
    for (size_t i = 0; i < count; i++) {
        const atc_menu_item_t *a = &items[i];

        if (a->key) {
            const char *role = reserved_key_role(a->key);
            if (role)
                atc_menu_printf("WARN: key '%c' collides with %s\r\n",
                                a->key, role);
        }
        const widget_ops_t *ops = widget_ops(a->type);
        if (ops && ops->validate) ops->validate(a);

        if (!a->key) continue;
        for (size_t j = i + 1; j < count; j++) {
            if (a->key == items[j].key)
                atc_menu_printf("WARN: dup key '%c'\r\n", a->key);
        }
    }
}

void atc_menu_handle_key(char k) {
    if (!nav_items() || !g_port) return;

    if (cmdmode_active())       { cmdmode_key(k);     return; }
    if (widget_input_active())  { widget_input_key(k); return; }

    if (k == ATC_KEY_CMD && g_port->cmd) { cmdmode_enter(); return; }
    if (k == ATC_KEY_REFRESH)            { atc_menu_render(); return; }
    if (k == ATC_KEY_BACK && nav_depth() > 0) {
        nav_pop();
        atc_menu_render();
        return;
    }
    if (k == ATC_KEY_PATH && nav_depth() > 0) {
        nav_show_path();
        return;
    }

    g_status_dirty = false;
    const atc_menu_item_t *items = nav_items();
    size_t                 count = nav_count();
    for (size_t i = 0; i < count; i++) {
        if (items[i].key != k) continue;
        const widget_ops_t *ops = widget_ops(items[i].type);
        if (ops && ops->on_key) {
            ops->on_key(&items[i], i);
        } else {
            if (items[i].action) items[i].action();
            menu_render_row_at(i);
        }
        return;
    }
}
