/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "atc_menu/atc_menu.h"
#include "testing.h"

#include <stddef.h>

static void rd_dummy(char *b, size_t n, atc_status_t *st) {
    (void)b; (void)n; (void)st;
}
static void act_dummy(void)              {}
static bool commit_dummy(const char *s)  { (void)s; return true; }

static atc_menu_item_t  child_items[2];
static atc_menu_table_t child;

static atc_menu_item_t  items[8];
static atc_menu_table_t tbl;

static uint8_t     choice_idx;
static const char *choices[]   = { "X", "Y", "Z" };

static const char *const some_notes[] = { "n1", "n2" };

int main(void) {
    /* begin() resets the table. */
    atc_menu_begin(&tbl, items, 8);
    EXPECT(tbl.items == items);
    EXPECT(tbl.count == 0);
    EXPECT(tbl.cap   == 8);
    EXPECT(tbl.notes == NULL);

    /* Build a child to reference via submenu. */
    atc_menu_begin(&child, child_items, 2);
    atc_menu_group(&child, "Child");
    atc_menu_value(&child, 'x', "X", "u", rd_dummy);
    atc_menu_end(&child);
    EXPECT(child.count == 2);
    EXPECT(child_items[0].type == ATC_ROW_GROUP);
    EXPECT(child_items[1].type == ATC_ROW_VALUE);

    /* Every row adder writes the right fields. */
    atc_menu_group   (&tbl, "Header");
    atc_menu_value   (&tbl, 'v', "Val",    "u", rd_dummy);
    atc_menu_state   (&tbl, 's', "State",       rd_dummy, act_dummy);
    atc_menu_action  (&tbl, 'a', "Act",         act_dummy);
    atc_menu_submenu (&tbl, 'm', "Sub",   &child);
    atc_menu_bar     (&tbl, 'b', "Bar",         rd_dummy);
    atc_menu_choice  (&tbl, 'c', "Pick",  choices, 3, &choice_idx, act_dummy);
    atc_menu_input   (&tbl, 'i', "In",    "u",  rd_dummy,
                      ATC_INPUT_INT, 0, 100, commit_dummy);
    atc_menu_notes   (&tbl, some_notes, 2);
    atc_menu_end     (&tbl);

    EXPECT(tbl.count      == 8);
    EXPECT(tbl.notes      == some_notes);
    EXPECT(tbl.note_count == 2);

    EXPECT(items[0].type  == ATC_ROW_GROUP);
    EXPECT_STREQ(items[0].label, "Header");

    EXPECT(items[1].type == ATC_ROW_VALUE);
    EXPECT(items[1].key  == 'v');
    EXPECT(items[1].read == rd_dummy);
    EXPECT_STREQ(items[1].unit, "u");

    EXPECT(items[2].type   == ATC_ROW_STATE);
    EXPECT(items[2].read   == rd_dummy);
    EXPECT(items[2].action == act_dummy);

    EXPECT(items[3].type   == ATC_ROW_ACTION);
    EXPECT(items[3].action == act_dummy);

    EXPECT(items[4].type    == ATC_ROW_SUBMENU);
    EXPECT(items[4].submenu == &child);

    EXPECT(items[5].type == ATC_ROW_BAR);
    EXPECT(items[5].read == rd_dummy);

    EXPECT(items[6].type          == ATC_ROW_CHOICE);
    EXPECT(items[6].choices       == choices);
    EXPECT(items[6].choice_count  == 3);
    EXPECT(items[6].choice_idx    == &choice_idx);
    EXPECT(items[6].choice_commit == act_dummy);

    EXPECT(items[7].type         == ATC_ROW_INPUT);
    EXPECT(items[7].input_type   == ATC_INPUT_INT);
    EXPECT(items[7].input_min    == 0);
    EXPECT(items[7].input_max    == 100);
    EXPECT(items[7].input_commit == commit_dummy);

    /* Capacity check: 9th add must be a no-op. */
    atc_menu_value(&tbl, 'z', "Overflow", "u", rd_dummy);
    EXPECT(tbl.count == 8);

    /* Rebuild path: begin() wipes count, allowing reuse. */
    atc_menu_begin(&tbl, items, 8);
    atc_menu_group(&tbl, "After rebuild");
    EXPECT(tbl.count == 1);
    EXPECT(items[0].type == ATC_ROW_GROUP);
    EXPECT_STREQ(items[0].label, "After rebuild");

    /* Slots get zero-initialised on each append, so a stale union field
     * from a previous build cannot leak through. */
    atc_menu_value(&tbl, 'k', "K", "u", rd_dummy);
    EXPECT(items[1].action  == NULL);
    EXPECT(items[1].submenu == NULL);

    TEST_RESULT();
}
