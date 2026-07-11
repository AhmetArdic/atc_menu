/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Whole framework in one translation unit: libc-free formatting, the
 * navigation stack, the renderer, and key handling. The public API and
 * data model live in atc_menu.h; layout tunables in layout.h.
 */

#include "atc_menu/atc_menu.h"
#include "layout.h"

#include <limits.h>
#include <stdarg.h>
#include <string.h>

/* ================================================================ style */

#define ANSI_RESET     "\033[0m"
#define ANSI_DIM       "\033[2m"
#define ANSI_BOLD      "\033[1m"
#define ANSI_FG_OK     "\033[32m"
#define ANSI_FG_WARN   "\033[33m"
#define ANSI_FG_ERR    "\033[31m"
#define ANSI_FG_VAL    "\033[36m"
#define ANSI_FG_KEY    "\033[1;35m"
#define ANSI_BG_ZEBRA  "\033[48;5;236m"
#define ANSI_HOME      "\033[H"
#define ANSI_EOL       "\033[K"
#define ANSI_CLR_BELOW "\033[J"
#define ANSI_GOTO_FMT  "\033[%d;1H"

/* UTF-8 glyph set; every glyph is one display column. */

#define SYM_PROMPT       "❯ "   /* status line, cmd/input editor, submenu marker */
#define SYM_CRUMB        " › "
#define SYM_CRUMB_ROOT   "Home"

#define SYM_BOX_H        "─"
#define SYM_BOX_V        "│"
#define SYM_BOX_TL       "╭"
#define SYM_BOX_TR       "╮"
#define SYM_BOX_BL       "╰"
#define SYM_BOX_BR       "╯"
#define SYM_BOX_LJ       "├"
#define SYM_BOX_RJ       "┤"
#define SYM_BOX_DOT      "┄"

#define SYM_ST_OK        "✓"
#define SYM_ST_WARN      "▲"
#define SYM_ST_ERR       "✕"
#define SYM_ST_ON        "●"
#define SYM_ST_OFF       "○"

#define SYM_BAR_FILL     "█"
#define SYM_BAR_EMPTY    " "
#define SYM_BAR_LBR      "▕"
#define SYM_BAR_RBR      "▏"
#define SYM_CHOICE_LBR   "❮"
#define SYM_CHOICE_RBR   "❯"
#define SYM_INPUT_CURSOR "▏"

enum { KEY_BS = 8, KEY_ESC = 27, KEY_DEL = 127 };

#define KEY_REFRESH 'r'
#define KEY_BACK    'b'
#define KEY_PATH    '?'
#define KEY_CMD     ':'

/* ================================================================ fmt
 *
 * libc-free vsnprintf replacement covering exactly what the menu uses:
 * %%, %c, %s, %d, %ld, '-' flag, '*'/numeric width, '.*'/'.N' precision
 * (%s only). Termination and return value match vsnprintf.
 */

typedef struct { char *out; size_t cap, len; } sink_t;

static void emit(sink_t *s, char c) {
    if (s->cap && s->len + 1 < s->cap) s->out[s->len] = c;
    s->len++;
}

static void emit_field(sink_t *s, const char *body, int blen, int width, bool left) {
    int pad = width - blen;
    if (!left) while (pad-- > 0) emit(s, ' ');
    for (int i = 0; i < blen; i++) emit(s, body[i]);
    if (left)  while (pad-- > 0) emit(s, ' ');
}

/* Accumulates in unsigned to stay defined at LONG_MIN. */
static int long_to_str(char *buf, long v) {
    char tmp[24];
    int  i = 0;
    bool neg = v < 0;
    unsigned long u = neg ? -(unsigned long)v : (unsigned long)v;
    do { tmp[i++] = (char)('0' + (u % 10)); u /= 10; } while (u);

    int n = 0;
    if (neg) buf[n++] = '-';
    while (i > 0) buf[n++] = tmp[--i];
    return n;
}

static int parse_int(const char **fmt) {
    int v = 0;
    while (**fmt >= '0' && **fmt <= '9') v = v * 10 + (*(*fmt)++ - '0');
    return v;
}

static int atc_vsnprintf(char *out, size_t cap, const char *fmt, va_list ap) {
    sink_t s = { out, cap, 0 };

    for (; *fmt; fmt++) {
        if (*fmt != '%') { emit(&s, *fmt); continue; }

        fmt++;
        if (*fmt == '%') { emit(&s, '%'); continue; }

        bool left = false;
        if (*fmt == '-') { left = true; fmt++; }

        int width;
        if (*fmt == '*') { width = va_arg(ap, int); fmt++; }
        else             width = parse_int(&fmt);
        if (width < 0) { left = true; width = -width; }

        int prec = -1;
        if (*fmt == '.') {
            fmt++;
            if (*fmt == '*') { prec = va_arg(ap, int); fmt++; }
            else             prec = parse_int(&fmt);
        }

        bool is_long = false;
        if (*fmt == 'l') { is_long = true; fmt++; }

        char numbuf[24];
        switch (*fmt) {
            case 's': {
                const char *p = va_arg(ap, const char *);
                if (!p) p = "";
                int n = 0;
                while ((prec < 0 || n < prec) && p[n]) n++;
                emit_field(&s, p, n, width, left);
                break;
            }
            case 'c': {
                char c = (char)va_arg(ap, int);
                emit_field(&s, &c, 1, width, left);
                break;
            }
            case 'd': {
                long v = is_long ? va_arg(ap, long) : va_arg(ap, int);
                int  n = long_to_str(numbuf, v);
                emit_field(&s, numbuf, n, width, left);
                break;
            }
            default: /* unknown conversion: surface it rather than swallow */
                emit(&s, '%');
                if (*fmt) emit(&s, *fmt);
                break;
        }
    }

    if (cap) out[s.len < cap ? s.len : cap - 1] = '\0';
    return (int)s.len;
}

static int atc_snprintf(char *out, size_t cap, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = atc_vsnprintf(out, cap, fmt, ap);
    va_end(ap);
    return n;
}

/* Rejects empty input, trailing garbage, and overflow. */
static bool atc_parse_long(const char *s, int base, long *out) {
    if (!s) return false;

    bool neg = false;
    if (*s == '+' || *s == '-') neg = (*s++ == '-');
    if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    if (!*s) return false;

    unsigned long limit = neg ? (unsigned long)LONG_MAX + 1UL : (unsigned long)LONG_MAX;
    unsigned long acc   = 0;
    for (; *s; s++) {
        unsigned d;
        if      (*s >= '0' && *s <= '9')                d = (unsigned)(*s - '0');
        else if (base == 16 && *s >= 'a' && *s <= 'f')  d = (unsigned)(*s - 'a' + 10);
        else if (base == 16 && *s >= 'A' && *s <= 'F')  d = (unsigned)(*s - 'A' + 10);
        else return false;
        if (acc > (limit - d) / (unsigned)base) return false;
        acc = acc * (unsigned)base + d;
    }

    *out = (long)(neg ? 0UL - acc : acc);
    return true;
}

/* ================================================================ port */

static const atc_menu_port_t *g_port;
static const atc_menu_info_t *g_info;

static int menu_printf(const char *fmt, ...) {
    if (!g_port || !g_port->write) return 0;
    static char buf[ROW_BUF]; /* non-reentrant: keep the row buffer off the stack */
    va_list ap;
    va_start(ap, fmt);
    int n = atc_vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0) return n;
    size_t len = (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1;
    g_port->write(buf, len);
    return (int)len;
}

/* ================================================================ nav */

typedef struct {
    const atc_menu_table_t *table;
    const char             *label;
} frame_t;

static const atc_menu_table_t *g_table;
static const char             *g_active_label;
static frame_t                 g_stack[NAV_DEPTH];
static size_t                  g_depth;

static const atc_menu_item_t *nav_items(void) { return g_table ? g_table->items : NULL; }
static size_t                 nav_count(void) { return g_table ? g_table->count : 0; }

static void nav_push(const atc_menu_item_t *opener) {
    const atc_menu_table_t *t = opener->submenu;
    if (!t || !t->items || t->count == 0) {
        menu_printf("WARN: SUBMENU '%c' has no submenu table\r\n", opener->key);
        return;
    }
    if (g_depth >= NAV_DEPTH) {
        menu_printf("WARN: nav stack full (depth %d)\r\n", NAV_DEPTH);
        return;
    }
    g_stack[g_depth].table = g_table;
    g_stack[g_depth].label = g_active_label;
    g_depth++;
    g_table        = t;
    g_active_label = opener->label;
}

static void nav_pop(void) {
    if (g_depth == 0) return;
    g_depth--;
    g_table        = g_stack[g_depth].table;
    g_active_label = g_stack[g_depth].label;
}

static void nav_show_path(void) {
    char   b[ROW_BUF];
    size_t n = 0;
    int    w = atc_snprintf(b, sizeof b, "%s", SYM_CRUMB_ROOT);
    if (w > 0) n = (size_t)w < sizeof b ? (size_t)w : sizeof b - 1;

    for (size_t i = 0; i <= g_depth && n + 1 < sizeof b; i++) {
        const char *lbl = (i < g_depth) ? g_stack[i].label : g_active_label;
        if (!lbl) continue;
        w = atc_snprintf(b + n, sizeof b - n, "%s%s", SYM_CRUMB, lbl);
        if (w > 0) n += (size_t)w < sizeof b - n ? (size_t)w : sizeof b - n - 1;
    }

    menu_printf("\r\n" ANSI_DIM "%.*s" ANSI_EOL ANSI_RESET "\r\n", (int)n, b);
}

/* ================================================================ row buffer */

typedef struct {
    char       *data;
    int         len;
    const char *bg;
} buf_t;

/* One row is composed at a time, so every buf_t shares this scratch. */
static char g_rowbuf[ROW_BUF];

static buf_t buf_begin(void) {
    buf_t b = { g_rowbuf, 0, "" };
    return b;
}

static void buf_flush(buf_t *b) {
    if (g_port && g_port->write && b->len > 0) g_port->write(b->data, (size_t)b->len);
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

static buf_t row_open(int zebra) {
    buf_t b = buf_begin();
    b.bg = (zebra & 1) ? ANSI_BG_ZEBRA : "";
    buf_printf(&b, ANSI_DIM SYM_BOX_V ANSI_RESET "%s", b.bg);
    return b;
}

static void row_close(buf_t *b) {
    buf_printf(b, ANSI_DIM SYM_BOX_V ANSI_RESET ANSI_EOL "\r\n");
    buf_flush(b);
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

static void key_col(buf_t *b, const atc_menu_item_t *it) {
    char k[2] = { it->key ? it->key : ' ', 0 };
    col(b, KEY_W, false, ANSI_FG_KEY, k);
    gap(b);
}

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

/* Full-width line: left glyph, INNER_W × mid, right glyph. */
static void fill_line(const char *left, const char *mid, const char *right) {
    buf_t b = buf_begin();
    buf_printf(&b, ANSI_DIM "%s", left);
    for (int i = 0; i < INNER_W; i++) buf_printf(&b, "%s", mid);
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
    const char *project = (g_info && g_info->project) ? g_info->project : "atc menu";
    const char *version = g_info ? g_info->version : NULL;
    const char *author  = g_info ? g_info->author  : NULL;
    const char *build   = g_info ? g_info->build   : NULL;

    box_top(author, build);
    info_row(project, version, ANSI_BOLD ANSI_FG_KEY, ANSI_FG_VAL);
    fill_line(SYM_BOX_LJ, SYM_BOX_H, SYM_BOX_RJ);
}

static void render_default_footer(bool show_back) {
    if (show_back)
        menu_printf("\r\n" ANSI_DIM
            "[r] refresh  [b] back  [?] path  [:] cmd"
            ANSI_RESET ANSI_EOL "\r\n");
    else
        menu_printf("\r\n" ANSI_DIM
            "[r] refresh  [:] cmd"
            ANSI_RESET ANSI_EOL "\r\n");
}

/* Line (1-based) of the box's bottom border; every cursor anchor derives
 * from this. NOTE rows must be last, so checking the tail is enough. */
static int box_bottom_line(void) {
    size_t count = nav_count();
    bool   notes = count > 0 && nav_items()[count - 1].type == ATC_ROW_NOTE;
    return HEADER_LINES + 1 + (int)count + (notes ? 1 : 0);
}

static void park_cursor(void) { menu_printf(ANSI_GOTO_FMT, box_bottom_line() + 3); }

static void park_at_row(size_t index) {
    menu_printf(ANSI_GOTO_FMT, HEADER_LINES + 1 + (int)index);
    menu_printf("%s", ANSI_EOL);
}

/* ================================================================ row renderers */

static void scalar_row(int z, const atc_menu_item_t *it,
                       const char *val_style, const char *val,
                       const char *unit, atc_status_t st) {
    const glyph_t *g = status_glyph(st);

    buf_t b = row_open(z);
    key_col(&b, it);
    col(&b, LABEL_W,  false, NULL,        it->label);                   gap(&b);
    col(&b, VALUE_W,  true,  val_style ? val_style : ANSI_FG_VAL, val); gap(&b);
    col(&b, UNIT_W,   false, ANSI_DIM,    unit ? unit : it->unit);      gap(&b);
    col(&b, STATUS_W, false, g->color,    g->text);
    row_close(&b);
}

static void render_value(int z, const atc_menu_item_t *it) {
    char val[READ_BUF] = {0};
    atc_status_t st = ATC_ST_NONE;
    if (it->read) it->read(val, sizeof val, &st);
    scalar_row(z, it, NULL, val, NULL, st);
}

static void render_group(int z, const atc_menu_item_t *it) {
    buf_t b = row_open(z);
    key_col(&b, it);
    col(&b, GROUP_LABEL_W, false, ANSI_BOLD, it->label);
    row_close(&b);
}

static void render_submenu(int z, const atc_menu_item_t *it) {
    buf_t b = row_open(z);
    key_col(&b, it);
    col(&b, SUBMENU_PROMPT_W, false, ANSI_FG_KEY, SYM_PROMPT);
    col(&b, SUBMENU_LABEL_W,  false, NULL,        it->label);
    row_close(&b);
}

/* No zebra; render_rows draws a dotted separator above the first one. */
static void render_note(const atc_menu_item_t *it) {
    buf_t b = row_open(0);
    col(&b, NOTE_W, false, ANSI_DIM, it->label);
    row_close(&b);
}

static void render_input_edit(int z, const atc_menu_item_t *it, const char *edit) {
    buf_t b = row_open(z);
    key_col(&b, it);
    col(&b, LABEL_W,      false, NULL,        it->label);
    gap(&b);
    col(&b, INPUT_EDIT_W, false, ANSI_FG_VAL, edit);
    row_close(&b);
}

#define BAR_CELLS  (VALUE_W - 2)
#define BAR_SUB    8
#define BAR_LEVELS (BAR_CELLS * BAR_SUB)

static const char *const BAR_PARTIAL[BAR_SUB] = {
    "", "▏", "▎", "▍", "▌", "▋", "▊", "▉",
};

static char *append(char *dst, const char *src) {
    while (*src) *dst++ = *src++;
    return dst;
}

static void render_bar(int z, const atc_menu_item_t *it) {
    char raw[READ_BUF] = {0};
    atc_status_t st = ATC_ST_NONE;
    if (it->read) it->read(raw, sizeof raw, &st);

    long v = 0;
    atc_parse_long(raw, 10, &v);
    int pct  = v < 0 ? 0 : v > 100 ? 100 : (int)v;
    int subs = (pct * BAR_LEVELS + 50) / 100;
    int full = subs / BAR_SUB;
    int part = subs % BAR_SUB;

    char bar[VALUE_BUF];
    char *p = append(bar, SYM_BAR_LBR);
    for (int i = 0; i < full; i++) p = append(p, SYM_BAR_FILL);
    if (part) p = append(p, BAR_PARTIAL[part]);
    for (int i = full + (part > 0); i < BAR_CELLS; i++) p = append(p, SYM_BAR_EMPTY);
    p = append(p, SYM_BAR_RBR);
    *p = '\0';

    char unit[8];
    atc_snprintf(unit, sizeof unit, "%d %%", pct);

    const char *color = (st == ATC_ST_WARN) ? ANSI_FG_WARN
                      : (st == ATC_ST_ERR)  ? ANSI_FG_ERR
                                            : ANSI_FG_OK;
    scalar_row(z, it, color, bar, unit, st);
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

    int sl = (int)strlen(sel);
    if (sl > CHOICE_STR_MAX) sl = CHOICE_STR_MAX;
    int lpad = (CHOICE_STR_MAX - sl) / 2;
    int rpad = CHOICE_STR_MAX - sl - lpad;
    char box[VALUE_BUF];
    atc_snprintf(box, sizeof box, "%s %*s%.*s%*s %s",
                 SYM_CHOICE_LBR, lpad, "", sl, sel, rpad, "", SYM_CHOICE_RBR);

    scalar_row(z, it, editing ? ANSI_BOLD ANSI_FG_VAL : NULL, box, NULL, st);
}

/* ================================================================ screen render */

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

static void render_row(size_t index) {
    park_at_row(index);
    render_item((int)index, &nav_items()[index]);
    park_cursor();
}

/* All data rows in order; assumes the cursor is on the first row's line. */
static void render_rows(void) {
    const atc_menu_item_t *items = nav_items();
    size_t                 count = nav_count();
    for (size_t i = 0; i < count; i++) {
        if (items[i].type == ATC_ROW_NOTE
            && (i == 0 || items[i - 1].type != ATC_ROW_NOTE))
            fill_line(SYM_BOX_V, SYM_BOX_DOT, SYM_BOX_V);
        render_item((int)i, &items[i]);
    }
}

static void render_group_span(size_t start) {
    const atc_menu_item_t *items = nav_items();
    size_t                 count = nav_count();
    size_t                 end   = start + 1;
    while (end < count && items[end].type != ATC_ROW_GROUP
                       && items[end].type != ATC_ROW_NOTE) end++;

    park_at_row(start);
    for (size_t i = start; i < end; i++) render_item((int)i, &items[i]);
    park_cursor();
}

/* ================================================================ validation */

/* Required callbacks per scalar row type. */
static const struct {
    const char *name, *missing;
    bool        need_read, need_action, need_commit;
} g_req[] = {
    [ATC_ROW_VALUE]  = { "VALUE",  "read",              true,  false, false },
    [ATC_ROW_STATE]  = { "STATE",  "read/action",       true,  true,  false },
    [ATC_ROW_ACTION] = { "ACTION", "action",            false, true,  false },
    [ATC_ROW_BAR]    = { "BAR",    "read",              true,  false, false },
    [ATC_ROW_INPUT]  = { "INPUT",  "read/input_commit", true,  false, true  },
};

static void validate_item(const atc_menu_item_t *it) {
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
                        validate_item(&it->submenu->items[i]);
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
        case ATC_ROW_NOTE:
            if (it->label && (int)strlen(it->label) > NOTE_W)
                menu_printf("WARN: note exceeds %d cols: '%s'\r\n", NOTE_W, it->label);
            return;
        default:
            break;
    }

    if ((g_req[it->type].need_read   && !it->read)
     || (g_req[it->type].need_action && !it->action)
     || (g_req[it->type].need_commit && !it->input_commit))
        menu_printf("WARN: ATC_ROW_%s '%c' missing %s\r\n",
                    g_req[it->type].name, k, g_req[it->type].missing);

    if (it->type == ATC_ROW_INPUT
        && (it->input_type == ATC_INPUT_INT || it->input_type == ATC_INPUT_HEX)
        && it->input_min > it->input_max)
        menu_printf("WARN: ATC_ROW_INPUT '%c' min > max\r\n", k);

    if (it->type != ATC_ROW_BAR) {
        if (it->label && strlen(it->label) > LABEL_W)
            menu_printf("WARN: label '%s' exceeds %d cols\r\n", it->label, LABEL_W);
        if (it->unit && strlen(it->unit) > UNIT_W)
            menu_printf("WARN: unit '%s' exceeds %d cols\r\n", it->unit, UNIT_W);
    }
}

/* ================================================================ modal state */

typedef enum { MODE_NAV, MODE_CMD, MODE_INPUT, MODE_CHOICE } mode_t;

enum { EDIT_BUF = CMD_BUF > INPUT_BUF ? CMD_BUF : INPUT_BUF };

static struct {
    mode_t                 mode;
    const atc_menu_item_t *item;         /* INPUT/CHOICE row being edited */
    size_t                 index;        /* its row index */
    char                   buf[EDIT_BUF]; /* CMD/INPUT edit text */
    uint8_t                len;
    uint8_t                pending;      /* CHOICE candidate index */
} g_m;

static void mode_exit(void) { g_m.mode = MODE_NAV; }

static void emit_footer(void);

/* Exit the modal editor, restore its row, redraw the footer. */
static void mode_end(void) {
    mode_exit();
    render_row(g_m.index);
    emit_footer();
}

static void emit_footer(void) {
    menu_printf(ANSI_GOTO_FMT, box_bottom_line() + 1);
    menu_printf("%s", ANSI_CLR_BELOW);

    const atc_menu_item_t *it = g_m.item;
    switch (g_m.mode) {
        case MODE_CMD:
            menu_printf("%s", "\r\n" SYM_PROMPT);
            return;
        case MODE_INPUT:
            if (it->input_type == ATC_INPUT_INT || it->input_type == ATC_INPUT_HEX)
                menu_printf("\r\n" ANSI_DIM
                    "[Enter] commit  [Esc] cancel  [BS] erase    range: %ld..%ld"
                    ANSI_RESET ANSI_EOL "\r\n",
                    (long)it->input_min, (long)it->input_max);
            else
                menu_printf("\r\n" ANSI_DIM
                    "[Enter] commit  [Esc] cancel  [BS] erase"
                    ANSI_RESET ANSI_EOL "\r\n");
            return;
        case MODE_CHOICE:
            menu_printf("\r\n" ANSI_DIM
                "[%c] cycle  [Enter] commit  [Esc] cancel"
                ANSI_RESET ANSI_EOL "\r\n", it->key);
            return;
        default:
            render_default_footer(g_depth > 0);
            return;
    }
}

/* ---- cmd mode ---- */

static void cmd_key(char k) {
    if (k == '\r' || k == '\n') {
        g_m.buf[g_m.len] = '\0';
        mode_exit();
        /* Repaint first so a status printed by the handler survives the
         * footer's clear-below, then refresh the rows in place so state
         * the handler changed is visible without touching that status. */
        atc_menu_render();
        if (g_port->cmd) {
            g_port->cmd(g_m.buf);
            park_at_row(0);
            render_rows();
            park_cursor();
        }
        return;
    }
    if (k == KEY_ESC) {
        mode_exit();
        park_cursor();
        menu_printf("%s", ANSI_CLR_BELOW);
        return;
    }
    if (k == KEY_BS || k == KEY_DEL) {
        if (g_m.len) {
            g_m.len--;
            menu_printf("%s", "\b \b");
        }
        return;
    }
    if (g_m.len < CMD_BUF - 1) {
        g_m.buf[g_m.len++] = k;
        g_port->write(&k, 1);
    }
}

/* ---- INPUT mode ---- */

static void input_paint(void) {
    char edit[INPUT_EDIT_BUF];
    atc_snprintf(edit, sizeof edit, "%s%s%s",
                 SYM_PROMPT, g_m.buf, SYM_INPUT_CURSOR);
    park_at_row(g_m.index);
    render_input_edit((int)g_m.index, g_m.item, edit);
    park_cursor();
}

static bool input_acceptable(atc_input_type_t t, char k) {
    switch (t) {
        case ATC_INPUT_INT:
            return (k >= '0' && k <= '9') || k == '-';
        case ATC_INPUT_HEX:
            return (k >= '0' && k <= '9') || (k >= 'a' && k <= 'f')
                || (k >= 'A' && k <= 'F') || k == 'x' || k == 'X';
        case ATC_INPUT_STR:
            return k >= 32 && k <= 126;
    }
    return false;
}

static bool input_validate(void) {
    if (g_m.len == 0) { atc_menu_status("invalid: empty"); return false; }

    atc_input_type_t t = g_m.item->input_type;
    if (t == ATC_INPUT_STR) return true;

    long v;
    if (!atc_parse_long(g_m.buf, t == ATC_INPUT_HEX ? 16 : 10, &v)) {
        atc_menu_status("invalid: parse error");
        return false;
    }
    if (v < g_m.item->input_min || v > g_m.item->input_max) {
        char msg[64];
        atc_snprintf(msg, sizeof msg, "out of range: %ld..%ld",
                     (long)g_m.item->input_min, (long)g_m.item->input_max);
        atc_menu_status(msg);
        return false;
    }
    return true;
}

static void input_enter(const atc_menu_item_t *it, size_t index) {
    g_m.mode   = MODE_INPUT;
    g_m.item   = it;
    g_m.index  = index;
    g_m.len    = 0;
    g_m.buf[0] = '\0';
    input_paint();
    emit_footer();
}

static void input_key(char k) {
    if (k == '\r' || k == '\n') {
        if (!input_validate()) return;
        if (!g_m.item->input_commit || g_m.item->input_commit(g_m.buf))
            mode_end();
        else
            input_paint();
        return;
    }
    if (k == KEY_ESC) { mode_end(); return; }
    if (k == KEY_BS || k == KEY_DEL) {
        if (g_m.len) {
            g_m.buf[--g_m.len] = '\0';
            input_paint();
        }
        return;
    }
    if (g_m.len < INPUT_BUF - 1 && input_acceptable(g_m.item->input_type, k)) {
        g_m.buf[g_m.len++] = k;
        g_m.buf[g_m.len]   = '\0';
        input_paint();
    }
}

/* ---- CHOICE mode ---- */

static void choice_paint(void) {
    const char *label = g_m.item->choices[g_m.pending]
                      ? g_m.item->choices[g_m.pending] : "";
    park_at_row(g_m.index);
    render_choice((int)g_m.index, g_m.item, label, true);
    park_cursor();
}

static void choice_press(const atc_menu_item_t *it, size_t index) {
    if (!it->choice_idx || it->choice_count == 0) return;

    if (it->choice_commit) {
        g_m.mode    = MODE_CHOICE;
        g_m.item    = it;
        g_m.index   = index;
        g_m.pending = *it->choice_idx;
        choice_paint();
        emit_footer();
    } else {
        *it->choice_idx = (uint8_t)((*it->choice_idx + 1) % it->choice_count);
        render_row(index);
    }
}

static void choice_key(char k) {
    if (k == '\r' || k == '\n') {
        *g_m.item->choice_idx = g_m.pending;
        atc_action_fn_t cb = g_m.item->choice_commit;
        mode_exit(); /* leave choice mode before the callback runs */
        if (cb) cb();
        render_row(g_m.index);
        emit_footer();
        return;
    }
    if (k == KEY_ESC) { mode_end(); return; }
    if (k == g_m.item->key) {
        g_m.pending = (uint8_t)((g_m.pending + 1) % g_m.item->choice_count);
        choice_paint();
    }
}

/* ================================================================ public API */

void atc_menu_render(void) {
    if (!g_port || !nav_items()) return;

    menu_printf("%s", ANSI_HOME);
    render_header();
    render_rows();
    fill_line(SYM_BOX_BL, SYM_BOX_H, SYM_BOX_BR);
    emit_footer();
}

void atc_menu_status(const char *msg) {
    menu_printf("\r\n" ANSI_DIM SYM_PROMPT "%s" ANSI_EOL ANSI_RESET "\r\n",
                msg ? msg : "");
}

static const char *reserved_role(char k) {
    return k == KEY_REFRESH ? "built-in refresh"
         : k == KEY_BACK    ? "built-in back"
         : k == KEY_PATH    ? "built-in path"
         : k == KEY_CMD     ? "built-in cmd" : NULL;
}

void atc_menu_init(const atc_menu_table_t *table,
                   const atc_menu_port_t  *port,
                   const atc_menu_info_t  *info) {
    g_port = port;
    g_info = info;

    g_m.mode       = MODE_NAV;
    g_m.len        = 0;
    g_table        = table;
    g_active_label = NULL;
    g_depth        = 0;

    if (!table || !table->items || !port) return;

    const atc_menu_item_t *items = table->items;
    size_t                 count = table->count;
    for (size_t i = 0; i < count; i++) {
        const atc_menu_item_t *a = &items[i];

        if (a->type == ATC_ROW_NOTE && i + 1 < count
            && items[i + 1].type != ATC_ROW_NOTE)
            menu_printf("WARN: NOTE rows must be last\r\n");

        if (a->key) {
            const char *role = reserved_role(a->key);
            if (role) menu_printf("WARN: key '%c' collides with %s\r\n", a->key, role);
        }
        validate_item(a);

        if (!a->key) continue;
        for (size_t j = i + 1; j < count; j++) {
            if (a->key == items[j].key)
                menu_printf("WARN: dup key '%c'\r\n", a->key);
        }
    }
}

static void handle_row_key(const atc_menu_item_t *it, size_t index) {
    switch (it->type) {
        case ATC_ROW_SUBMENU:
            nav_push(it);
            atc_menu_render();
            return;
        case ATC_ROW_GROUP:
            render_group_span(index);
            return;
        case ATC_ROW_INPUT:
            input_enter(it, index);
            return;
        case ATC_ROW_CHOICE:
            choice_press(it, index);
            return;
        default:
            if (it->action) it->action();
            render_row(index);
            return;
    }
}

void atc_menu_handle_key(char k) {
    if (!nav_items() || !g_port) return;

    switch (g_m.mode) {
        case MODE_CMD:    cmd_key(k);    return;
        case MODE_INPUT:  input_key(k);  return;
        case MODE_CHOICE: choice_key(k); return;
        case MODE_NAV:    break;
    }

    switch (k) {
        case KEY_REFRESH: atc_menu_render(); return;
        case KEY_CMD:
            if (g_port->cmd) {
                atc_menu_render();
                g_m.mode = MODE_CMD;
                g_m.len  = 0;
                menu_printf("%s", "\r\n" SYM_PROMPT);
                return;
            }
            break;
        case KEY_BACK:
            if (g_depth > 0) { nav_pop(); atc_menu_render(); return; }
            break;
        case KEY_PATH:
            if (g_depth > 0) { nav_show_path(); return; }
            break;
    }

    const atc_menu_item_t *items = nav_items();
    size_t                 count = nav_count();
    for (size_t i = 0; i < count; i++) {
        if (items[i].key == k) { handle_row_key(&items[i], i); return; }
    }
}
