/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "atc_menu/atc_menu.h"
#include "mock_port.h"
#include "testing.h"

static int led_actions;
static int run_actions;

static void rd_led(char *b, size_t n, atc_status_t *st) {
    (void)b; (void)n; *st = ATC_ST_OFF;
}
static void rd_temp(char *b, size_t n, atc_status_t *st) {
    snprintf(b, n, "24.5"); *st = ATC_ST_OK;
}
static void act_led(void) { led_actions++; }
static void act_run(void) { run_actions++; }

int main(void) {
    led_actions = 0;
    run_actions = 0;

    static const atc_menu_item_t items[] = {
        { .type = ATC_ROW_STATE,  .key = 'L', .label = "LED", .unit = "",
          .read = rd_led, .action = act_led },
        { .type = ATC_ROW_ACTION, .key = '1', .label = "Run", .unit = "",
          .action = act_run },
        { .type = ATC_ROW_VALUE,  .key = 't', .label = "Temp", .unit = "C",
          .read = rd_temp },
        { .type = ATC_ROW_GROUP,  .key = 'g', .label = "Group", .unit = "" },
        { .type = ATC_ROW_VALUE,  .label = "T1", .unit = "C", .read = rd_temp },
        { .type = ATC_ROW_VALUE,  .label = "T2", .unit = "C", .read = rd_temp },
    };

    mock_reset();
    ATC_INIT_ITEMS(items, &mock_port);

    /* Hotkey dispatches to the matching row's action. */
    atc_menu_handle_key('L');
    EXPECT(led_actions == 1);

    atc_menu_handle_key('1');
    EXPECT(run_actions == 1);

    /* Unmapped keys are ignored. */
    atc_menu_handle_key('Q');
    EXPECT(led_actions == 1);
    EXPECT(run_actions == 1);

    /* VALUE row hotkey has no action but still repaints the row. */
    mock_reset();
    atc_menu_handle_key('t');
    size_t partial_len = mock_len();
    EXPECT(partial_len > 0);

    /* Partial repaint must be substantially smaller than a full repaint. */
    mock_reset();
    atc_menu_render();
    size_t full_len = mock_len();
    EXPECT(full_len > partial_len * 2);

    /* Group key repaints all rows in the span: bigger than one row,
     * smaller than the full menu. */
    mock_reset();
    atc_menu_handle_key('g');
    size_t group_len = mock_len();
    EXPECT(group_len > partial_len);
    EXPECT(group_len < full_len);

    /* 'r' triggers a render. */
    mock_reset();
    atc_menu_handle_key('r');
    EXPECT(mock_len() > 0);

    /* ':' starts command mode and a newline submits it. */
    mock_reset();
    atc_menu_handle_key(':');
    atc_menu_handle_key('p');
    atc_menu_handle_key('i');
    atc_menu_handle_key('n');
    atc_menu_handle_key('g');
    atc_menu_handle_key('\n');
    EXPECT(mock_cmd_calls() == 1);
    EXPECT_STREQ(mock_last_cmd(), "ping");

    /* ESC cancels command mode without firing the cmd handler. */
    mock_reset();
    atc_menu_handle_key(':');
    atc_menu_handle_key('a');
    atc_menu_handle_key(27);
    EXPECT(mock_cmd_calls() == 0);

    /* Back in normal mode, hotkeys work again. */
    atc_menu_handle_key('1');
    EXPECT(run_actions == 2);

    /* ---------------- Sub-menu navigation ----------------- */

    static const atc_menu_item_t leaf[] = {
        { .type = ATC_ROW_GROUP, .label = "Leaf" },
        { .type = ATC_ROW_VALUE, .label = "X", .unit = "C", .read = rd_temp },
    };
    static const atc_menu_table_t leaf_tbl = {
        .items = leaf, .count = sizeof leaf / sizeof leaf[0],
    };
    static const atc_menu_item_t mid[] = {
        { .type = ATC_ROW_GROUP,   .label = "Mid" },
        { .type = ATC_ROW_SUBMENU, .key = 'd', .label = "Deeper",
          .submenu = &leaf_tbl },
    };
    static const atc_menu_table_t mid_tbl = {
        .items = mid, .count = sizeof mid / sizeof mid[0],
    };
    static const atc_menu_item_t root[] = {
        { .type = ATC_ROW_GROUP,   .label = "Root" },
        { .type = ATC_ROW_SUBMENU, .key = 'm', .label = "Mid menu",
          .submenu = &mid_tbl },
        { .type = ATC_ROW_ACTION,  .key = '1', .label = "Run",
          .action = act_run },
    };

    mock_reset();
    ATC_INIT_ITEMS(root, &mock_port);
    mock_reset();
    atc_menu_handle_key('m');
    EXPECT_CONTAINS(mock_buffer(), "Mid");
    EXPECT_CONTAINS(mock_buffer(), "Deeper");
    EXPECT_NOT_CONTAINS(mock_buffer(), "Home \xe2\x80\xba ");

    mock_reset();
    atc_menu_handle_key('?');
    EXPECT_CONTAINS(mock_buffer(), "Home \xe2\x80\xba Mid menu"); /* Home › Mid menu */

    mock_reset();
    atc_menu_handle_key('b');
    EXPECT_CONTAINS(mock_buffer(), "Root");
    EXPECT_NOT_CONTAINS(mock_buffer(), "Home \xe2\x80\xba ");

    /* Built-in keys are no-ops at the root. */
    mock_reset();
    atc_menu_handle_key('b');
    EXPECT(mock_len() == 0);
    atc_menu_handle_key('?');
    EXPECT(mock_len() == 0);

    /* Two-deep nesting: '?' chains all labels. */
    mock_reset();
    atc_menu_handle_key('m');
    atc_menu_handle_key('d');
    EXPECT_CONTAINS(mock_buffer(), "Leaf");
    mock_reset();
    atc_menu_handle_key('?');
    EXPECT_CONTAINS(mock_buffer(), "Home \xe2\x80\xba Mid menu \xe2\x80\xba Deeper"); /* Home › Mid menu › Deeper */

    mock_reset();
    atc_menu_handle_key('b');
    EXPECT_CONTAINS(mock_buffer(), "Mid");
    EXPECT_CONTAINS(mock_buffer(), "Deeper");
    mock_reset();
    atc_menu_handle_key('b');
    EXPECT_CONTAINS(mock_buffer(), "Root");

    /* Old-table hotkey (post-pop) dispatches into the now-active root. */
    atc_menu_handle_key('1');
    EXPECT(run_actions == 3);

    /* Stack overflow: 5 levels with depth 4 must refuse the 5th. */
    static const atc_menu_item_t lvl5[] = {
        { .type = ATC_ROW_GROUP, .label = "L5" },
    };
    static const atc_menu_table_t lvl5_tbl = {
        .items = lvl5, .count = sizeof lvl5 / sizeof lvl5[0],
    };
    static const atc_menu_item_t lvl4[] = {
        { .type = ATC_ROW_SUBMENU, .key = 'n', .label = "L5",
          .submenu = &lvl5_tbl },
    };
    static const atc_menu_table_t lvl4_tbl = {
        .items = lvl4, .count = sizeof lvl4 / sizeof lvl4[0],
    };
    static const atc_menu_item_t lvl3[] = {
        { .type = ATC_ROW_SUBMENU, .key = 'n', .label = "L4",
          .submenu = &lvl4_tbl },
    };
    static const atc_menu_table_t lvl3_tbl = {
        .items = lvl3, .count = sizeof lvl3 / sizeof lvl3[0],
    };
    static const atc_menu_item_t lvl2[] = {
        { .type = ATC_ROW_SUBMENU, .key = 'n', .label = "L3",
          .submenu = &lvl3_tbl },
    };
    static const atc_menu_table_t lvl2_tbl = {
        .items = lvl2, .count = sizeof lvl2 / sizeof lvl2[0],
    };
    static const atc_menu_item_t lvl1[] = {
        { .type = ATC_ROW_SUBMENU, .key = 'n', .label = "L2",
          .submenu = &lvl2_tbl },
    };
    static const atc_menu_table_t lvl1_tbl = {
        .items = lvl1, .count = sizeof lvl1 / sizeof lvl1[0],
    };
    static const atc_menu_item_t lvl0[] = {
        { .type = ATC_ROW_SUBMENU, .key = 'n', .label = "L1",
          .submenu = &lvl1_tbl },
    };
    ATC_INIT_ITEMS(lvl0, &mock_port);
    atc_menu_handle_key('n');   /* depth 1 */
    atc_menu_handle_key('n');   /* depth 2 */
    atc_menu_handle_key('n');   /* depth 3 */
    atc_menu_handle_key('n');   /* depth 4 */
    mock_reset();
    atc_menu_handle_key('n');   /* depth 5: refused */
    EXPECT_CONTAINS(mock_buffer(), "WARN");
    EXPECT_CONTAINS(mock_buffer(), "stack full");

    TEST_RESULT();
}
