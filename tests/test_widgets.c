/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "atc_menu/atc_menu.h"
#include "mock_port.h"
#include "testing.h"

#include <stdint.h>
#include <stdlib.h>

static int g_bar_pct;
static atc_status_t g_bar_status;

static void rd_bar(char *b, size_t n, atc_status_t *st) {
    snprintf(b, n, "%d", g_bar_pct);
    *st = g_bar_status;
}

static const char *choices_3[] = { "ECO", "NORMAL", "TURBO" };
static uint8_t     choice_idx_3;

static int  g_commit_calls;
static char g_commit_last[32];
static int  g_input_value;

static void rd_input(char *b, size_t n, atc_status_t *st) {
    snprintf(b, n, "%d", g_input_value);
    *st = ATC_ST_OK;
}

static bool commit_input(const char *s) {
    g_commit_calls++;
    snprintf(g_commit_last, sizeof g_commit_last, "%s", s);
    g_input_value = atoi(s);
    return true;
}

static int g_reject_commit_calls;
static bool commit_reject(const char *s) {
    (void)s; g_reject_commit_calls++;
    return false;
}

int main(void) {
    /* ---------- BAR rendering ---------- */
    static const atc_menu_item_t bar_items[] = {
        { .type = ATC_ROW_BAR, .key = 'b', .label = "Battery", .read = rd_bar },
    };

    g_bar_pct = 0;  g_bar_status = ATC_ST_ERR;
    mock_reset();
    ATC_INIT_ITEMS(bar_items, &mock_port);
    atc_menu_render();
    EXPECT_CONTAINS(mock_buffer(), "[........]");
    EXPECT_CONTAINS(mock_buffer(), "0 %");
    EXPECT_CONTAINS(mock_buffer(), "ERR");

    g_bar_pct = 50;  g_bar_status = ATC_ST_WARN;
    mock_reset();
    atc_menu_render();
    EXPECT_CONTAINS(mock_buffer(), "[####....]");
    EXPECT_CONTAINS(mock_buffer(), "50 %");
    EXPECT_CONTAINS(mock_buffer(), "WARN");

    g_bar_pct = 100;  g_bar_status = ATC_ST_OK;
    mock_reset();
    atc_menu_render();
    EXPECT_CONTAINS(mock_buffer(), "[########]");
    EXPECT_CONTAINS(mock_buffer(), "100 %");
    EXPECT_CONTAINS(mock_buffer(), "OK");

    /* Out-of-range percent clamps; negative -> 0 cells, >100 -> 8 cells. */
    g_bar_pct = -10;  g_bar_status = ATC_ST_OK;
    mock_reset();
    atc_menu_render();
    EXPECT_CONTAINS(mock_buffer(), "[........]");

    g_bar_pct = 250;  g_bar_status = ATC_ST_OK;
    mock_reset();
    atc_menu_render();
    EXPECT_CONTAINS(mock_buffer(), "[########]");

    /* ---------- CHOICE rendering & cycling ---------- */
    choice_idx_3 = 0;
    static const atc_menu_item_t choice_items[] = {
        { .type = ATC_ROW_CHOICE, .key = 'm', .label = "Mode",
          .choices = choices_3, .choice_count = 3, .choice_idx = &choice_idx_3 },
    };

    mock_reset();
    ATC_INIT_ITEMS(choice_items, &mock_port);
    atc_menu_render();
    EXPECT_CONTAINS(mock_buffer(), "ECO");
    EXPECT_CONTAINS(mock_buffer(), "[");

    /* First press: ECO -> NORMAL */
    mock_reset();
    atc_menu_handle_key('m');
    EXPECT(choice_idx_3 == 1);
    EXPECT_CONTAINS(mock_buffer(), "NORMAL");

    /* Second press: NORMAL -> TURBO */
    mock_reset();
    atc_menu_handle_key('m');
    EXPECT(choice_idx_3 == 2);
    EXPECT_CONTAINS(mock_buffer(), "TURBO");

    /* Third press: TURBO -> ECO (modulo) */
    mock_reset();
    atc_menu_handle_key('m');
    EXPECT(choice_idx_3 == 0);
    EXPECT_CONTAINS(mock_buffer(), "ECO");

    /* CHOICE with overlong choice string warns at init. */
    static const char *bad_choices[] = { "OK", "TOO_LONG_STRING" };
    static uint8_t bad_idx;
    static const atc_menu_item_t bad_choice_items[] = {
        { .type = ATC_ROW_CHOICE, .key = 'x', .label = "Bad",
          .choices = bad_choices, .choice_count = 2, .choice_idx = &bad_idx },
    };
    mock_reset();
    ATC_INIT_ITEMS(bad_choice_items, &mock_port);
    EXPECT_CONTAINS(mock_buffer(), "WARN");
    EXPECT_CONTAINS(mock_buffer(), "exceeds 6 cols");

    /* CHOICE missing pointer warns. */
    static const atc_menu_item_t missing_choice_items[] = {
        { .type = ATC_ROW_CHOICE, .key = 'y', .label = "Bad" },
    };
    mock_reset();
    ATC_INIT_ITEMS(missing_choice_items, &mock_port);
    EXPECT_CONTAINS(mock_buffer(), "missing choices/idx");

    /* ---------- INPUT: open editor, type, commit ---------- */
    g_input_value = 50;
    g_commit_calls = 0;
    g_commit_last[0] = '\0';

    static const atc_menu_item_t input_items[] = {
        { .type = ATC_ROW_INPUT, .key = 'd', .label = "PWM Duty", .unit = "%",
          .read = rd_input, .input_type = ATC_INPUT_INT,
          .input_min = 0, .input_max = 100, .input_commit = commit_input },
    };

    mock_reset();
    ATC_INIT_ITEMS(input_items, &mock_port);
    atc_menu_render();
    EXPECT_CONTAINS(mock_buffer(), "PWM Duty");
    EXPECT_CONTAINS(mock_buffer(), "[r] refresh");

    /* Press 'd' to open input mode; footer should change. */
    mock_reset();
    atc_menu_handle_key('d');
    EXPECT_CONTAINS(mock_buffer(), "[Enter] commit");
    EXPECT_CONTAINS(mock_buffer(), "range: 0..100");

    /* Type '7' '5' then Enter -> commit called with "75". */
    atc_menu_handle_key('7');
    atc_menu_handle_key('5');
    atc_menu_handle_key('\r');
    EXPECT(g_commit_calls == 1);
    EXPECT_STREQ(g_commit_last, "75");
    EXPECT(g_input_value == 75);

    /* After commit, footer reverts to normal hints. */
    mock_reset();
    atc_menu_render();
    EXPECT_CONTAINS(mock_buffer(), "[r] refresh");
    EXPECT_NOT_CONTAINS(mock_buffer(), "[Enter] commit");

    /* ---------- INPUT: out-of-range rejection ---------- */
    g_commit_calls = 0;
    g_commit_last[0] = '\0';
    atc_menu_handle_key('d');           /* enter input mode */
    atc_menu_handle_key('5');
    atc_menu_handle_key('0');
    atc_menu_handle_key('0');           /* "500" — over 100 */
    mock_reset();
    atc_menu_handle_key('\r');          /* validate fails */
    EXPECT(g_commit_calls == 0);
    EXPECT_CONTAINS(mock_buffer(), "out of range");

    /* Backspace twice clears "00", then "5" + Enter commits "5". */
    atc_menu_handle_key(8);             /* BS */
    atc_menu_handle_key(8);             /* BS */
    atc_menu_handle_key('\r');
    EXPECT(g_commit_calls == 1);
    EXPECT_STREQ(g_commit_last, "5");
    EXPECT(g_input_value == 5);

    /* ---------- INPUT: Esc cancels ---------- */
    g_commit_calls = 0;
    atc_menu_handle_key('d');
    atc_menu_handle_key('9');
    atc_menu_handle_key('9');
    atc_menu_handle_key(27);            /* Esc */
    EXPECT(g_commit_calls == 0);
    EXPECT(g_input_value == 5);         /* unchanged */

    /* Subsequent 'd' opens fresh editor (buffer cleared). */
    mock_reset();
    atc_menu_handle_key('d');
    EXPECT_CONTAINS(mock_buffer(), "[Enter] commit");
    /* Cancel again to leave clean state. */
    atc_menu_handle_key(27);

    /* ---------- INPUT: rejecting commit keeps editor open ---------- */
    /* Use key 'k' — 'r' would collide with the built-in refresh shortcut. */
    static const atc_menu_item_t reject_items[] = {
        { .type = ATC_ROW_INPUT, .key = 'k', .label = "Locked", .unit = "",
          .read = rd_input, .input_type = ATC_INPUT_INT,
          .input_min = 0, .input_max = 100, .input_commit = commit_reject },
    };
    g_reject_commit_calls = 0;
    mock_reset();
    ATC_INIT_ITEMS(reject_items, &mock_port);
    atc_menu_handle_key('k');           /* enter input mode */
    atc_menu_handle_key('5');
    atc_menu_handle_key('0');
    atc_menu_handle_key('\r');
    EXPECT(g_reject_commit_calls == 1);
    /* Editor still open → Esc cleans up (otherwise next test inherits state). */
    atc_menu_handle_key(27);

    /* ---------- Init validation: BAR / INPUT missing required callbacks ---------- */
    static const atc_menu_item_t bar_no_read[] = {
        { .type = ATC_ROW_BAR, .key = 'B', .label = "B" },
    };
    mock_reset();
    ATC_INIT_ITEMS(bar_no_read, &mock_port);
    EXPECT_CONTAINS(mock_buffer(), "ATC_ROW_BAR");
    EXPECT_CONTAINS(mock_buffer(), "missing read");

    static const atc_menu_item_t input_no_commit[] = {
        { .type = ATC_ROW_INPUT, .key = 'I', .label = "I", .unit = "",
          .read = rd_input, .input_type = ATC_INPUT_INT,
          .input_min = 0, .input_max = 100 },
    };
    mock_reset();
    ATC_INIT_ITEMS(input_no_commit, &mock_port);
    EXPECT_CONTAINS(mock_buffer(), "ATC_ROW_INPUT");
    EXPECT_CONTAINS(mock_buffer(), "missing read/input_commit");

    /* ---------- Init validation: every reserved system key warns ---------- */
    static const atc_menu_item_t reserved[] = {
        { .type = ATC_ROW_VALUE, .key = 'r', .label = "R", .read = rd_input },
        { .type = ATC_ROW_VALUE, .key = 'b', .label = "B", .read = rd_input },
        { .type = ATC_ROW_VALUE, .key = '?', .label = "P", .read = rd_input },
        { .type = ATC_ROW_VALUE, .key = ':', .label = "C", .read = rd_input },
    };
    mock_reset();
    ATC_INIT_ITEMS(reserved, &mock_port);
    EXPECT_CONTAINS(mock_buffer(), "'r' collides");
    EXPECT_CONTAINS(mock_buffer(), "'b' collides");
    EXPECT_CONTAINS(mock_buffer(), "'?' collides");
    EXPECT_CONTAINS(mock_buffer(), "':' collides");

    TEST_RESULT();
}
