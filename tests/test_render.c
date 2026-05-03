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
    ATC_INIT_ITEMS(items, &mock_port);
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

    /* State rows render their status glyph. */
    EXPECT_CONTAINS(out, "LED");
    EXPECT_CONTAINS(out, "\xe2\x97\x8f"); /* ● (ON) */

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
    static const atc_menu_table_t sub_tbl = {
        .items = sub, .count = sizeof sub / sizeof sub[0],
    };
    static const atc_menu_item_t with_sub[] = {
        { .type = ATC_ROW_GROUP,   .label = "Top" },
        { .type = ATC_ROW_VALUE,   .key = 't', .label = "Temp", .unit = "C",
          .read = rd_temp },
        { .type = ATC_ROW_SUBMENU, .key = 'i', .label = "Inner",
          .submenu = &sub_tbl },
    };

    mock_reset();
    ATC_INIT_ITEMS(with_sub, &mock_port);
    mock_reset();
    atc_menu_render();
    out = mock_buffer();

    /* SUBMENU label and the '❯ ' marker both appear (ANSI escapes are
     * interleaved between them, so we can't grep for them adjacently). */
    EXPECT_CONTAINS(out, "Inner");
    EXPECT_CONTAINS(out, "\xe2\x9d\xaf "); /* ❯ submenu marker */
    /* Path indicator hidden at root. */
    EXPECT_NOT_CONTAINS(out, "Home \xe2\x80\xba "); /* Home › */

    mock_reset();
    atc_menu_handle_key('i');
    out = mock_buffer();
    EXPECT_CONTAINS(out, "SubGroup");
    EXPECT_CONTAINS(out, "[b]");
    EXPECT_CONTAINS(out, "[?]");

    mock_reset();
    atc_menu_handle_key('?');
    EXPECT_CONTAINS(mock_buffer(), "Home \xe2\x80\xba Inner"); /* Home › Inner */

    /* Two-level: '?' chains parent + active labels. */
    static const atc_menu_item_t deep_leaf[] = {
        { .type = ATC_ROW_GROUP, .label = "DeepLeaf" },
    };
    static const atc_menu_table_t deep_leaf_tbl = {
        .items = deep_leaf, .count = sizeof deep_leaf / sizeof deep_leaf[0],
    };
    static const atc_menu_item_t deep_mid[] = {
        { .type = ATC_ROW_SUBMENU, .key = 'l', .label = "Leaf",
          .submenu = &deep_leaf_tbl },
    };
    static const atc_menu_table_t deep_mid_tbl = {
        .items = deep_mid, .count = sizeof deep_mid / sizeof deep_mid[0],
    };
    static const atc_menu_item_t deep_root[] = {
        { .type = ATC_ROW_SUBMENU, .key = 'm', .label = "Middle",
          .submenu = &deep_mid_tbl },
    };
    ATC_INIT_ITEMS(deep_root, &mock_port);
    atc_menu_handle_key('m');
    mock_reset();
    atc_menu_handle_key('l');
    EXPECT_CONTAINS(mock_buffer(), "DeepLeaf");
    EXPECT_NOT_CONTAINS(mock_buffer(), "Home \xe2\x80\xba "); /* Home › */
    mock_reset();
    atc_menu_handle_key('?');
    EXPECT_CONTAINS(mock_buffer(), "Home \xe2\x80\xba Middle \xe2\x80\xba Leaf"); /* Home › Middle › Leaf */

    ATC_INIT_ITEMS(items, &mock_port);
    mock_reset();
    atc_menu_handle_key('?');
    EXPECT(mock_len() == 0);

    /* ---------------- Notes block ----------------- */
    static const atc_menu_item_t leaf_with_notes_items[] = {
        { .type = ATC_ROW_GROUP, .label = "Leaf" },
        { .type = ATC_ROW_VALUE, .label = "Y", .unit = "V", .read = rd_temp },
    };
    static const char *const leaf_notes[] = {
        "FIRST_NOTE_LINE",
        "SECOND_NOTE_LINE",
    };
    static const atc_menu_table_t leaf_with_notes = {
        .items = leaf_with_notes_items,
        .count = sizeof leaf_with_notes_items / sizeof leaf_with_notes_items[0],
        .notes = leaf_notes,
        .note_count = sizeof leaf_notes / sizeof leaf_notes[0],
    };
    static const atc_menu_item_t notes_root_items[] = {
        { .type = ATC_ROW_GROUP,   .label = "Top" },
        { .type = ATC_ROW_SUBMENU, .key = 'i', .label = "Inner",
          .submenu = &leaf_with_notes },
    };
    static const char *const root_notes[] = {
        "ROOT_NOTE_LINE",
    };
    static const atc_menu_table_t notes_root = {
        .items = notes_root_items,
        .count = sizeof notes_root_items / sizeof notes_root_items[0],
        .notes = root_notes,
        .note_count = 1,
    };

    mock_reset();
    atc_menu_init(&notes_root, &mock_port);
    mock_reset();
    atc_menu_render();
    out = mock_buffer();
    /* Root-level note appears on root render. */
    EXPECT_CONTAINS(out, "ROOT_NOTE_LINE");
    EXPECT_NOT_CONTAINS(out, "FIRST_NOTE_LINE");

    /* Drilling into the sub-menu swaps the note set. */
    mock_reset();
    atc_menu_handle_key('i');
    out = mock_buffer();
    EXPECT_CONTAINS(out, "FIRST_NOTE_LINE");
    EXPECT_CONTAINS(out, "SECOND_NOTE_LINE");
    EXPECT_NOT_CONTAINS(out, "ROOT_NOTE_LINE");

    /* Popping back restores the root notes. */
    mock_reset();
    atc_menu_handle_key('b');
    out = mock_buffer();
    EXPECT_CONTAINS(out, "ROOT_NOTE_LINE");
    EXPECT_NOT_CONTAINS(out, "FIRST_NOTE_LINE");

    /* A table with no notes renders no notes block. */
    static const atc_menu_item_t no_notes_items[] = {
        { .type = ATC_ROW_GROUP, .label = "NoNotes" },
    };
    static const atc_menu_table_t no_notes_tbl = {
        .items = no_notes_items, .count = 1,
    };
    mock_reset();
    atc_menu_init(&no_notes_tbl, &mock_port);
    mock_reset();
    atc_menu_render();
    EXPECT_NOT_CONTAINS(mock_buffer(), "ROOT_NOTE_LINE");

    /* Oversize label/unit are truncated at their column boundary. */
    static const atc_menu_item_t over[] = {
        { .type = ATC_ROW_VALUE, .key = 'x',
          .label = "ABCDEFGHIJKLMNOPQRSTUVWX_TAILSHOULDDISAPPEAR",
          .unit  = "kgPAS_TAIL", .read = rd_temp },
    };
    ATC_INIT_ITEMS(over, &mock_port);
    mock_reset();
    atc_menu_render();
    out = mock_buffer();
    EXPECT_CONTAINS(out, "ABCDEFGHIJKLMNOPQRSTUVWX");
    EXPECT_NOT_CONTAINS(out, "TAILSHOULDDISAPPEAR");
    EXPECT_NOT_CONTAINS(out, "kgPAS_TAIL");

    TEST_RESULT();
}
