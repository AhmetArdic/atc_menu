/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/render.h"

#include <string.h>

static void box_edge(const char *left, const char *right) {
    row_buf_t b;
    row_buf_reset(&b);
    row_buf_printf(&b, ANSI_DIM "%s", left);
    for (int i = 0; i < MENU_INNER_W; i++) row_buf_printf(&b, "%s", SYM_BOX_H);
    row_buf_printf(&b, "%s" ANSI_RESET ANSI_EOL "\r\n", right);
    row_buf_flush(&b);
}

void render_box_separator(void) { box_edge(SYM_BOX_LJ, SYM_BOX_RJ); }
void render_box_bottom(void)    { box_edge(SYM_BOX_BL, SYM_BOX_BR); }

static void notes_dotted_separator(void) {
    row_buf_t b;
    row_buf_reset(&b);
    row_buf_printf(&b, ANSI_DIM SYM_BOX_V);
    for (int i = 0; i < MENU_INNER_W; i++) row_buf_printf(&b, "%s", SYM_BOX_DOT);
    row_buf_printf(&b, SYM_BOX_V ANSI_RESET ANSI_EOL "\r\n");
    row_buf_flush(&b);
}

void render_notes_block(const char *const *notes, size_t count) {
    if (!notes || count == 0) return;

    notes_dotted_separator();

    for (size_t i = 0; i < count; i++) {
        row_t r;
        row_begin(&r, &ROW_LAYOUT_NOTE, 0);
        row_set(&r, 0, NULL, notes[i]);
        row_end(&r);
    }
}

int render_notes_lines(size_t count) {
    return count == 0 ? 0 : 1 + (int)count;
}

void render_validate_notes(const char *const *notes, size_t count) {
    if (!notes || count == 0) return;
    for (size_t i = 0; i < count; i++) {
        if (notes[i] && (int)strlen(notes[i]) > MENU_REGION_NOTE_W)
            menu_printf("WARN: note exceeds %d cols: '%s'\r\n",
                            MENU_REGION_NOTE_W, notes[i]);
    }
}

void render_box_top(const char *title, const char *version) {
    const int total = MENU_INNER_W;

    row_buf_t b;
    row_buf_reset(&b);
    row_buf_printf(&b, "%s", ANSI_DIM SYM_BOX_TL SYM_BOX_H);
    int used = 1;

    int tl = title   ? (int)strlen(title)   : 0;
    int vl = version ? (int)strlen(version) : 0;

    int reserve_right = vl ? (vl + 3) : 0;

    if (title && used + tl + 2 + reserve_right <= total) {
        row_buf_printf(&b, " %s ", title);
        used += tl + 2;
    }

    int dashes_end = total - reserve_right;
    for (; used < dashes_end; used++) row_buf_printf(&b, "%s", SYM_BOX_H);

    if (version && used + reserve_right <= total) {
        row_buf_printf(&b, " %s ", version);
        used += vl + 2;
    }

    for (; used < total; used++) row_buf_printf(&b, "%s", SYM_BOX_H);
    row_buf_printf(&b, "%s", SYM_BOX_TR ANSI_RESET ANSI_EOL "\r\n");
    row_buf_flush(&b);
}

void render_info_row(const char *l, const char *r,
                     const char *l_color, const char *r_color) {
    int lw = l ? (int)strlen(l) : 0;
    int rw = r ? (int)strlen(r) : 0;
    if (rw > MENU_REGION_NOTE_W)      rw = MENU_REGION_NOTE_W;
    if (lw > MENU_REGION_NOTE_W - rw) lw = MENU_REGION_NOTE_W - rw;
    int gap = MENU_REGION_NOTE_W - lw - rw;

    row_buf_t b;
    row_buf_reset(&b);
    row_buf_printf(&b, ANSI_DIM SYM_BOX_V ANSI_RESET);
    if (l_color && *l_color) row_buf_printf(&b, "%s", l_color);
    row_buf_printf(&b, "%.*s" ANSI_RESET, lw, l ? l : "");
    row_buf_printf(&b, "%*s", gap, "");
    if (r_color && *r_color) row_buf_printf(&b, "%s", r_color);
    row_buf_printf(&b, "%.*s" ANSI_RESET, rw, r ? r : "");
    row_buf_printf(&b, ANSI_DIM SYM_BOX_V ANSI_RESET ANSI_EOL "\r\n");
    row_buf_flush(&b);
}

void render_header(const atc_menu_info_t *info) {
    const char *project = (info && info->project) ? info->project : "atc menu";
    const char *version = (info && info->version) ? info->version : NULL;
    const char *author  = (info && info->author)  ? info->author  : NULL;
    const char *build   = (info && info->build)   ? info->build   : NULL;

    render_box_top(author, build);
    render_info_row(project, version, ANSI_BOLD ANSI_FG_KEY, ANSI_FG_VAL);
    render_box_separator();
}

int render_header_lines(const atc_menu_info_t *info) {
    (void)info;
    return 3;
}

void render_default_footer(bool show_back) {
    if (show_back)
        menu_printf("\r\n" ANSI_DIM
            "[r] refresh  [b] back  [?] path  [:] cmd"
            ANSI_RESET ANSI_EOL "\r\n");
    else
        menu_printf("\r\n" ANSI_DIM
            "[r] refresh  [:] cmd"
            ANSI_RESET ANSI_EOL "\r\n");
}

void render_status_line(const char *msg) {
    menu_printf("\r\n" ANSI_DIM SYM_PROMPT "%s" ANSI_EOL ANSI_RESET "\r\n",
                    msg ? msg : "");
    menu_set_status_dirty(true);
}

void render_park_cursor(int y, bool clear_below) {
    menu_printf(ANSI_GOTO_FMT, y);
    if (clear_below) menu_printf("%s", ANSI_CLR_BELOW);
}
