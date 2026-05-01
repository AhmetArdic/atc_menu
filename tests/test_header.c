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

/* MENU_HDR_INNER (50) is internal to the renderer, so reproduce the box
 * width here. Top edge: corner + 52 dashes + corner = 54 visible chars. */
#define BOX_WIDTH 54

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
    atc_menu_init(one_row, 1, &mock_port);
    mock_reset();
    atc_menu_render();
}

int main(void) {
    /* --- No info: default "atc menu" project, no version. --- */
    render_with_info(NULL);
    const char *out = mock_buffer();
    EXPECT(visible_line_len(out, 0) == BOX_WIDTH);
    EXPECT_CONTAINS(out, "atc menu");

    /* --- Project + version both fit. --- */
    static const atc_menu_info_t fits = {
        .project = "ATC Menu Demo",
        .version = "1.0.0",
    };
    render_with_info(&fits);
    out = mock_buffer();
    EXPECT(visible_line_len(out, 0) == BOX_WIDTH);
    EXPECT_CONTAINS(out, "ATC Menu Demo");
    EXPECT_CONTAINS(out, "1.0.0");

    /* --- Long project, short version: project skipped, version stays.
     * Threshold: tl + 8 > 52 (title needs +2 padding, +1 leading dash, +5
     * reserve for " v9 -"). 60-char project clears it comfortably. --- */
    static const atc_menu_info_t long_project = {
        .project = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
        .version = "v9",
    };
    render_with_info(&long_project);
    out = mock_buffer();
    EXPECT(visible_line_len(out, 0) == BOX_WIDTH);
    EXPECT_NOT_CONTAINS(out, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    EXPECT_CONTAINS(out, "v9");

    /* --- Both too long: both skipped, top edge is just dashes. Version
     * guard fails when reserve_right > 51 → vl > 48; 60 chars clears it. --- */
    static const atc_menu_info_t both_long = {
        .project = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
        .version = "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB",
    };
    render_with_info(&both_long);
    out = mock_buffer();
    EXPECT(visible_line_len(out, 0) == BOX_WIDTH);
    EXPECT_NOT_CONTAINS(out, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    EXPECT_NOT_CONTAINS(out, "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB");

    /* --- Project only (no version): no spurious dashes / junk on right. --- */
    static const atc_menu_info_t no_version = {
        .project = "Solo",
    };
    render_with_info(&no_version);
    out = mock_buffer();
    EXPECT(visible_line_len(out, 0) == BOX_WIDTH);
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
    EXPECT(visible_line_len(out, 0) == BOX_WIDTH);  /* top edge */
    EXPECT(visible_line_len(out, 1) == BOX_WIDTH);  /* author/build */
    EXPECT(visible_line_len(out, 2) == BOX_WIDTH);  /* separator */

    /* --- Long author + long build: clamped, box width preserved. --- */
    static const atc_menu_info_t long_meta = {
        .project = "X", .version = "1",
        .author  = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
        .build   = "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB",
    };
    render_with_info(&long_meta);
    EXPECT(visible_line_len(mock_buffer(), 1) == BOX_WIDTH);

    atc_menu_set_info(NULL);
    TEST_RESULT();
}
