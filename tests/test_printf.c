/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "atc_menu/atc_menu.h"
#include "mock_port.h"
#include "testing.h"

int main(void) {
    static const atc_menu_item_t tbl[] = {
        { .type = ATC_ROW_GROUP, .label = "X", .unit = "" },
    };

    /* No port configured: printf returns 0. */
    ATC_INIT_ITEMS(tbl, NULL);
    EXPECT(atc_menu_printf("hello") == 0);

    /* Port configured: formatted output ends up in the buffer. */
    mock_reset();
    ATC_INIT_ITEMS(tbl, &mock_port);
    mock_reset();

    int n = atc_menu_printf("hello %s %d", "world", 42);
    EXPECT(n == (int)strlen("hello world 42"));
    EXPECT_CONTAINS(mock_buffer(), "hello world 42");

    /* Status message includes our text. */
    mock_reset();
    atc_menu_status("doing thing");
    EXPECT_CONTAINS(mock_buffer(), "doing thing");

    TEST_RESULT();
}
