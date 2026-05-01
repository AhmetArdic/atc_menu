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
    atc_menu_init(good, sizeof good / sizeof good[0], &mock_port);
    EXPECT_NOT_CONTAINS(mock_buffer(), "WARN");

    /* VALUE without read must warn. */
    static const atc_menu_item_t bad_value[] = {
        { .type = ATC_ROW_VALUE, .key = 't', .label = "Temp", .unit = "" },
    };
    mock_reset();
    atc_menu_init(bad_value, 1, &mock_port);
    EXPECT_CONTAINS(mock_buffer(), "WARN");
    EXPECT_CONTAINS(mock_buffer(), "missing read");

    /* STATE without action must warn. */
    static const atc_menu_item_t bad_state[] = {
        { .type = ATC_ROW_STATE, .key = 'L', .label = "LED", .unit = "",
          .read = rd_ok },
    };
    mock_reset();
    atc_menu_init(bad_state, 1, &mock_port);
    EXPECT_CONTAINS(mock_buffer(), "WARN");

    /* ACTION without action must warn. */
    static const atc_menu_item_t bad_action[] = {
        { .type = ATC_ROW_ACTION, .key = '1', .label = "Run", .unit = "" },
    };
    mock_reset();
    atc_menu_init(bad_action, 1, &mock_port);
    EXPECT_CONTAINS(mock_buffer(), "WARN");
    EXPECT_CONTAINS(mock_buffer(), "missing action");

    /* Duplicate hotkeys must warn. */
    static const atc_menu_item_t dup[] = {
        { .type = ATC_ROW_ACTION, .key = '1', .label = "A", .unit = "",
          .action = noop },
        { .type = ATC_ROW_ACTION, .key = '1', .label = "B", .unit = "",
          .action = noop },
    };
    mock_reset();
    atc_menu_init(dup, 2, &mock_port);
    EXPECT_CONTAINS(mock_buffer(), "dup key");

    /* SUBMENU without a submenu pointer must warn. */
    static const atc_menu_item_t bad_sub[] = {
        { .type = ATC_ROW_SUBMENU, .key = 's', .label = "S" },
    };
    mock_reset();
    atc_menu_init(bad_sub, 1, &mock_port);
    EXPECT_CONTAINS(mock_buffer(), "WARN");
    EXPECT_CONTAINS(mock_buffer(), "missing submenu");

    /* Valid SUBMENU produces no warnings. */
    static const atc_menu_item_t leaf[] = {
        { .type = ATC_ROW_GROUP, .label = "Leaf" },
    };
    static const atc_menu_item_t good_sub[] = {
        { .type = ATC_ROW_SUBMENU, .key = 's', .label = "S",
          .submenu = leaf, .submenu_count = 1 },
    };
    mock_reset();
    atc_menu_init(good_sub, 1, &mock_port);
    EXPECT_NOT_CONTAINS(mock_buffer(), "WARN");

    /* User row with key 'b' must warn (collides with built-in back). */
    static const atc_menu_item_t b_collide[] = {
        { .type = ATC_ROW_ACTION, .key = 'b', .label = "B", .action = noop },
    };
    mock_reset();
    atc_menu_init(b_collide, 1, &mock_port);
    EXPECT_CONTAINS(mock_buffer(), "WARN");
    EXPECT_CONTAINS(mock_buffer(), "built-in back");

    /* Label/unit longer than their column must warn. */
    static const atc_menu_item_t long_label[] = {
        { .type = ATC_ROW_VALUE, .key = 't',
          .label = "ThisLabelIsWayTooLongForTheLabelColumnToFit",
          .unit = "C", .read = rd_ok },
    };
    mock_reset();
    atc_menu_init(long_label, 1, &mock_port);
    EXPECT_CONTAINS(mock_buffer(), "WARN");
    EXPECT_CONTAINS(mock_buffer(), "label");

    static const atc_menu_item_t long_unit[] = {
        { .type = ATC_ROW_VALUE, .key = 't', .label = "T",
          .unit = "milligrams", .read = rd_ok },
    };
    mock_reset();
    atc_menu_init(long_unit, 1, &mock_port);
    EXPECT_CONTAINS(mock_buffer(), "WARN");
    EXPECT_CONTAINS(mock_buffer(), "unit");

    TEST_RESULT();
}
