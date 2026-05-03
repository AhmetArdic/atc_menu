/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "input/nav.h"

#include "render/render.h"

#include <stdio.h>

typedef struct {
    const atc_menu_table_t *table;
    const char             *label;
} nav_frame_t;

static const atc_menu_table_t *g_table;
static const char             *g_active_label;

static nav_frame_t g_stack[ATC_MENU_STACK_DEPTH];
static size_t      g_stack_depth;

void nav_reset(const atc_menu_table_t *root) {
    g_table        = root;
    g_active_label = NULL;
    g_stack_depth  = 0;
}

const atc_menu_item_t *nav_items(void) {
    return g_table ? g_table->items : NULL;
}
size_t nav_count(void) {
    return g_table ? g_table->count : 0;
}
size_t nav_depth(void) { return g_stack_depth; }

const char *const *nav_notes(void) {
    return g_table ? g_table->notes : NULL;
}
size_t nav_note_count(void) {
    return g_table ? g_table->note_count : 0;
}

void nav_push(const atc_menu_item_t *opener) {
    const atc_menu_table_t *t = opener->submenu;
    if (!t || !t->items || t->count == 0) {
        atc_menu_printf("WARN: SUBMENU '%c' has no submenu table\r\n", opener->key);
        return;
    }
    if (g_stack_depth >= ATC_MENU_STACK_DEPTH) {
        atc_menu_printf("WARN: nav stack full (depth %d)\r\n", ATC_MENU_STACK_DEPTH);
        return;
    }
    g_stack[g_stack_depth].table = g_table;
    g_stack[g_stack_depth].label = g_active_label;
    g_stack_depth++;
    g_table        = t;
    g_active_label = opener->label;
}

void nav_pop(void) {
    if (g_stack_depth == 0) return;
    g_stack_depth--;
    g_table        = g_stack[g_stack_depth].table;
    g_active_label = g_stack[g_stack_depth].label;
}

void nav_show_path(void) {
    char   b[MENU_ROW_BUF];
    size_t n = 0;
    int    w = snprintf(b, sizeof b, "%s", SYM_CRUMB_ROOT);
    if (w > 0) n = (size_t)w < sizeof b ? (size_t)w : sizeof b - 1;

    for (size_t i = 0; i <= g_stack_depth && n + 1 < sizeof b; i++) {
        const char *lbl = (i < g_stack_depth) ? g_stack[i].label : g_active_label;
        if (!lbl) continue;
        w = snprintf(b + n, sizeof b - n, "%s%s", SYM_CRUMB, lbl);
        if (w > 0) n += (size_t)w < sizeof b - n ? (size_t)w : sizeof b - n - 1;
    }

    atc_menu_printf("\r\n" ANSI_DIM "%.*s" ANSI_EOL ANSI_RESET "\r\n", (int)n, b);
    menu_set_status_dirty(true);
}
