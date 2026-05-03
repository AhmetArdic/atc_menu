/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/render.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const status_disp_t STATUS[] = {
    [ATC_ST_NONE] = { "",            ""           },
    [ATC_ST_OK]   = { ANSI_FG_OK,    SYM_ST_OK    },
    [ATC_ST_WARN] = { ANSI_FG_WARN,  SYM_ST_WARN  },
    [ATC_ST_ERR]  = { ANSI_FG_ERR,   SYM_ST_ERR   },
    [ATC_ST_ON]   = { ANSI_FG_OK,    SYM_ST_ON    },
    [ATC_ST_OFF]  = { ANSI_DIM,      SYM_ST_OFF   },
};

const status_disp_t *status_disp(atc_status_t st) {
    static const status_disp_t empty = { "", "" };
    if ((unsigned)st >= sizeof STATUS / sizeof STATUS[0]) return &empty;
    return &STATUS[st];
}

static const char *zebra_bg(int zebra_idx) {
    return (zebra_idx & 1) ? ANSI_BG_ZEBRA : "";
}

static int utf8_cols(const char *s) {
    int cols = 0;
    if (!s) return 0;
    while (*s) {
        unsigned char c = (unsigned char)*s;
        int adv;
        if      ((c & 0x80) == 0x00) adv = 1;
        else if ((c & 0xE0) == 0xC0) adv = 2;
        else if ((c & 0xF0) == 0xE0) adv = 3;
        else if ((c & 0xF8) == 0xF0) adv = 4;
        else                         adv = 1;
        cols++;
        s += adv;
    }
    return cols;
}

void row_reset(row_t *r) {
    r->bg  = "";
    r->len = 0;
}

void row_flush(row_t *r) {
    const atc_menu_port_t *p = menu_port();
    if (p && p->write && r->len > 0) p->write(r->buf, (size_t)r->len);
    r->len = 0;
}

static void row_vappend(row_t *r, const char *fmt, va_list ap) {
    int avail = (int)sizeof r->buf - r->len;
    if (avail <= 1) return;
    int n = vsnprintf(r->buf + r->len, (size_t)avail, fmt, ap);
    if (n < 0)         return;
    if (n >= avail)    n = avail - 1;
    r->len += n;
}

void row_printf(row_t *r, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    row_vappend(r, fmt, ap);
    va_end(ap);
}

void row_open(row_t *r, int zebra_idx) {
    row_reset(r);
    r->bg = zebra_bg(zebra_idx);
    row_printf(r, ANSI_DIM SYM_BOX_V ANSI_RESET "%s", r->bg);
}

void row_gap(row_t *r) {
    row_printf(r, "%*s", MENU_FIELD_GAP_W, "");
}

void row_key(row_t *r, char k) {
    row_printf(r, ANSI_FG_KEY "%c" ANSI_RESET "%s", k ? k : ' ', r->bg);
}

void row_cell(row_t *r, int width, const char *style, const char *text) {
    if (!text) text = "";
    int cols = utf8_cols(text);
    int pad  = width - cols;

    if (style && *style) row_printf(r, "%s", style);
    if (cols <= width) {
        row_printf(r, "%s", text);
        if (pad > 0) row_printf(r, "%*s", pad, "");
    } else {
        row_printf(r, "%-*.*s", width, width, text);
    }
    if (style && *style) row_printf(r, ANSI_RESET "%s", r->bg);
}

void row_cell_right(row_t *r, int width, const char *style, const char *text) {
    if (!text) text = "";
    int cols = utf8_cols(text);
    int pad  = width - cols;

    if (style && *style) row_printf(r, "%s", style);
    if (cols <= width) {
        if (pad > 0) row_printf(r, "%*s", pad, "");
        row_printf(r, "%s", text);
    } else {
        row_printf(r, "%-*.*s", width, width, text);
    }
    if (style && *style) row_printf(r, ANSI_RESET "%s", r->bg);
}

void row_text(row_t *r, const char *style, const char *text) {
    if (style && *style) row_printf(r, "%s", style);
    row_printf(r, "%s", text ? text : "");
    if (style && *style) row_printf(r, ANSI_RESET "%s", r->bg);
}

void row_close(row_t *r) {
    row_printf(r, ANSI_DIM SYM_BOX_V ANSI_RESET ANSI_EOL "\r\n");
    row_flush(r);
}

void render_row_cells(int zebra_idx, const render_cells_t *c) {
    const status_disp_t *sd = status_disp(c->status);
    const char          *vc = c->value_color ? c->value_color : ANSI_FG_VAL;

    row_t r;
    row_open(&r, zebra_idx);
    row_key(&r, c->key);
    row_gap(&r);
    row_cell      (&r, MENU_LABEL_COL,  NULL,      c->label);
    row_gap(&r);
    row_cell_right(&r, MENU_VALUE_COL,  vc,        c->value);
    row_gap(&r);
    row_cell      (&r, MENU_UNIT_COL,   ANSI_DIM,  c->unit);
    row_gap(&r);
    row_cell      (&r, MENU_STATUS_COL, sd->color, sd->text);
    row_close(&r);
}

static void box_edge(const char *left, const char *right) {
    row_t r;
    row_reset(&r);
    row_printf(&r, ANSI_DIM "%s", left);
    for (int i = 0; i < MENU_INNER_W; i++) row_printf(&r, "%s", SYM_BOX_H);
    row_printf(&r, "%s" ANSI_RESET ANSI_EOL "\r\n", right);
    row_flush(&r);
}

void render_box_separator(void) { box_edge(SYM_BOX_LJ, SYM_BOX_RJ); }
void render_box_bottom(void)    { box_edge(SYM_BOX_BL, SYM_BOX_BR); }

static void notes_dotted_separator(void) {
    row_t r;
    row_reset(&r);
    row_printf(&r, ANSI_DIM SYM_BOX_V);
    for (int i = 0; i < MENU_INNER_W; i++) row_printf(&r, "%s", SYM_BOX_DOT);
    row_printf(&r, SYM_BOX_V ANSI_RESET ANSI_EOL "\r\n");
    row_flush(&r);
}

void render_notes_block(const char *const *notes, size_t count) {
    if (!notes || count == 0) return;

    notes_dotted_separator();

    for (size_t i = 0; i < count; i++) {
        row_t r;
        row_open(&r, 0);
        row_cell(&r, MENU_NOTE_W, ANSI_DIM, notes[i]);
        row_close(&r);
    }
}

int render_notes_lines(size_t count) {
    return count == 0 ? 0 : 1 + (int)count;
}

void render_validate_notes(const char *const *notes, size_t count) {
    if (!notes || count == 0) return;
    for (size_t i = 0; i < count; i++) {
        if (notes[i] && (int)strlen(notes[i]) > MENU_NOTE_W)
            menu_printf("WARN: note exceeds %d cols: '%s'\r\n",
                            MENU_NOTE_W, notes[i]);
    }
}

void render_box_top(const char *title, const char *version) {
    const int total = MENU_INNER_W;

    row_t r;
    row_reset(&r);
    row_printf(&r, "%s", ANSI_DIM SYM_BOX_TL SYM_BOX_H);
    int used = 1;

    int tl = title   ? (int)strlen(title)   : 0;
    int vl = version ? (int)strlen(version) : 0;

    int reserve_right = vl ? (vl + 3) : 0;

    if (title && used + tl + 2 + reserve_right <= total) {
        row_printf(&r, " %s ", title);
        used += tl + 2;
    }

    int dashes_end = total - reserve_right;
    for (; used < dashes_end; used++) row_printf(&r, "%s", SYM_BOX_H);

    if (version && used + reserve_right <= total) {
        row_printf(&r, " %s ", version);
        used += vl + 2;
    }

    for (; used < total; used++) row_printf(&r, "%s", SYM_BOX_H);
    row_printf(&r, "%s", SYM_BOX_TR ANSI_RESET ANSI_EOL "\r\n");
    row_flush(&r);
}

void render_info_row(const char *l, const char *r,
                     const char *l_color, const char *r_color) {
    int lw = l ? (int)strlen(l) : 0;
    int rw = r ? (int)strlen(r) : 0;
    if (rw > MENU_NOTE_W)      rw = MENU_NOTE_W;
    if (lw > MENU_NOTE_W - rw) lw = MENU_NOTE_W - rw;
    int gap = MENU_NOTE_W - lw - rw;

    row_t row;
    row_open(&row, 0);
    if (l_color && *l_color) row_printf(&row, "%s", l_color);
    row_printf(&row, "%.*s" ANSI_RESET, lw, l ? l : "");
    row_printf(&row, "%*s", gap, "");
    if (r_color && *r_color) row_printf(&row, "%s", r_color);
    row_printf(&row, "%.*s" ANSI_RESET, rw, r ? r : "");
    row_close(&row);
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
