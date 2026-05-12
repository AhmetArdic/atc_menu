/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "atc_menu/atc_menu_macros.h"
#include "testing.h"

#include <stddef.h>

static void rd_dummy(char *b, size_t n, atc_status_t *st) {
    (void)b; (void)n; (void)st;
}
static void act_dummy(void)             {}
static bool commit_dummy(const char *s) { (void)s; return true; }

static uint8_t     choice_idx = 0;
static const char *choices[]  = { "A", "B", "C" };

ATC_MENU(leaf, ATC_NO_NOTES,
    ATC_GROUP ("Leaf"),
    ATC_VALUE ('v', "V", "u", rd_dummy),
);

static const char *const sample_notes[] = {
    "first note",
    "second note",
};

ATC_MENU(sample, ATC_WITH_NOTES(sample_notes),
    ATC_GROUP  ("Header"),
    ATC_VALUE  ('v', "Val",    "u",  rd_dummy),
    ATC_STATE  ('s', "State",       rd_dummy, act_dummy),
    ATC_ACTION ('a', "Act",         act_dummy),
    ATC_SUBMENU('m', "More", &leaf),
    ATC_BAR    ('b', "Bar",         rd_dummy),
    ATC_CHOICE ('c', "Pick", choices, 3, &choice_idx, act_dummy),
    ATC_INPUT  ('i', "In",   "u",   rd_dummy,
                ATC_INPUT_INT, 0, 100, commit_dummy),
);

int main(void) {
    /* Table-level fields. */
    EXPECT(sample.items == sample_items);
    EXPECT(sample.count == 8);
    EXPECT(sample.notes == sample_notes);
    EXPECT(sample.note_count == 2);

    EXPECT(leaf.items == leaf_items);
    EXPECT(leaf.count == 2);
    EXPECT(leaf.notes == NULL);
    EXPECT(leaf.note_count == 0);

    /* Each row macro lays down the expected type + key + label. */
    EXPECT(sample_items[0].type  == ATC_ROW_GROUP);
    EXPECT_STREQ(sample_items[0].label, "Header");

    EXPECT(sample_items[1].type  == ATC_ROW_VALUE);
    EXPECT(sample_items[1].key   == 'v');
    EXPECT(sample_items[1].read  == rd_dummy);
    EXPECT_STREQ(sample_items[1].unit, "u");

    EXPECT(sample_items[2].type   == ATC_ROW_STATE);
    EXPECT(sample_items[2].read   == rd_dummy);
    EXPECT(sample_items[2].action == act_dummy);

    EXPECT(sample_items[3].type   == ATC_ROW_ACTION);
    EXPECT(sample_items[3].action == act_dummy);

    EXPECT(sample_items[4].type     == ATC_ROW_SUBMENU);
    EXPECT(sample_items[4].submenu  == &leaf);

    EXPECT(sample_items[5].type == ATC_ROW_BAR);
    EXPECT(sample_items[5].read == rd_dummy);

    EXPECT(sample_items[6].type          == ATC_ROW_CHOICE);
    EXPECT(sample_items[6].choices       == choices);
    EXPECT(sample_items[6].choice_count  == 3);
    EXPECT(sample_items[6].choice_idx    == &choice_idx);
    EXPECT(sample_items[6].choice_commit == act_dummy);

    EXPECT(sample_items[7].type         == ATC_ROW_INPUT);
    EXPECT(sample_items[7].input_type   == ATC_INPUT_INT);
    EXPECT(sample_items[7].input_min    == 0);
    EXPECT(sample_items[7].input_max    == 100);
    EXPECT(sample_items[7].input_commit == commit_dummy);

    TEST_RESULT();
}
