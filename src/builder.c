/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "atc_menu/atc_menu.h"

#include <string.h>

/* The builder writes through `items`, which the public type qualifies as
 * `const` so static-tables can live in .rodata. Casting away const here
 * is safe because atc_menu_begin() is only ever called with a writable
 * buffer the caller has provided. */
static atc_menu_item_t *append(atc_menu_table_t *t) {
    if (!t || !t->items || t->count >= t->cap) return NULL;
    atc_menu_item_t *items = (atc_menu_item_t *)t->items;
    atc_menu_item_t *slot  = &items[t->count++];
    memset(slot, 0, sizeof *slot);
    return slot;
}

void atc_menu_begin(atc_menu_table_t *t, atc_menu_item_t *items, size_t cap) {
    if (!t) return;
    t->items      = items;
    t->count      = 0;
    t->cap        = cap;
    t->notes      = NULL;
    t->note_count = 0;
}

void atc_menu_end(atc_menu_table_t *t) { (void)t; }

void atc_menu_notes(atc_menu_table_t *t, const char *const *notes, size_t n) {
    if (!t) return;
    t->notes      = notes;
    t->note_count = n;
}

void atc_menu_group(atc_menu_table_t *t, const char *label) {
    atc_menu_item_t *it = append(t);
    if (!it) return;
    it->type  = ATC_ROW_GROUP;
    it->label = label;
}

void atc_menu_value(atc_menu_table_t *t, char k, const char *label,
                    const char *unit, atc_read_fn_t read) {
    atc_menu_item_t *it = append(t);
    if (!it) return;
    it->type  = ATC_ROW_VALUE;
    it->key   = k;
    it->label = label;
    it->unit  = unit;
    it->read  = read;
}

void atc_menu_state(atc_menu_table_t *t, char k, const char *label,
                    atc_read_fn_t read, atc_action_fn_t action) {
    atc_menu_item_t *it = append(t);
    if (!it) return;
    it->type   = ATC_ROW_STATE;
    it->key    = k;
    it->label  = label;
    it->read   = read;
    it->action = action;
}

void atc_menu_action(atc_menu_table_t *t, char k, const char *label,
                     atc_action_fn_t action) {
    atc_menu_item_t *it = append(t);
    if (!it) return;
    it->type   = ATC_ROW_ACTION;
    it->key    = k;
    it->label  = label;
    it->action = action;
}

void atc_menu_submenu(atc_menu_table_t *t, char k, const char *label,
                      const atc_menu_table_t *sub) {
    atc_menu_item_t *it = append(t);
    if (!it) return;
    it->type    = ATC_ROW_SUBMENU;
    it->key     = k;
    it->label   = label;
    it->submenu = sub;
}

void atc_menu_bar(atc_menu_table_t *t, char k, const char *label,
                  atc_read_fn_t read) {
    atc_menu_item_t *it = append(t);
    if (!it) return;
    it->type  = ATC_ROW_BAR;
    it->key   = k;
    it->label = label;
    it->read  = read;
}

void atc_menu_choice(atc_menu_table_t *t, char k, const char *label,
                     const char **choices, uint8_t count,
                     uint8_t *idx_storage, atc_action_fn_t commit) {
    atc_menu_item_t *it = append(t);
    if (!it) return;
    it->type          = ATC_ROW_CHOICE;
    it->key           = k;
    it->label         = label;
    it->choices       = choices;
    it->choice_count  = count;
    it->choice_idx    = idx_storage;
    it->choice_commit = commit;
}

void atc_menu_input(atc_menu_table_t *t, char k, const char *label,
                    const char *unit, atc_read_fn_t read,
                    atc_input_type_t type, int32_t min, int32_t max,
                    atc_input_fn_t commit) {
    atc_menu_item_t *it = append(t);
    if (!it) return;
    it->type         = ATC_ROW_INPUT;
    it->key          = k;
    it->label        = label;
    it->unit         = unit;
    it->read         = read;
    it->input_type   = type;
    it->input_min    = min;
    it->input_max    = max;
    it->input_commit = commit;
}

void atc_menu_log_view(atc_menu_table_t *t, char k, const char *label,
                       atc_log_ring_t *ring) {
    atc_menu_item_t *it = append(t);
    if (!it) return;
    it->type     = ATC_ROW_LOG_VIEW;
    it->key      = k;
    it->label    = label;
    it->log_ring = ring;
}
