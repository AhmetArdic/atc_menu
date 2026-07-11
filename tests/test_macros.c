/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "atc_menu/menu_macros.h"
#include "testing.h"

#include <stddef.h>

static void rd_dummy(char *b, size_t n, atc_menu_status_t *st) {
    (void)b; (void)n; (void)st;
}
static void act_dummy(void)             {}
static bool commit_dummy(const char *s) { (void)s; return true; }

static uint_least8_t choice_idx = 0;
static const char *choices[]  = { "A", "B", "C" };

ATC_MENU(leaf,
    ATC_GROUP ('r', "Leaf"),
    ATC_VALUE ('v', "V", "u", rd_dummy),
);

/* Keyless variants: same macros, hotkey argument omitted. */
ATC_MENU(bare,
    ATC_GROUP  ("Bare"),
    ATC_VALUE  ("Val",   "u", rd_dummy),
    ATC_STATE  ("State", rd_dummy, act_dummy),
    ATC_ACTION ("Act",   act_dummy),
    ATC_SUBMENU("More",  &leaf),
    ATC_BAR    ("Bar",   rd_dummy),
    ATC_CHOICE ("Pick",  choices, 3, &choice_idx, act_dummy),
    ATC_INPUT  ("In",    "u", rd_dummy,
                ATC_MENU_INPUT_INT, 0, 100, commit_dummy),
);

ATC_MENU(sample,
    ATC_GROUP  ("Header"),
    ATC_VALUE  ('v', "Val",    "u",  rd_dummy),
    ATC_STATE  ('s', "State",       rd_dummy, act_dummy),
    ATC_ACTION ('a', "Act",         act_dummy),
    ATC_SUBMENU('m', "More", &leaf),
    ATC_BAR    ('b', "Bar",         rd_dummy),
    ATC_CHOICE ('c', "Pick", choices, 3, &choice_idx, act_dummy),
    ATC_INPUT  ('i', "In",   "u",   rd_dummy,
                ATC_MENU_INPUT_INT, 0, 100, commit_dummy),
    ATC_NOTE   ("first note"),
);

int main(void) {
    /* Table-level fields. */
    EXPECT(sample.items == sample_items);
    EXPECT(sample.count == 9);

    EXPECT(leaf.items == leaf_items);
    EXPECT(leaf.count == 2);

    /* Each row macro lays down the expected type + key + label. */
    EXPECT(sample_items[0].type  == ATC_MENU_ROW_GROUP);
    EXPECT_STREQ(sample_items[0].label, "Header");

    EXPECT(sample_items[1].type  == ATC_MENU_ROW_VALUE);
    EXPECT(sample_items[1].key   == 'v');
    EXPECT(sample_items[1].read  == rd_dummy);
    EXPECT_STREQ(sample_items[1].unit, "u");

    EXPECT(sample_items[2].type   == ATC_MENU_ROW_STATE);
    EXPECT(sample_items[2].read   == rd_dummy);
    EXPECT(sample_items[2].action == act_dummy);

    EXPECT(sample_items[3].type   == ATC_MENU_ROW_ACTION);
    EXPECT(sample_items[3].action == act_dummy);

    EXPECT(sample_items[4].type     == ATC_MENU_ROW_SUBMENU);
    EXPECT(sample_items[4].submenu  == &leaf);

    EXPECT(sample_items[5].type == ATC_MENU_ROW_BAR);
    EXPECT(sample_items[5].read == rd_dummy);

    EXPECT(sample_items[6].type          == ATC_MENU_ROW_CHOICE);
    EXPECT(sample_items[6].choices       == choices);
    EXPECT(sample_items[6].choice_count  == 3);
    EXPECT(sample_items[6].choice_idx    == &choice_idx);
    EXPECT(sample_items[6].choice_commit == act_dummy);

    EXPECT(sample_items[7].type         == ATC_MENU_ROW_INPUT);
    EXPECT(sample_items[7].input_type   == ATC_MENU_INPUT_INT);
    EXPECT(sample_items[7].input_min    == 0);
    EXPECT(sample_items[7].input_max    == 100);
    EXPECT(sample_items[7].input_commit == commit_dummy);

    EXPECT(sample_items[8].type == ATC_MENU_ROW_NOTE);
    EXPECT_STREQ(sample_items[8].label, "first note");

    /* Keyed GROUP carries its refresh hotkey. */
    EXPECT(leaf_items[0].key == 'r');

    /* Keyless variants: key stays 0, payload fields still land. */
    EXPECT(bare.count == 8);
    for (size_t i = 0; i < bare.count; ++i)
        EXPECT(bare_items[i].key == 0);

    EXPECT(bare_items[0].type == ATC_MENU_ROW_GROUP);
    EXPECT(bare_items[1].type == ATC_MENU_ROW_VALUE);
    EXPECT(bare_items[1].read == rd_dummy);
    EXPECT_STREQ(bare_items[1].unit, "u");
    EXPECT(bare_items[2].type   == ATC_MENU_ROW_STATE);
    EXPECT(bare_items[2].action == act_dummy);
    EXPECT(bare_items[3].type   == ATC_MENU_ROW_ACTION);
    EXPECT(bare_items[4].type    == ATC_MENU_ROW_SUBMENU);
    EXPECT(bare_items[4].submenu == &leaf);
    EXPECT(bare_items[5].type == ATC_MENU_ROW_BAR);
    EXPECT(bare_items[6].type          == ATC_MENU_ROW_CHOICE);
    EXPECT(bare_items[6].choice_idx    == &choice_idx);
    EXPECT(bare_items[7].type         == ATC_MENU_ROW_INPUT);
    EXPECT(bare_items[7].input_max    == 100);
    EXPECT(bare_items[7].input_commit == commit_dummy);

    TEST_RESULT();
}
