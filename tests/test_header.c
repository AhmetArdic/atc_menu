/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * Header layout regression tests for print_top_edge. The geometry is
 * subtle (off-by-one corner accounting, +3 reserve_right for a +2 visible
 * version block, ANSI dim/reset dance) — this file pins the visible width
 * and the title/version skip guards so future refactors can't drift the
 * box alignment without a test failure.
 */

#include "atc_menu/atc_menu.h"
#include "mock_port.h"
#include "testing.h"

/* MENU_INNER_W (= sum of key/label/value/unit/status columns with field
 * gaps and outer pad) is private to the renderer; we read it back from
 * the rendered top edge so this test stays sane when layout.h is tuned. */
static size_t g_box_width;

/* Strip CSI (ESC '[' params final) sequences and return visible length of
 * line @p line_idx from @p src (0-based). Returns 0 if line missing. */
static size_t visible_line_len(const char *src, int line_idx) {
    int    cur   = 0;
    size_t count = 0;
    while (*src) {
        if (src[0] == '\x1b' && src[1] == '[') {
            src += 2;
            while (*src && (*src < '@' || *src > '~')) src++;
            if (*src) src++;
            continue;
        }
        if (*src == '\n') {
            if (cur == line_idx) return count;
            cur++;
            count = 0;
            src++;
            continue;
        }
        if (*src == '\r') { src++; continue; }
        count++;
        src++;
    }
    return cur == line_idx ? count : 0;
}

static const atc_menu_item_t one_row[] = {
    { .type = ATC_ROW_GROUP, .label = "G" },
};

static void render_with_info(const atc_menu_info_t *info) {
    atc_menu_set_info(info);
    ATC_INIT_ITEMS(one_row, &mock_port);
    mock_reset();
    atc_menu_render();
}

int main(void) {
    /* --- No info: default "atc menu" project, no version. Calibrate
     * BOX_WIDTH from the rendered top edge — every other line must match. */
    render_with_info(NULL);
    const char *out = mock_buffer();
    g_box_width = visible_line_len(out, 0);
    EXPECT(g_box_width > 0);
    EXPECT_CONTAINS(out, "atc menu");

    /* --- Project + version both fit. --- */
    static const atc_menu_info_t fits = {
        .project = "ATC Menu Demo",
        .version = "1.0.0",
    };
    render_with_info(&fits);
    out = mock_buffer();
    EXPECT(visible_line_len(out, 0) == g_box_width);
    EXPECT_CONTAINS(out, "ATC Menu Demo");
    EXPECT_CONTAINS(out, "1.0.0");

    /* --- Long project, short version: project skipped, version stays.
     * Strings are sized to exceed any reasonable layout.h tuning. --- */
#define LONG_F \
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" \
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
#define LONG_A \
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" \
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" \
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
#define LONG_B \
    "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB" \
    "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB" \
    "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"

    static const atc_menu_info_t long_project = {
        .project = LONG_F,
        .version = "v9",
    };
    render_with_info(&long_project);
    out = mock_buffer();
    EXPECT(visible_line_len(out, 0) == g_box_width);
    EXPECT_NOT_CONTAINS(out, LONG_F);
    EXPECT_CONTAINS(out, "v9");

    /* --- Both too long: both skipped, top edge is just dashes. --- */
    static const atc_menu_info_t both_long = {
        .project = LONG_A,
        .version = LONG_B,
    };
    render_with_info(&both_long);
    out = mock_buffer();
    EXPECT(visible_line_len(out, 0) == g_box_width);
    EXPECT_NOT_CONTAINS(out, LONG_A);
    EXPECT_NOT_CONTAINS(out, LONG_B);

    /* --- Project only (no version): no spurious dashes / junk on right. --- */
    static const atc_menu_info_t no_version = {
        .project = "Solo",
    };
    render_with_info(&no_version);
    out = mock_buffer();
    EXPECT(visible_line_len(out, 0) == g_box_width);
    EXPECT_CONTAINS(out, "Solo");

    /* --- Author/build row also respects the box width. --- */
    static const atc_menu_info_t with_meta = {
        .project = "X",
        .version = "1",
        .author  = "me",
        .build   = "today",
    };
    render_with_info(&with_meta);
    out = mock_buffer();
    EXPECT(visible_line_len(out, 0) == g_box_width);  /* top edge */
    EXPECT(visible_line_len(out, 1) == g_box_width);  /* author/build */
    EXPECT(visible_line_len(out, 2) == g_box_width);  /* separator */

    /* --- Long author + long build: clamped, box width preserved. --- */
    static const atc_menu_info_t long_meta = {
        .project = "X", .version = "1",
        .author  = LONG_A,
        .build   = LONG_B,
    };
    render_with_info(&long_meta);
    EXPECT(visible_line_len(mock_buffer(), 1) == g_box_width);

    atc_menu_set_info(NULL);
    TEST_RESULT();
}
