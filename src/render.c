/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render.h"
#include "nav.h"

#include <stdarg.h>
#include <string.h>

/* ================================================================ buffer */

typedef struct {
    char       *data;
    int         len;
    const char *bg;
} buf_t;

/* Non-reentrant: one row is composed at a time, so all buf_t instances
 * share a single module-static scratch instead of a per-call stack array. */
static char g_rowbuf[ROW_BUF];

static buf_t buf_begin(void) {
    buf_t b = { g_rowbuf, 0, "" };
    return b;
}

static void buf_flush(buf_t *b) {
    const atc_menu_port_t *p = menu_port();
    if (p && p->write && b->len > 0) p->write(b->data, (size_t)b->len);
    b->len = 0;
}

static void buf_printf(buf_t *b, const char *fmt, ...) {
    int avail = (int)ROW_BUF - b->len;
    if (avail <= 1) return;
    va_list ap;
    va_start(ap, fmt);
    int n = atc_vsnprintf(b->data + b->len, (size_t)avail, fmt, ap);
    va_end(ap);
    if (n < 0)      return;
    if (n >= avail) n = avail - 1;
    b->len += n;
}

/* ================================================================ row primitives */

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

static void open_row(buf_t *b, int zebra) {
    b->bg = (zebra & 1) ? ANSI_BG_ZEBRA : "";
    buf_printf(b, ANSI_DIM SYM_BOX_V ANSI_RESET "%s", b->bg);
}

static void close_row(buf_t *b) {
    buf_printf(b, ANSI_DIM SYM_BOX_V ANSI_RESET ANSI_EOL "\r\n");
}

static void gap(buf_t *b) { buf_printf(b, "   "); }

static void col(buf_t *b, int width, bool right_align,
                const char *style, const char *text) {
    if (!text) text = "";
    int cols = utf8_cols(text);
    int pad  = width - cols;
    bool styled = style && *style;

    if (styled) buf_printf(b, "%s", style);
    if (cols > width) {
        buf_printf(b, "%-*.*s", width, width, text);
    } else if (right_align) {
        if (pad > 0) buf_printf(b, "%*s", pad, "");
        buf_printf(b, "%s", text);
    } else {
        buf_printf(b, "%s", text);
        if (pad > 0) buf_printf(b, "%*s", pad, "");
    }
    if (styled) buf_printf(b, ANSI_RESET "%s", b->bg);
}

/* ================================================================ status glyph */

typedef struct { const char *color, *text; } glyph_t;

static const glyph_t *status_glyph(atc_status_t st) {
    static const glyph_t map[] = {
        [ATC_ST_NONE] = { "",           ""          },
        [ATC_ST_OK]   = { ANSI_FG_OK,   SYM_ST_OK   },
        [ATC_ST_WARN] = { ANSI_FG_WARN, SYM_ST_WARN },
        [ATC_ST_ERR]  = { ANSI_FG_ERR,  SYM_ST_ERR  },
        [ATC_ST_ON]   = { ANSI_FG_OK,   SYM_ST_ON   },
        [ATC_ST_OFF]  = { ANSI_DIM,     SYM_ST_OFF  },
    };
    size_t i = (size_t)st;
    return &map[i < sizeof map / sizeof map[0] ? i : ATC_ST_NONE];
}

/* ================================================================ chrome */

static void box_edge(const char *left, const char *right) {
    buf_t b = buf_begin();
    buf_printf(&b, ANSI_DIM "%s", left);
    for (int i = 0; i < INNER_W; i++) buf_printf(&b, "%s", SYM_BOX_H);
    buf_printf(&b, "%s" ANSI_RESET ANSI_EOL "\r\n", right);
    buf_flush(&b);
}

static void box_top(const char *title, const char *version) {
    buf_t b = buf_begin();
    buf_printf(&b, "%s", ANSI_DIM SYM_BOX_TL SYM_BOX_H);
    int used = 1;

    int tl = title   ? (int)strlen(title)   : 0;
    int vl = version ? (int)strlen(version) : 0;
    int reserve_right = vl ? (vl + 3) : 0;

    if (title && used + tl + 2 + reserve_right <= INNER_W) {
        buf_printf(&b, " %s ", title);
        used += tl + 2;
    }

    int dashes_end = INNER_W - reserve_right;
    for (; used < dashes_end; used++) buf_printf(&b, "%s", SYM_BOX_H);

    if (version && used + reserve_right <= INNER_W) {
        buf_printf(&b, " %s ", version);
        used += vl + 2;
    }

    for (; used < INNER_W; used++) buf_printf(&b, "%s", SYM_BOX_H);
    buf_printf(&b, "%s", SYM_BOX_TR ANSI_RESET ANSI_EOL "\r\n");
    buf_flush(&b);
}

static void info_row(const char *l, const char *r,
                     const char *l_color, const char *r_color) {
    int lw = l ? (int)strlen(l) : 0;
    int rw = r ? (int)strlen(r) : 0;
    if (rw > NOTE_W)      rw = NOTE_W;
    if (lw > NOTE_W - rw) lw = NOTE_W - rw;
    int gap_w = NOTE_W - lw - rw;

    buf_t b = buf_begin();
    buf_printf(&b, ANSI_DIM SYM_BOX_V ANSI_RESET);
    if (l_color && *l_color) buf_printf(&b, "%s", l_color);
    buf_printf(&b, "%.*s" ANSI_RESET, lw, l ? l : "");
    buf_printf(&b, "%*s", gap_w, "");
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

/* Notes render without zebra striping; the first one gets a dotted
 * separator above it (see render_all). */
static void render_note(const atc_menu_item_t *it) {
    buf_t b = buf_begin();
    open_row(&b, 0);
    col(&b, NOTE_W, false, ANSI_DIM, it->label);
    close_row(&b);
    buf_flush(&b);
}

static void note_separator(void) {
    buf_t b = buf_begin();
    buf_printf(&b, ANSI_DIM SYM_BOX_V);
    for (int i = 0; i < INNER_W; i++) buf_printf(&b, "%s", SYM_BOX_DOT);
    buf_printf(&b, SYM_BOX_V ANSI_RESET ANSI_EOL "\r\n");
    buf_flush(&b);
}

/* NOTE rows are required to be last, so checking the tail is enough. */
static bool has_notes(void) {
    size_t count = nav_count();
    return count > 0 && nav_items()[count - 1].type == ATC_ROW_NOTE;
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

/* Line (1-based) of the box's bottom border. Header + rows + note
 * separator, then the closing border one line below. Every cursor anchor
 * derives from this so the footer/park math has a single source of truth. */
static int box_bottom_line(void) {
    return HEADER_LINES + 1 + (int)nav_count() + (has_notes() ? 1 : 0);
}

int render_footer_anchor_line(void) {
    return box_bottom_line() + 1;
}

void menu_park_cursor(void) {
    menu_printf(ANSI_GOTO_FMT, box_bottom_line() + 3);
}

/* ================================================================ row renderers */

static void scalar_row(int z, const atc_menu_item_t *it,
                       const char *val_style, const char *val,
                       const char *unit, atc_status_t st) {
    const glyph_t *g = status_glyph(st);
    char k[2] = { it->key ? it->key : ' ', 0 };

    buf_t b = buf_begin();
    open_row(&b, z);
    col(&b, KEY_W,    false, ANSI_FG_KEY, k);                          gap(&b);
    col(&b, LABEL_W,  false, NULL,        it->label);                  gap(&b);
    col(&b, VALUE_W,  true,  val_style ? val_style : ANSI_FG_VAL, val); gap(&b);
    col(&b, UNIT_W,   false, ANSI_DIM,    unit ? unit : it->unit);     gap(&b);
    col(&b, STATUS_W, false, g->color,    g->text);
    close_row(&b);
    buf_flush(&b);
}

static void render_value(int z, const atc_menu_item_t *it) {
    char val[READ_BUF] = {0};
    atc_status_t st = ATC_ST_NONE;
    if (it->read) it->read(val, sizeof val, &st);
    scalar_row(z, it, NULL, val, NULL, st);
}

static void render_group(int z, const atc_menu_item_t *it) {
    char k[2] = { it->key ? it->key : ' ', 0 };
    buf_t b = buf_begin();
    open_row(&b, z);
    col(&b, KEY_W,         false, ANSI_FG_KEY, k);
    gap(&b);
    col(&b, GROUP_LABEL_W, false, ANSI_BOLD,   it->label);
    close_row(&b);
    buf_flush(&b);
}

static void render_submenu(int z, const atc_menu_item_t *it) {
    char k[2] = { it->key ? it->key : ' ', 0 };
    buf_t b = buf_begin();
    open_row(&b, z);
    col(&b, KEY_W,            false, ANSI_FG_KEY, k);
    gap(&b);
    col(&b, SUBMENU_PROMPT_W, false, ANSI_FG_KEY, SYM_SUBMENU);
    col(&b, SUBMENU_LABEL_W,  false, NULL,        it->label);
    close_row(&b);
    buf_flush(&b);
}

/* BAR */

#define BAR_CELLS  (VALUE_W - 2)
#define BAR_SUB    8
#define BAR_LEVELS (BAR_CELLS * BAR_SUB)

static const char *const BAR_PARTIAL[BAR_SUB] = {
    "", "▏", "▎", "▍", "▌", "▋", "▊", "▉",
};

static int clamp_pct(long pct) {
    return pct < 0 ? 0 : pct > 100 ? 100 : (int)pct;
}

static const char *bar_color(atc_status_t st) {
    return (st == ATC_ST_WARN) ? ANSI_FG_WARN
         : (st == ATC_ST_ERR)  ? ANSI_FG_ERR
                               : ANSI_FG_OK;
}

static char *append(char *dst, const char *src) {
    while (*src) *dst++ = *src++;
    return dst;
}

static void compose_bar(int pct, char *bar, char *unit, size_t unit_cap) {
    int subs = (pct * BAR_LEVELS + 50) / 100;
    int full = subs / BAR_SUB;
    int part = subs % BAR_SUB;

    char *p = append(bar, SYM_BAR_LBR);
    for (int i = 0; i < full; i++) p = append(p, SYM_BAR_FILL);
    if (part) p = append(p, BAR_PARTIAL[part]);
    for (int i = full + (part > 0); i < BAR_CELLS; i++) p = append(p, SYM_BAR_EMPTY);
    p = append(p, SYM_BAR_RBR);
    *p = '\0';

    atc_snprintf(unit, unit_cap, "%d %%", pct);
}

static void render_bar(int z, const atc_menu_item_t *it) {
    char raw[READ_BUF] = {0};
    atc_status_t st = ATC_ST_NONE;
    if (it->read) it->read(raw, sizeof raw, &st);

    long raw_val = 0;
    atc_parse_long(raw, 10, &raw_val);
    int pct = clamp_pct(raw_val);
    char bar[VALUE_BUF];
    char unit[8];
    compose_bar(pct, bar, unit, sizeof unit);
    scalar_row(z, it, bar_color(st), bar, unit, st);
}

/* CHOICE */

static void format_choice_box(const char *sel, char *out, size_t cap) {
    int sl = (int)strlen(sel);
    if (sl > CHOICE_STR_MAX) sl = CHOICE_STR_MAX;
    int lpad = (CHOICE_STR_MAX - sl) / 2;
    int rpad = CHOICE_STR_MAX - sl - lpad;
    atc_snprintf(out, cap, "%s %*s%.*s%*s %s",
                 SYM_CHOICE_LBR, lpad, "", sl, sel, rpad, "", SYM_CHOICE_RBR);
}

static void render_choice(int z, const atc_menu_item_t *it,
                          const char *override, bool editing) {
    char raw[READ_BUF] = {0};
    atc_status_t st = ATC_ST_OK;
    if (it->read) it->read(raw, sizeof raw, &st);

    const char *sel = override ? override
        : (it->choice_idx && *it->choice_idx < it->choice_count
           && it->choices[*it->choice_idx])
            ? it->choices[*it->choice_idx] : "";

    char box[VALUE_BUF];
    format_choice_box(sel, box, sizeof box);
    scalar_row(z, it, editing ? ANSI_BOLD ANSI_FG_VAL : NULL, box, NULL, st);
}

/* INPUT edit */

static void render_input_edit(int z, const atc_menu_item_t *it, const char *edit) {
    char k[2] = { it->key ? it->key : ' ', 0 };
    buf_t b = buf_begin();
    open_row(&b, z);
    col(&b, KEY_W,        false, ANSI_FG_KEY, k);
    gap(&b);
    col(&b, LABEL_W,      false, NULL,        it->label);
    gap(&b);
    col(&b, INPUT_EDIT_W, false, ANSI_FG_VAL, edit);
    close_row(&b);
    buf_flush(&b);
}

/* ================================================================ dispatch */

static void render_item(int z, const atc_menu_item_t *it) {
    switch (it->type) {
        case ATC_ROW_GROUP:    render_group(z, it);    return;
        case ATC_ROW_SUBMENU:  render_submenu(z, it);  return;
        case ATC_ROW_VALUE:
        case ATC_ROW_STATE:
        case ATC_ROW_ACTION:
        case ATC_ROW_INPUT:    render_value(z, it);    return;
        case ATC_ROW_BAR:      render_bar(z, it);      return;
        case ATC_ROW_CHOICE:   render_choice(z, it, NULL, false); return;
        case ATC_ROW_NOTE:     render_note(it);        return;
    }
}

static int row_y(size_t index) { return HEADER_LINES + 1 + (int)index; }

static void park_at_row(size_t index) {
    menu_printf(ANSI_GOTO_FMT, row_y(index));
    menu_printf("%s", ANSI_EOL);
}

void render_all(void) {
    if (!nav_items() || !menu_port()) return;

    menu_printf("%s", ANSI_HOME);
    render_header();

    const atc_menu_item_t *items = nav_items();
    size_t                 count = nav_count();
    for (size_t i = 0; i < count; i++) {
        if (items[i].type == ATC_ROW_NOTE
            && (i == 0 || items[i - 1].type != ATC_ROW_NOTE))
            note_separator();
        render_item((int)i, &items[i]);
    }

    box_edge(SYM_BOX_BL, SYM_BOX_BR);
}

void render_row(size_t index) {
    park_at_row(index);
    render_item((int)index, &nav_items()[index]);
    menu_park_cursor();
}

void render_row_input(size_t index, const char *edit_text) {
    park_at_row(index);
    render_input_edit((int)index, &nav_items()[index], edit_text);
    menu_park_cursor();
}

void render_row_choice(size_t index, const char *pending_label) {
    park_at_row(index);
    render_choice((int)index, &nav_items()[index], pending_label, true);
    menu_park_cursor();
}

void render_group_span(size_t start) {
    const atc_menu_item_t *items = nav_items();
    size_t                 count = nav_count();
    size_t                 end   = start + 1;
    while (end < count && items[end].type != ATC_ROW_GROUP
                       && items[end].type != ATC_ROW_NOTE) end++;

    park_at_row(start);
    for (size_t i = start; i < end; i++) render_item((int)i, &items[i]);
    menu_park_cursor();
}

/* ================================================================ validation */

static void warn_label_unit(const atc_menu_item_t *it) {
    if (it->label && strlen(it->label) > LABEL_W)
        menu_printf("WARN: label '%s' exceeds %d cols\r\n", it->label, LABEL_W);
    if (it->unit && strlen(it->unit) > UNIT_W)
        menu_printf("WARN: unit '%s' exceeds %d cols\r\n", it->unit, UNIT_W);
}

void render_validate_item(const atc_menu_item_t *it) {
    char k = it->key;
    switch (it->type) {
        case ATC_ROW_GROUP:
            if (it->label && strlen(it->label) > GROUP_LABEL_W)
                menu_printf("WARN: GROUP label '%s' exceeds %d cols\r\n",
                            it->label, GROUP_LABEL_W);
            return;
        case ATC_ROW_SUBMENU:
            if (!it->submenu || !it->submenu->items || it->submenu->count == 0)
                menu_printf("WARN: ATC_ROW_SUBMENU '%c' missing submenu\r\n", k);
            if (it->label && strlen(it->label) > SUBMENU_LABEL_W)
                menu_printf("WARN: SUBMENU label '%s' exceeds %d cols\r\n",
                            it->label, SUBMENU_LABEL_W);
            if (it->submenu && it->submenu->items)
                for (size_t i = 0; i < it->submenu->count; i++)
                    if (it->submenu->items[i].type == ATC_ROW_NOTE)
                        render_validate_item(&it->submenu->items[i]);
            return;
        case ATC_ROW_VALUE:
            if (!it->read) menu_printf("WARN: ATC_ROW_VALUE '%c' missing read\r\n", k);
            warn_label_unit(it); return;
        case ATC_ROW_STATE:
            if (!it->read || !it->action)
                menu_printf("WARN: ATC_ROW_STATE '%c' missing read/action\r\n", k);
            warn_label_unit(it); return;
        case ATC_ROW_ACTION:
            if (!it->action)
                menu_printf("WARN: ATC_ROW_ACTION '%c' missing action\r\n", k);
            warn_label_unit(it); return;
        case ATC_ROW_BAR:
            if (!it->read) menu_printf("WARN: ATC_ROW_BAR '%c' missing read\r\n", k);
            return;
        case ATC_ROW_CHOICE:
            if (!it->choices || it->choice_count == 0 || !it->choice_idx) {
                menu_printf("WARN: ATC_ROW_CHOICE '%c' missing choices/idx\r\n", k);
                return;
            }
            for (uint8_t c = 0; c < it->choice_count; c++)
                if (it->choices[c] && strlen(it->choices[c]) > CHOICE_STR_MAX)
                    menu_printf("WARN: CHOICE '%c' choice '%s' exceeds %d cols\r\n",
                                k, it->choices[c], CHOICE_STR_MAX);
            return;
        case ATC_ROW_INPUT:
            if (!it->read || !it->input_commit)
                menu_printf("WARN: ATC_ROW_INPUT '%c' missing read/input_commit\r\n", k);
            if ((it->input_type == ATC_INPUT_INT || it->input_type == ATC_INPUT_HEX)
                && it->input_min > it->input_max)
                menu_printf("WARN: ATC_ROW_INPUT '%c' min > max\r\n", k);
            warn_label_unit(it); return;
        case ATC_ROW_NOTE:
            if (it->label && (int)strlen(it->label) > NOTE_W)
                menu_printf("WARN: note exceeds %d cols: '%s'\r\n", NOTE_W, it->label);
            return;
    }
}
