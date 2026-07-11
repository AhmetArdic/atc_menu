/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "atc_menu/atc_menu.h"
#include "mock_port.h"
#include "testing.h"

static void rd_ok(char *b, size_t n, atc_status_t *st) {
    (void)b; (void)n; *st = ATC_ST_OK;
}

static void noop(void) {}

int main(void) {
    /* A fully-populated table should produce no warnings. */
    static const atc_menu_item_t good[] = {
        { .type = ATC_ROW_GROUP,  .label = "Group", .unit = "" },
        { .type = ATC_ROW_VALUE,  .key = 't', .label = "Temp", .unit = "C",
          .read = rd_ok },
        { .type = ATC_ROW_STATE,  .key = 'L', .label = "LED",  .unit = "",
          .read = rd_ok, .action = noop },
        { .type = ATC_ROW_ACTION, .key = '1', .label = "Run",  .unit = "",
          .action = noop },
    };
    mock_reset();
    ATC_INIT_ITEMS(good, &mock_port);
    EXPECT_NOT_CONTAINS(mock_buffer(), "WARN");

    /* Rows missing their required callbacks each warn. */
    static const atc_menu_item_t bad_rows[] = {
        { .type = ATC_ROW_VALUE,  .key = 't', .label = "Temp", .unit = "" },
        { .type = ATC_ROW_STATE,  .key = 'L', .label = "LED",  .unit = "",
          .read = rd_ok },
        { .type = ATC_ROW_ACTION, .key = '1', .label = "Run",  .unit = "" },
        { .type = ATC_ROW_SUBMENU, .key = 's', .label = "S" },
    };
    mock_reset();
    ATC_INIT_ITEMS(bad_rows, &mock_port);
    EXPECT_CONTAINS(mock_buffer(), "ATC_ROW_VALUE 't' missing read");
    EXPECT_CONTAINS(mock_buffer(), "ATC_ROW_STATE 'L' missing read/action");
    EXPECT_CONTAINS(mock_buffer(), "ATC_ROW_ACTION '1' missing action");
    EXPECT_CONTAINS(mock_buffer(), "ATC_ROW_SUBMENU 's' missing submenu");

    /* Duplicate hotkeys must warn. */
    static const atc_menu_item_t dup[] = {
        { .type = ATC_ROW_ACTION, .key = '1', .label = "A", .unit = "",
          .action = noop },
        { .type = ATC_ROW_ACTION, .key = '1', .label = "B", .unit = "",
          .action = noop },
    };
    mock_reset();
    ATC_INIT_ITEMS(dup, &mock_port);
    EXPECT_CONTAINS(mock_buffer(), "dup key");

    /* Valid SUBMENU produces no warnings. */
    static const atc_menu_item_t leaf[] = {
        { .type = ATC_ROW_GROUP, .label = "Leaf" },
    };
    static const atc_menu_table_t leaf_tbl = {
        .items = leaf, .count = 1,
    };
    static const atc_menu_item_t good_sub[] = {
        { .type = ATC_ROW_SUBMENU, .key = 's', .label = "S",
          .submenu = &leaf_tbl },
    };
    mock_reset();
    ATC_INIT_ITEMS(good_sub, &mock_port);
    EXPECT_NOT_CONTAINS(mock_buffer(), "WARN");

    /* User row with key 'b' must warn (collides with built-in back). */
    static const atc_menu_item_t b_collide[] = {
        { .type = ATC_ROW_ACTION, .key = 'b', .label = "B", .action = noop },
    };
    mock_reset();
    ATC_INIT_ITEMS(b_collide, &mock_port);
    EXPECT_CONTAINS(mock_buffer(), "WARN");
    EXPECT_CONTAINS(mock_buffer(), "built-in back");

    /* Label/unit longer than their column must warn. */
    static const atc_menu_item_t long_label_unit[] = {
        { .type = ATC_ROW_VALUE, .key = 't',
          .label = "ThisLabelIsWayTooLongForTheLabelColumnToFit",
          .unit = "milligrams", .read = rd_ok },
    };
    mock_reset();
    ATC_INIT_ITEMS(long_label_unit, &mock_port);
    EXPECT_CONTAINS(mock_buffer(), "label");
    EXPECT_CONTAINS(mock_buffer(), "unit");

    /* Notes that exceed the inner width must warn (root table). */
    static const atc_menu_item_t long_note_items[] = {
        { .type = ATC_ROW_GROUP, .label = "G" },
        { .type = ATC_ROW_NOTE,
          .label = "ThisRootNoteIsDeliberatelyMuchLongerThanTheInnerBoxWidthSoItOverflowsAndShouldWarn" },
    };
    mock_reset();
    ATC_INIT_ITEMS(long_note_items, &mock_port);
    EXPECT_CONTAINS(mock_buffer(), "WARN");
    EXPECT_CONTAINS(mock_buffer(), "note exceeds");

    /* Long notes on a child SUBMENU table also warn at init. */
    static const atc_menu_item_t child_items[] = {
        { .type = ATC_ROW_GROUP, .label = "C" },
        { .type = ATC_ROW_NOTE,
          .label = "ThisChildNoteIsDeliberatelyMuchLongerThanTheInnerBoxWidthSoItOverflowsAndShouldWarn" },
    };
    static const atc_menu_table_t child_tbl = {
        .items = child_items, .count = sizeof child_items / sizeof child_items[0],
    };
    static const atc_menu_item_t parent_items[] = {
        { .type = ATC_ROW_SUBMENU, .key = 's', .label = "S",
          .submenu = &child_tbl },
    };
    mock_reset();
    ATC_INIT_ITEMS(parent_items, &mock_port);
    EXPECT_CONTAINS(mock_buffer(), "note exceeds");

    /* Notes that fit produce no note warnings. */
    static const atc_menu_item_t ok_note_items[] = {
        { .type = ATC_ROW_GROUP, .label = "G" },
        { .type = ATC_ROW_NOTE,  .label = "Short note." },
    };
    mock_reset();
    ATC_INIT_ITEMS(ok_note_items, &mock_port);
    EXPECT_NOT_CONTAINS(mock_buffer(), "note exceeds");

    /* A NOTE followed by a non-NOTE row must warn. */
    static const atc_menu_item_t misplaced_note[] = {
        { .type = ATC_ROW_NOTE,  .label = "Too early." },
        { .type = ATC_ROW_GROUP, .label = "G" },
    };
    mock_reset();
    ATC_INIT_ITEMS(misplaced_note, &mock_port);
    EXPECT_CONTAINS(mock_buffer(), "NOTE rows must be last");

    TEST_RESULT();
}
