/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "atc_menu/atc_menu.h"
#include "mock_port.h"
#include "testing.h"

#include <stdio.h>

static void rd_temp(char *b, size_t n, atc_status_t *st) {
    *st = ATC_ST_OK;
    if (b && n) snprintf(b, n, "42.0");
}

static void rd_led(char *b, size_t n, atc_status_t *st) {
    (void)b; (void)n; *st = ATC_ST_ON;
}

static void noop(void) {}

int main(void) {
    static const atc_menu_item_t items[] = {
        { .type = ATC_ROW_GROUP, .label = "Sensors", .unit = "" },
        { .type = ATC_ROW_VALUE, .key = 't', .label = "Temp", .unit = "C",
          .read = rd_temp },
        { .type = ATC_ROW_GROUP, .label = "Outputs", .unit = "" },
        { .type = ATC_ROW_STATE, .key = 'L', .label = "LED", .unit = "",
          .read = rd_led, .action = noop },
    };

    mock_reset();
    atc_menu_init(items, sizeof items / sizeof items[0], &mock_port);
    mock_reset();
    atc_menu_render();

    const char *out = mock_buffer();
    EXPECT(mock_len() > 0);

    /* Group labels appear. */
    EXPECT_CONTAINS(out, "Sensors");
    EXPECT_CONTAINS(out, "Outputs");

    /* Value rows show label, formatted value, and unit. */
    EXPECT_CONTAINS(out, "Temp");
    EXPECT_CONTAINS(out, "42.0");
    EXPECT_CONTAINS(out, "C");

    /* State rows render their status text. */
    EXPECT_CONTAINS(out, "LED");
    EXPECT_CONTAINS(out, "ON");

    /* Footer hints at the built-in keys. */
    EXPECT_CONTAINS(out, "[r]");
    EXPECT_CONTAINS(out, "[:]");

    TEST_RESULT();
}
