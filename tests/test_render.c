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

    /* Footer at root must NOT advertise back. */
    EXPECT_NOT_CONTAINS(out, "[b]");

    /* ---------------- Sub-menu render ----------------- */

    static const atc_menu_item_t sub[] = {
        { .type = ATC_ROW_GROUP, .label = "SubGroup" },
        { .type = ATC_ROW_VALUE, .label = "Y", .unit = "V", .read = rd_temp },
    };
    static const atc_menu_item_t with_sub[] = {
        { .type = ATC_ROW_GROUP,   .label = "Top" },
        { .type = ATC_ROW_VALUE,   .key = 't', .label = "Temp", .unit = "C",
          .read = rd_temp },
        { .type = ATC_ROW_SUBMENU, .key = 'i', .label = "Inner",
          .submenu = sub, .submenu_count = sizeof sub / sizeof sub[0] },
    };

    mock_reset();
    atc_menu_init(with_sub, sizeof with_sub / sizeof with_sub[0], &mock_port);
    mock_reset();
    atc_menu_render();
    out = mock_buffer();

    /* SUBMENU label and the '> ' marker both appear (ANSI escapes are
     * interleaved between them, so we can't grep for them adjacently). */
    EXPECT_CONTAINS(out, "Inner");
    EXPECT_CONTAINS(out, "> ");
    /* Path indicator hidden at root. */
    EXPECT_NOT_CONTAINS(out, "Home > ");

    mock_reset();
    atc_menu_handle_key('i');
    out = mock_buffer();
    EXPECT_CONTAINS(out, "SubGroup");
    EXPECT_CONTAINS(out, "[b]");
    EXPECT_CONTAINS(out, "[?]");

    mock_reset();
    atc_menu_handle_key('?');
    EXPECT_CONTAINS(mock_buffer(), "Home > Inner");

    /* Two-level: '?' chains parent + active labels. */
    static const atc_menu_item_t deep_leaf[] = {
        { .type = ATC_ROW_GROUP, .label = "DeepLeaf" },
    };
    static const atc_menu_item_t deep_mid[] = {
        { .type = ATC_ROW_SUBMENU, .key = 'l', .label = "Leaf",
          .submenu = deep_leaf,
          .submenu_count = sizeof deep_leaf / sizeof deep_leaf[0] },
    };
    static const atc_menu_item_t deep_root[] = {
        { .type = ATC_ROW_SUBMENU, .key = 'm', .label = "Middle",
          .submenu = deep_mid,
          .submenu_count = sizeof deep_mid / sizeof deep_mid[0] },
    };
    atc_menu_init(deep_root, sizeof deep_root / sizeof deep_root[0],
                  &mock_port);
    atc_menu_handle_key('m');
    mock_reset();
    atc_menu_handle_key('l');
    EXPECT_CONTAINS(mock_buffer(), "DeepLeaf");
    EXPECT_NOT_CONTAINS(mock_buffer(), "Home > ");
    mock_reset();
    atc_menu_handle_key('?');
    EXPECT_CONTAINS(mock_buffer(), "Home > Middle > Leaf");

    atc_menu_init(items, sizeof items / sizeof items[0], &mock_port);
    mock_reset();
    atc_menu_handle_key('?');
    EXPECT(mock_len() == 0);

    /* Oversize label/unit are truncated at their column boundary. */
    static const atc_menu_item_t over[] = {
        { .type = ATC_ROW_VALUE, .key = 'x',
          .label = "ABCDEFGHIJKLMNOPQRSTUVWX_TAILSHOULDDISAPPEAR",
          .unit  = "kgPAS_TAIL", .read = rd_temp },
    };
    atc_menu_init(over, 1, &mock_port);
    mock_reset();
    atc_menu_render();
    out = mock_buffer();
    EXPECT_CONTAINS(out, "ABCDEFGHIJKLMNOPQRSTUVWX");
    EXPECT_NOT_CONTAINS(out, "TAILSHOULDDISAPPEAR");
    EXPECT_NOT_CONTAINS(out, "kgPAS_TAIL");

    TEST_RESULT();
}
