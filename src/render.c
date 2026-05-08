/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render.h"
#include "nav.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================ row buffer */

typedef struct {
    char buf[MENU_ROW_BUF];
    int  len;
} row_buf_t;

static void buf_reset(row_buf_t *b) { b->len = 0; }

static void buf_flush(row_buf_t *b) {
    const atc_menu_port_t *p = menu_port();
    if (p && p->write && b->len > 0) p->write(b->buf, (size_t)b->len);
    b->len = 0;
}

static void buf_printf(row_buf_t *b, const char *fmt, ...) {
    int avail = (int)sizeof b->buf - b->len;
    if (avail <= 1) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b->buf + b->len, (size_t)avail, fmt, ap);
    va_end(ap);
    if (n < 0)      return;
    if (n >= avail) n = avail - 1;
    b->len += n;
}

/* ================================================================ row layout */

typedef enum { ALIGN_LEFT, ALIGN_RIGHT } align_t;

typedef struct {
    int         width;
    align_t     align;
    const char *default_style;
    int         gap_after;
} region_t;

typedef struct {
    const region_t *regions;
    size_t          count;
} layout_t;

#define GAP MENU_REGION_GAP_W

static const region_t LAYOUT_SCALAR_R[] = {
    { MENU_REGION_KEY_W,    ALIGN_LEFT,  ANSI_FG_KEY, GAP },
    { MENU_REGION_LABEL_W,  ALIGN_LEFT,  NULL,        GAP },
    { MENU_REGION_VALUE_W,  ALIGN_RIGHT, ANSI_FG_VAL, GAP },
    { MENU_REGION_UNIT_W,   ALIGN_LEFT,  ANSI_DIM,    GAP },
    { MENU_REGION_STATUS_W, ALIGN_LEFT,  NULL,        0   },
};

static const region_t LAYOUT_GROUP_R[] = {
    { MENU_REGION_KEY_W,         ALIGN_LEFT, ANSI_FG_KEY, GAP },
    { MENU_REGION_GROUP_LABEL_W, ALIGN_LEFT, NULL,        0   },
};

static const region_t LAYOUT_SUBMENU_R[] = {
    { MENU_REGION_KEY_W,            ALIGN_LEFT, ANSI_FG_KEY, GAP },
    { MENU_REGION_SUBMENU_PROMPT_W, ALIGN_LEFT, ANSI_FG_KEY, 0   },
    { MENU_REGION_SUBMENU_LABEL_W,  ALIGN_LEFT, NULL,        0   },
};

static const region_t LAYOUT_INPUT_R[] = {
    { MENU_REGION_KEY_W,        ALIGN_LEFT, ANSI_FG_KEY, GAP },
    { MENU_REGION_LABEL_W,      ALIGN_LEFT, NULL,        GAP },
    { MENU_REGION_INPUT_EDIT_W, ALIGN_LEFT, ANSI_FG_VAL, 0   },
};

static const region_t LAYOUT_NOTE_R[] = {
    { MENU_REGION_NOTE_W, ALIGN_LEFT, ANSI_DIM, 0 },
};

#undef GAP

#define LAYOUT(arr) { arr, sizeof arr / sizeof arr[0] }
static const layout_t LAYOUT_SCALAR  = LAYOUT(LAYOUT_SCALAR_R);
static const layout_t LAYOUT_GROUP   = LAYOUT(LAYOUT_GROUP_R);
static const layout_t LAYOUT_SUBMENU = LAYOUT(LAYOUT_SUBMENU_R);
static const layout_t LAYOUT_INPUT   = LAYOUT(LAYOUT_INPUT_R);
static const layout_t LAYOUT_NOTE    = LAYOUT(LAYOUT_NOTE_R);
#undef LAYOUT

/* ================================================================ helpers */

static int utf8_cols(const char *s) {
    int cols = 0;
    if (!s) return 0;
    while (*s) {
        unsigned char c = (unsigned char)*s;
        int adv = ((c & 0x80) == 0x00) ? 1
                : ((c & 0xE0) == 0xC0) ? 2
                : ((c & 0xF0) == 0xE0) ? 3
                : ((c & 0xF8) == 0xF0) ? 4 : 1;
        cols++;
        s += adv;
    }
    return cols;
}

static const char *zebra_bg(int idx) {
    return (idx & 1) ? ANSI_BG_ZEBRA : "";
}

static void emit_padded(row_buf_t *b, const region_t *def, const char *text) {
    int cols = utf8_cols(text);
    int pad  = def->width - cols;
    if (cols > def->width) {
        buf_printf(b, "%-*.*s", def->width, def->width, text);
    } else if (def->align == ALIGN_RIGHT) {
        if (pad > 0) buf_printf(b, "%*s", pad, "");
        buf_printf(b, "%s", text);
    } else {
        buf_printf(b, "%s", text);
        if (pad > 0) buf_printf(b, "%*s", pad, "");
    }
}

/* ================================================================ row composer */

#define MAX_REGIONS 8

typedef struct {
    const layout_t *layout;
    const char     *bg;
    struct { const char *style, *text; } cells[MAX_REGIONS];
    row_buf_t       out;
} row_t;

static void row_begin(row_t *r, const layout_t *layout, int zebra) {
    r->layout = layout;
    r->bg     = zebra_bg(zebra);
    buf_reset(&r->out);
    size_t n = layout->count > MAX_REGIONS ? MAX_REGIONS : layout->count;
    for (size_t i = 0; i < n; i++) { r->cells[i].style = NULL; r->cells[i].text = NULL; }
}

static void row_set(row_t *r, size_t idx, const char *style, const char *text) {
    if (idx >= r->layout->count || idx >= MAX_REGIONS) return;
    r->cells[idx].style = style;
    r->cells[idx].text  = text;
}

static void row_end(row_t *r) {
    buf_printf(&r->out, ANSI_DIM SYM_BOX_V ANSI_RESET "%s", r->bg);

    for (size_t i = 0; i < r->layout->count; i++) {
        const region_t *def   = &r->layout->regions[i];
        const char     *text  = r->cells[i].text ? r->cells[i].text : "";
        const char     *style = (r->cells[i].style && *r->cells[i].style)
                              ? r->cells[i].style : def->default_style;
        bool styled = style && *style;

        if (styled) buf_printf(&r->out, "%s", style);
        emit_padded(&r->out, def, text);
        if (styled) buf_printf(&r->out, ANSI_RESET "%s", r->bg);

        if (i + 1 < r->layout->count && def->gap_after > 0)
            buf_printf(&r->out, "%*s", def->gap_after, "");
    }

    buf_printf(&r->out, ANSI_DIM SYM_BOX_V ANSI_RESET ANSI_EOL "\r\n");
    buf_flush(&r->out);
}

/* ================================================================ status disp */

typedef struct { const char *color, *text; } status_disp_t;

static const status_disp_t *status_disp(atc_status_t st) {
    static const status_disp_t TABLE[] = {
        [ATC_ST_NONE] = { "",            ""           },
        [ATC_ST_OK]   = { ANSI_FG_OK,    SYM_ST_OK    },
        [ATC_ST_WARN] = { ANSI_FG_WARN,  SYM_ST_WARN  },
        [ATC_ST_ERR]  = { ANSI_FG_ERR,   SYM_ST_ERR   },
        [ATC_ST_ON]   = { ANSI_FG_OK,    SYM_ST_ON    },
        [ATC_ST_OFF]  = { ANSI_DIM,      SYM_ST_OFF   },
    };
    static const status_disp_t empty = { "", "" };
    if ((unsigned)st >= sizeof TABLE / sizeof TABLE[0]) return &empty;
    return &TABLE[st];
}

/* ================================================================ chrome */

static void box_edge(const char *left, const char *right) {
    row_buf_t b;
    buf_reset(&b);
    buf_printf(&b, ANSI_DIM "%s", left);
    for (int i = 0; i < MENU_INNER_W; i++) buf_printf(&b, "%s", SYM_BOX_H);
    buf_printf(&b, "%s" ANSI_RESET ANSI_EOL "\r\n", right);
    buf_flush(&b);
}

static void box_top(const char *title, const char *version) {
    const int total = MENU_INNER_W;

    row_buf_t b;
    buf_reset(&b);
    buf_printf(&b, "%s", ANSI_DIM SYM_BOX_TL SYM_BOX_H);
    int used = 1;

    int tl = title   ? (int)strlen(title)   : 0;
    int vl = version ? (int)strlen(version) : 0;
    int reserve_right = vl ? (vl + 3) : 0;

    if (title && used + tl + 2 + reserve_right <= total) {
        buf_printf(&b, " %s ", title);
        used += tl + 2;
    }

    int dashes_end = total - reserve_right;
    for (; used < dashes_end; used++) buf_printf(&b, "%s", SYM_BOX_H);

    if (version && used + reserve_right <= total) {
        buf_printf(&b, " %s ", version);
        used += vl + 2;
    }

    for (; used < total; used++) buf_printf(&b, "%s", SYM_BOX_H);
    buf_printf(&b, "%s", SYM_BOX_TR ANSI_RESET ANSI_EOL "\r\n");
    buf_flush(&b);
}

static void info_row(const char *l, const char *r,
                     const char *l_color, const char *r_color) {
    int lw = l ? (int)strlen(l) : 0;
    int rw = r ? (int)strlen(r) : 0;
    if (rw > MENU_REGION_NOTE_W)      rw = MENU_REGION_NOTE_W;
    if (lw > MENU_REGION_NOTE_W - rw) lw = MENU_REGION_NOTE_W - rw;
    int gap = MENU_REGION_NOTE_W - lw - rw;

    row_buf_t b;
    buf_reset(&b);
    buf_printf(&b, ANSI_DIM SYM_BOX_V ANSI_RESET);
    if (l_color && *l_color) buf_printf(&b, "%s", l_color);
    buf_printf(&b, "%.*s" ANSI_RESET, lw, l ? l : "");
    buf_printf(&b, "%*s", gap, "");
    if (r_color && *r_color) buf_printf(&b, "%s", r_color);
    buf_printf(&b, "%.*s" ANSI_RESET, rw, r ? r : "");
    buf_printf(&b, ANSI_DIM SYM_BOX_V ANSI_RESET ANSI_EOL "\r\n");
    buf_flush(&b);
}

static void render_header(void) {
    const atc_menu_info_t *info = menu_info();
    const char *project = (info && info->project) ? info->project : "atc menu";
    const char *version = (info && info->version) ? info->version : NULL;
    const char *author  = (info && info->author)  ? info->author  : NULL;
    const char *build   = (info && info->build)   ? info->build   : NULL;

    box_top(author, build);
    info_row(project, version, ANSI_BOLD ANSI_FG_KEY, ANSI_FG_VAL);
    box_edge(SYM_BOX_LJ, SYM_BOX_RJ);
}

static int notes_lines(size_t count) { return count == 0 ? 0 : 1 + (int)count; }

static void render_notes(const char *const *notes, size_t count) {
    if (!notes || count == 0) return;

    row_buf_t b;
    buf_reset(&b);
    buf_printf(&b, ANSI_DIM SYM_BOX_V);
    for (int i = 0; i < MENU_INNER_W; i++) buf_printf(&b, "%s", SYM_BOX_DOT);
    buf_printf(&b, SYM_BOX_V ANSI_RESET ANSI_EOL "\r\n");
    buf_flush(&b);

    for (size_t i = 0; i < count; i++) {
        row_t r;
        row_begin(&r, &LAYOUT_NOTE, 0);
        row_set(&r, 0, NULL, notes[i]);
        row_end(&r);
    }
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

void menu_park_cursor(void) {
    int below = MENU_HEADER_LINES + (int)nav_count()
              + notes_lines(nav_note_count()) + 4;
    render_park_cursor(below, !menu_status_dirty());
}

void render_validate_notes(const char *const *notes, size_t count) {
    if (!notes || count == 0) return;
    for (size_t i = 0; i < count; i++) {
        if (notes[i] && (int)strlen(notes[i]) > MENU_REGION_NOTE_W)
            menu_printf("WARN: note exceeds %d cols: '%s'\r\n",
                        MENU_REGION_NOTE_W, notes[i]);
    }
}

/* ================================================================ scalar / group / submenu */

static void render_scalar(int zebra, const atc_menu_item_t *it) {
    char val[MENU_BUF_SIZE] = {0};
    atc_status_t st = ATC_ST_NONE;
    if (it->read) it->read(val, sizeof val, &st);

    char key_buf[2] = { it->key ? it->key : ' ', 0 };
    const status_disp_t *sd = status_disp(st);

    row_t r;
    row_begin(&r, &LAYOUT_SCALAR, zebra);
    row_set(&r, 0, NULL,      key_buf);
    row_set(&r, 1, NULL,      it->label);
    row_set(&r, 2, NULL,      val);
    row_set(&r, 3, NULL,      it->unit);
    row_set(&r, 4, sd->color, sd->text);
    row_end(&r);
}

static void render_group(int zebra, const atc_menu_item_t *it) {
    char key_buf[2] = { it->key ? it->key : ' ', 0 };

    row_t r;
    row_begin(&r, &LAYOUT_GROUP, zebra);
    row_set(&r, 0, NULL,      key_buf);
    row_set(&r, 1, ANSI_BOLD, it->label);
    row_end(&r);
}

static void render_submenu(int zebra, const atc_menu_item_t *it) {
    char key_buf[2] = { it->key ? it->key : ' ', 0 };

    row_t r;
    row_begin(&r, &LAYOUT_SUBMENU, zebra);
    row_set(&r, 0, NULL, key_buf);
    row_set(&r, 1, NULL, SYM_SUBMENU);
    row_set(&r, 2, NULL, it->label);
    row_end(&r);
}

/* ================================================================ bar */

#if ATC_MENU_WIDGET_BAR

#define BAR_CELLS  (MENU_REGION_VALUE_W - 2)
#define BAR_SUB    8
#define BAR_LEVELS (BAR_CELLS * BAR_SUB)

static const char *const BAR_PARTIAL[BAR_SUB] = {
    "", "▏", "▎", "▍", "▌", "▋", "▊", "▉",
};

static int clamp_pct(long pct) {
    if (pct < 0)   return 0;
    if (pct > 100) return 100;
    return (int)pct;
}

static const char *bar_fill_color(atc_status_t st) {
    return (st == ATC_ST_WARN) ? ANSI_FG_WARN
         : (st == ATC_ST_ERR)  ? ANSI_FG_ERR
                               : ANSI_FG_OK;
}

static void compose_bar(int pct, char *bar, size_t bar_cap,
                        char *unit, size_t unit_cap) {
    int subs = (pct * BAR_LEVELS + 50) / 100;
    int full = subs / BAR_SUB;
    int part = subs % BAR_SUB;

    size_t bp = 0;
    int n = snprintf(bar + bp, bar_cap - bp, "%s", SYM_BAR_LBR);
    if (n > 0) bp += (size_t)n;
    for (int i = 0; i < full && bp < bar_cap; i++) {
        n = snprintf(bar + bp, bar_cap - bp, "%s", SYM_BAR_FILL);
        if (n > 0) bp += (size_t)n;
    }
    if (part && bp < bar_cap) {
        n = snprintf(bar + bp, bar_cap - bp, "%s", BAR_PARTIAL[part]);
        if (n > 0) bp += (size_t)n;
    }
    for (int i = full + (part > 0); i < BAR_CELLS && bp < bar_cap; i++) {
        n = snprintf(bar + bp, bar_cap - bp, "%s", SYM_BAR_EMPTY);
        if (n > 0) bp += (size_t)n;
    }
    snprintf(bar + bp, bar_cap - bp, "%s", SYM_BAR_RBR);

    snprintf(unit, unit_cap, "%d %%", pct);
}

static void render_bar(int zebra, const atc_menu_item_t *it) {
    char raw[MENU_BUF_SIZE] = {0};
    atc_status_t st = ATC_ST_NONE;
    if (it->read) it->read(raw, sizeof raw, &st);

    int pct = clamp_pct(strtol(raw, NULL, 10));
    char bar[MENU_REGION_VALUE_BUF];
    char unit[MENU_REGION_UNIT_W + 1];
    compose_bar(pct, bar, sizeof bar, unit, sizeof unit);

    char key_buf[2] = { it->key ? it->key : ' ', 0 };
    const status_disp_t *sd = status_disp(st);

    row_t r;
    row_begin(&r, &LAYOUT_SCALAR, zebra);
    row_set(&r, 0, NULL,              key_buf);
    row_set(&r, 1, NULL,              it->label);
    row_set(&r, 2, bar_fill_color(st), bar);
    row_set(&r, 3, NULL,              unit);
    row_set(&r, 4, sd->color,         sd->text);
    row_end(&r);
}

#endif /* ATC_MENU_WIDGET_BAR */

/* ================================================================ choice */

#if ATC_MENU_WIDGET_CHOICE

#define CHOICE_STR_MAX (MENU_REGION_VALUE_W - 4)

static const char *choice_label(const atc_menu_item_t *it, uint8_t idx) {
    if (!it->choices || idx >= it->choice_count) return "";
    return it->choices[idx] ? it->choices[idx] : "";
}

static void format_choice_box(const char *sel, char *out, size_t cap) {
    int sl = (int)strlen(sel);
    if (sl > CHOICE_STR_MAX) sl = CHOICE_STR_MAX;
    int lpad = (CHOICE_STR_MAX - sl) / 2;
    int rpad = CHOICE_STR_MAX - sl - lpad;
    snprintf(out, cap, "%s %*s%.*s%*s %s",
             SYM_CHOICE_LBR, lpad, "", sl, sel, rpad, "", SYM_CHOICE_RBR);
}

static void render_choice(int zebra, const atc_menu_item_t *it,
                          const char *override_label, bool editing) {
    char raw[MENU_BUF_SIZE] = {0};
    atc_status_t st = ATC_ST_OK;
    if (it->read) it->read(raw, sizeof raw, &st);

    const char *sel = override_label ? override_label
                    : (it->choice_idx ? choice_label(it, *it->choice_idx) : "");

    char box[MENU_REGION_VALUE_BUF];
    format_choice_box(sel, box, sizeof box);

    char key_buf[2] = { it->key ? it->key : ' ', 0 };
    const status_disp_t *sd = status_disp(st);

    row_t r;
    row_begin(&r, &LAYOUT_SCALAR, zebra);
    row_set(&r, 0, NULL,        key_buf);
    row_set(&r, 1, NULL,        it->label);
    row_set(&r, 2, editing ? ANSI_BOLD ANSI_FG_VAL : NULL, box);
    row_set(&r, 3, NULL,        it->unit);
    row_set(&r, 4, sd->color,   sd->text);
    row_end(&r);
}

#endif /* ATC_MENU_WIDGET_CHOICE */

/* ================================================================ input edit */

#if ATC_MENU_WIDGET_INPUT

static void render_input_edit(int zebra, const atc_menu_item_t *it,
                              const char *edit_text) {
    char key_buf[2] = { it->key ? it->key : ' ', 0 };

    row_t r;
    row_begin(&r, &LAYOUT_INPUT, zebra);
    row_set(&r, 0, NULL, key_buf);
    row_set(&r, 1, NULL, it->label);
    row_set(&r, 2, NULL, edit_text);
    row_end(&r);
}

#endif /* ATC_MENU_WIDGET_INPUT */

/* ================================================================ dispatch */

static void render_item(int zebra, const atc_menu_item_t *it) {
    switch (it->type) {
        case ATC_ROW_GROUP:    render_group(zebra, it);    return;
        case ATC_ROW_SUBMENU:  render_submenu(zebra, it);  return;
        case ATC_ROW_VALUE:
        case ATC_ROW_STATE:
        case ATC_ROW_ACTION:   render_scalar(zebra, it);   return;
#if ATC_MENU_WIDGET_BAR
        case ATC_ROW_BAR:      render_bar(zebra, it);      return;
#endif
#if ATC_MENU_WIDGET_CHOICE
        case ATC_ROW_CHOICE:   render_choice(zebra, it, NULL, false); return;
#endif
#if ATC_MENU_WIDGET_INPUT
        case ATC_ROW_INPUT:    render_scalar(zebra, it);   return;
#endif
        default: return;
    }
}

static int row_y(size_t index) { return MENU_HEADER_LINES + 1 + (int)index; }

void render_all(void) {
    if (!nav_items() || !menu_port()) return;

    menu_printf("%s", ANSI_HOME);
    render_header();

    const atc_menu_item_t *items = nav_items();
    size_t                 count = nav_count();
    for (size_t i = 0; i < count; i++) render_item((int)i, &items[i]);

    render_notes(nav_notes(), nav_note_count());
    box_edge(SYM_BOX_BL, SYM_BOX_BR);
    menu_printf("%s", ANSI_CLR_BELOW);
}

void render_row(size_t index) {
    render_park_cursor(row_y(index), false);
    menu_printf("%s", ANSI_EOL);
    render_item((int)index, &nav_items()[index]);
    menu_park_cursor();
}

void render_row_input(size_t index, const char *edit_text) {
#if ATC_MENU_WIDGET_INPUT
    render_park_cursor(row_y(index), false);
    menu_printf("%s", ANSI_EOL);
    render_input_edit((int)index, &nav_items()[index], edit_text);
    menu_park_cursor();
#else
    (void)index; (void)edit_text;
#endif
}

void render_row_choice(size_t index, const char *pending_label) {
#if ATC_MENU_WIDGET_CHOICE
    render_park_cursor(row_y(index), false);
    menu_printf("%s", ANSI_EOL);
    render_choice((int)index, &nav_items()[index], pending_label, true);
    menu_park_cursor();
#else
    (void)index; (void)pending_label;
#endif
}

void render_group_span(size_t start) {
    const atc_menu_item_t *items = nav_items();
    size_t                 count = nav_count();
    size_t                 end   = start + 1;
    while (end < count && items[end].type != ATC_ROW_GROUP) end++;

    render_park_cursor(row_y(start), false);
    menu_printf("%s", ANSI_EOL);
    for (size_t i = start; i < end; i++) render_item((int)i, &items[i]);
    menu_park_cursor();
}

/* ================================================================ validation */

static void warn_label_unit(const atc_menu_item_t *it) {
    if (it->label && strlen(it->label) > MENU_REGION_LABEL_W)
        menu_printf("WARN: label '%s' exceeds %d cols\r\n",
                    it->label, MENU_REGION_LABEL_W);
    if (it->unit && strlen(it->unit) > MENU_REGION_UNIT_W)
        menu_printf("WARN: unit '%s' exceeds %d cols\r\n",
                    it->unit, MENU_REGION_UNIT_W);
}

void render_validate_item(const atc_menu_item_t *it) {
    switch (it->type) {
        case ATC_ROW_GROUP:
            if (it->label && strlen(it->label) > MENU_REGION_GROUP_LABEL_W)
                menu_printf("WARN: GROUP label '%s' exceeds %d cols\r\n",
                            it->label, MENU_REGION_GROUP_LABEL_W);
            return;
        case ATC_ROW_SUBMENU:
            if (!it->submenu || !it->submenu->items || it->submenu->count == 0)
                menu_printf("WARN: ATC_ROW_SUBMENU '%c' missing submenu\r\n", it->key);
            if (it->label && strlen(it->label) > MENU_REGION_SUBMENU_LABEL_W)
                menu_printf("WARN: SUBMENU label '%s' exceeds %d cols\r\n",
                            it->label, MENU_REGION_SUBMENU_LABEL_W);
            if (it->submenu)
                render_validate_notes(it->submenu->notes, it->submenu->note_count);
            return;
        case ATC_ROW_VALUE:
            if (!it->read)
                menu_printf("WARN: ATC_ROW_VALUE '%c' missing read\r\n", it->key);
            warn_label_unit(it);
            return;
        case ATC_ROW_STATE:
            if (!it->read || !it->action)
                menu_printf("WARN: ATC_ROW_STATE '%c' missing read/action\r\n", it->key);
            warn_label_unit(it);
            return;
        case ATC_ROW_ACTION:
            if (!it->action)
                menu_printf("WARN: ATC_ROW_ACTION '%c' missing action\r\n", it->key);
            warn_label_unit(it);
            return;
#if ATC_MENU_WIDGET_BAR
        case ATC_ROW_BAR:
            if (!it->read)
                menu_printf("WARN: ATC_ROW_BAR '%c' missing read\r\n", it->key);
            return;
#endif
#if ATC_MENU_WIDGET_CHOICE
        case ATC_ROW_CHOICE:
            if (!it->choices || it->choice_count == 0 || !it->choice_idx) {
                menu_printf("WARN: ATC_ROW_CHOICE '%c' missing choices/idx\r\n", it->key);
                return;
            }
            for (uint8_t c = 0; c < it->choice_count; c++) {
                if (it->choices[c] && strlen(it->choices[c]) > CHOICE_STR_MAX)
                    menu_printf("WARN: CHOICE '%c' choice '%s' exceeds %d cols\r\n",
                                it->key, it->choices[c], CHOICE_STR_MAX);
            }
            return;
#endif
#if ATC_MENU_WIDGET_INPUT
        case ATC_ROW_INPUT:
            if (!it->read || !it->input_commit)
                menu_printf("WARN: ATC_ROW_INPUT '%c' missing read/input_commit\r\n",
                            it->key);
            if ((it->input_type == ATC_INPUT_INT || it->input_type == ATC_INPUT_HEX)
                && it->input_min > it->input_max)
                menu_printf("WARN: ATC_ROW_INPUT '%c' min > max\r\n", it->key);
            warn_label_unit(it);
            return;
#endif
        default: return;
    }
}
