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
    atc_menu_init(items, sizeof items / sizeof items[0], &mock_port);

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

    TEST_RESULT();
}
