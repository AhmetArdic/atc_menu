/**
 * @file menu_render.c
 * @brief ATC Menu - page and row renderer. The single built-in look lives here:
 *        pure ASCII, fixed-width rows, zebra striping, right-aligned values.
 * @author Ahmet Talha ARDIC
 * @date   2026-07-31
 */
#include <string.h>

#include "menu_internal.h"

#define ROW_W       ATC_MENU_ROW_W
#define COL_LABEL   7             /* 0-based index where the label starts (column 8) */
#define COL_VAL_END (ROW_W - 2)   /* 1-based column where the value ends */
#define VAL_MAX     ATC_MENU_CFG_VAL
#define TEXT_W      (COL_VAL_END - COL_LABEL)  /* wrap width of a TEXT row */

/* Value scratch: VAL_MAX after clamping, but value_text() writes the full
 * number first ("-2147483.648" = 12) before it is clamped. */
#define VAL_BUF     (VAL_MAX + 2)

#define S_RESET "\x1b[0m"
#define S_ZEBRA "\x1b[48;5;236m"
#define S_NUM   "\x1b[1;33m"
#define S_LBL   "\x1b[22;39m"
#define S_VAL   "\x1b[1;32m"
#define S_TITLE "\x1b[1;37m"
#define S_VER   "\x1b[1;36m"
#define S_AUTH  "\x1b[37m"
#define S_DIM   "\x1b[90m"
#define S_FRAME "\x1b[36m"
#define S_ERR   "\x1b[1;31m"
#define EOL     ATC_MENU_CFG_EOL

/* Bounded line builder: a segment that does not fit is dropped whole, so an
 * escape sequence is never cut in half. */
typedef struct {
    char *p;
    unsigned len;
} sb_t;

static void sb_mem(sb_t *b, const char *s, unsigned n)
{
    if (b->len + n <= ATC_MENU_LINE_BUF) {
        memcpy(b->p + b->len, s, n);
        b->len += n;
    }
}

static void sb_str(sb_t *b, const char *s)
{
    sb_mem(b, s, (unsigned)strlen(s));
}

static void sb_fill(sb_t *b, char c, unsigned n)
{
    if (b->len + n <= ATC_MENU_LINE_BUF) {
        memset(b->p + b->len, c, n);
        b->len += n;
    }
}

static void sb_u32(sb_t *b, uint32_t v)
{
    char t[10];
    sb_mem(b, t, atc_menu_fmt_u32(t, v));
}

static void sb_pos(sb_t *b, unsigned row)
{
    sb_str(b, "\x1b[");
    sb_u32(b, row);
    sb_str(b, ";1H");
}

static const atc_menu_item_t *cur_items(const atc_menu_ctx_t *ctx)
{
    return ctx->nav[ctx->depth].page->items;
}

static atc_menu_u8 cur_start(const atc_menu_ctx_t *ctx)
{
    return ctx->nav[ctx->depth].start;
}

/* Breaks at the last fitting space, else hard-breaks at TEXT_W. */
static unsigned wrap_len(const char *s, unsigned start, unsigned len)
{
    unsigned remain = len - start;
    unsigned w = remain < TEXT_W ? remain : TEXT_W;
    unsigned i;

    if (w == TEXT_W && remain > TEXT_W) {
        i = w;
        while (i > 0 && s[start + i - 1] != ' ')
            i--;
        if (i)
            w = i;
    }
    return w;
}

/* Advances past one wrapped line and the single space that broke it. */
static unsigned wrap_advance(const char *s, unsigned pos, unsigned len)
{
    pos += wrap_len(s, pos, len);
    while (pos < len && s[pos] == ' ')
        pos++;
    return pos;
}

/* Row count of one item: 1 except TEXT, which wraps to fit its text. */
unsigned atc_menu_item_rows(const atc_menu_item_t *it)
{
    unsigned len, pos = 0, lines = 0;

    if (it->kind != ATC_MENU_KIND_TEXT)
        return 1;

    len = it->label ? (unsigned)strlen(it->label) : 0;
    do {
        pos = wrap_advance(it->label, pos, len);
        lines++;
    } while (pos < len);
    return lines;
}

/* Item count starting at `start` that fits in `budget` rows; at least 1. */
static unsigned items_fit(const atc_menu_ctx_t *ctx, atc_menu_u8 start, unsigned budget)
{
    const atc_menu_item_t *it = cur_items(ctx);
    atc_menu_u8 total = ATC_MENU_COUNT(ctx);
    unsigned n = 0, h;

    while (start + n < total) {
        h = atc_menu_item_rows(&it[start + n]);
        if (n && h > budget)
            break;
        budget = h > budget ? 0 : budget - h;
        n++;
        if (!budget)
            break;
    }
    return n;
}

atc_menu_u8 atc_menu_page_items(const atc_menu_ctx_t *ctx, atc_menu_u8 start)
{
    return (atc_menu_u8)items_fit(ctx, start, ctx->items_per_page);
}

atc_menu_u8 atc_menu_page_before(const atc_menu_ctx_t *ctx, atc_menu_u8 start)
{
    atc_menu_u8 pos = 0, prev = 0;

    while (pos < start) {
        prev = pos;
        pos = (atc_menu_u8)(pos + atc_menu_page_items(ctx, pos));
    }
    return prev;
}

static unsigned hdr_lines(const atc_menu_ctx_t *ctx)
{
    unsigned n = 0;

    if (ctx->info) {
        if (ctx->info->name || ctx->info->version)
            n++;
        if (ctx->info->author)
            n++;
    }
    return n;
}

static unsigned vis_items(const atc_menu_ctx_t *ctx)
{
    return items_fit(ctx, cur_start(ctx), ctx->items_per_page);
}

/* Physical row count of the current vis_items(), not just their count. */
static unsigned vis_rows(const atc_menu_ctx_t *ctx)
{
    const atc_menu_item_t *it = cur_items(ctx);
    atc_menu_u8 s = cur_start(ctx);
    unsigned n = vis_items(ctx), rows = 0, i;

    for (i = 0; i < n; i++)
        rows += atc_menu_item_rows(&it[s + i]);
    return rows;
}

/* Maps a physical-row offset to its item and wrap line within that item. */
static atc_menu_u8 row_to_item(const atc_menu_ctx_t *ctx, unsigned row_off, unsigned *line)
{
    const atc_menu_item_t *it = cur_items(ctx);
    atc_menu_u8 idx = cur_start(ctx);
    unsigned h;

    for (;;) {
        h = atc_menu_item_rows(&it[idx]);
        if (row_off < h) {
            *line = row_off;
            return idx;
        }
        row_off -= h;
        idx++;
    }
}

/* 1-based selection number of the item on the visible page, 0 if not selectable. */
static unsigned sel_number(const atc_menu_ctx_t *ctx, atc_menu_u8 idx)
{
    const atc_menu_item_t *it = cur_items(ctx);
    unsigned n = 0;
    atc_menu_u8 i, s = cur_start(ctx), end = (atc_menu_u8)(s + vis_items(ctx));

    for (i = s; i < end; i++) {
        if (it[i].kind == ATC_MENU_KIND_LABEL ||
            it[i].kind == ATC_MENU_KIND_SEPARATOR ||
            it[i].kind == ATC_MENU_KIND_TEXT)
            continue;
        n++;
        if (i == idx)
            return n;
    }
    return 0;
}

static unsigned copy_trunc(char *dst, const char *s)
{
    unsigned n = 0;

    while (s && s[n] && n < VAL_MAX) {
        dst[n] = s[n];
        n++;
    }
    return n;
}

/* "0x" + bits/4 hex digits; shared by READOUT's hex formats and HEX. */
static unsigned hex_text(char *dst, uint32_t v, unsigned bits)
{
    dst[0] = '0';
    dst[1] = 'x';
    return 2 + atc_menu_fmt_hex(dst + 2, v, (bits + 3u) / 4u);
}

/* Rows with no format byte (NUMBER, FIXED) have flags 0, which reads signed. */
static int row_signed(const atc_menu_item_t *it)
{
    return (it->flags & ATC_MENU_UNSIGNED) == 0;
}

static unsigned dec_text(char *dst, int32_t v, const atc_menu_item_t *it)
{
    if (!row_signed(it))
        return atc_menu_fmt_u32(dst, (uint32_t)v);
    return atc_menu_fmt_i32(dst, v);
}

static unsigned fix_text(char *dst, int32_t v, unsigned decimals,
                         const atc_menu_item_t *it)
{
    return atc_menu_fmt_fix(dst, v, decimals, row_signed(it));
}

/* Text shown in the value column; dst must hold VAL_BUF chars. This is where
 * every getter is called, once per row build. */
static unsigned value_text(const atc_menu_item_t *it, char *dst)
{
    unsigned n = 0;

    switch (it->kind) {
    case ATC_MENU_KIND_READOUT: {
        const atc_menu_bind_t *b = (const atc_menu_bind_t *)it->detail;
        int32_t v = atc_menu_bind_get(b);
        /* ATC_MENU_UNSIGNED shares the byte, so mask it off before matching. */
        switch (it->flags & (atc_menu_u8)~ATC_MENU_UNSIGNED) {
        case ATC_MENU_HEX8:  n = hex_text(dst, (uint32_t)v, 8);      break;
        case ATC_MENU_HEX16: n = hex_text(dst, (uint32_t)v, 16);     break;
        case ATC_MENU_HEX32: n = hex_text(dst, (uint32_t)v, 32);     break;
        case ATC_MENU_FIX1:  n = fix_text(dst, v, 1, it);            break;
        case ATC_MENU_FIX2:  n = fix_text(dst, v, 2, it);            break;
        case ATC_MENU_FIX3:  n = fix_text(dst, v, 3, it);            break;
        default:             n = dec_text(dst, v, it);               break;
        }
        break;
    }
    case ATC_MENU_KIND_CHECKBOX: {
        const atc_menu_bind_t *b = (const atc_menu_bind_t *)it->detail;
        memcpy(dst, atc_menu_bind_get(b) ? "[X]" : "[ ]", 3);
        n = 3;
        break;
    }
    case ATC_MENU_KIND_NUMBER: {
        const atc_menu_num_t *d = (const atc_menu_num_t *)it->detail;
        int32_t v = atc_menu_bind_get(&d->bind);
        n = d->decimals ? fix_text(dst, v, d->decimals, it)
                        : dec_text(dst, v, it);
        break;
    }
    case ATC_MENU_KIND_HEX: {
        const atc_menu_hex_t *d = (const atc_menu_hex_t *)it->detail;
        n = hex_text(dst, (uint32_t)atc_menu_bind_get(&d->bind), d->bits);
        break;
    }
    case ATC_MENU_KIND_CHOICE: {
        const atc_menu_choice_t *d = (const atc_menu_choice_t *)it->detail;
        int32_t v = atc_menu_bind_get(&d->bind);
        if (v >= 0 && v < (int32_t)d->count) {
            n = copy_trunc(dst, d->items[v]);
        } else {
            dst[0] = '?';
            n = 1;
        }
        break;
    }
    case ATC_MENU_KIND_CUSTOM: {
        const atc_menu_custom_t *d = (const atc_menu_custom_t *)it->detail;
        /* No show() is a PROMPT row; an overrun is caught by the clamp below. */
        if (d->show) {
            n = d->show(d->arg, dst, VAL_MAX);
        } else {
            memcpy(dst, "...", 3);
            n = 3;
        }
        break;
    }
    case ATC_MENU_KIND_SUBMENU:
        dst[0] = '>';
        n = 1;
        break;
    default: /* ACTION, LABEL, SEPARATOR, TEXT: no value */
        break;
    }
    if (n > VAL_MAX)
        n = VAL_MAX;
    return n;
}

/* Plain ROW_W-column buffer for wrap line `line` (0-based) of a TEXT item. */
static void text_plain(const atc_menu_item_t *it, unsigned line, char *plain)
{
    const char *s = it->label ? it->label : "";
    unsigned len = (unsigned)strlen(s);
    unsigned pos = 0, n, l;

    for (l = 0; l < line; l++)
        pos = wrap_advance(s, pos, len);
    n = wrap_len(s, pos, len);
    memset(plain, ' ', ROW_W);
    memcpy(plain + COL_LABEL, s + pos, n);
}

/* One physical row: colors + fixed ROW_W columns, no positioning, no EOL.
 * SGR 0 is never emitted mid-row so zebra survives; `line` picks which
 * wrap line of a TEXT item to draw, all sharing one shade via `idx`. */
static void row_content(const atc_menu_ctx_t *ctx, atc_menu_u8 idx, unsigned line, sb_t *b)
{
    const atc_menu_item_t *it = &cur_items(ctx)[idx];
    unsigned vi = (unsigned)(idx - cur_start(ctx));
    int zebra = (vi & 1u) == 0;
    char plain[ROW_W];

    if (it->kind == ATC_MENU_KIND_TEXT) {
        text_plain(it, line, plain);
        sb_str(b, S_RESET);
        if (zebra)
            sb_str(b, S_ZEBRA);
        sb_str(b, S_LBL);
        sb_mem(b, plain, ROW_W);
        sb_str(b, S_RESET);
        return;
    }

    {
        char vb[VAL_BUF];
        unsigned vlen = value_text(it, vb);
        unsigned vstart = COL_VAL_END - vlen;
        unsigned num, llen = 0, lmax;
        const char *label = it->label ? it->label : "";

        memset(plain, ' ', ROW_W);
        if (it->kind == ATC_MENU_KIND_SEPARATOR) {
            memset(plain + 3, '-', COL_VAL_END - 3);
        } else {
            num = sel_number(ctx, idx);
            if (num) {
                plain[2] = num >= 10 ? (char)('0' + num / 10) : ' ';
                plain[3] = (char)('0' + num % 10);
            }
            lmax = vstart - COL_LABEL - (vlen ? 1 : 0);
            while (label[llen] && llen < lmax) {
                plain[COL_LABEL + llen] = label[llen];
                llen++;
            }
            if (vlen)
                memcpy(plain + vstart, vb, vlen);
        }

        sb_str(b, S_RESET);
        if (zebra)
            sb_str(b, S_ZEBRA);
        if (it->kind == ATC_MENU_KIND_SEPARATOR ||
            it->kind == ATC_MENU_KIND_LABEL) {
            sb_str(b, S_FRAME);
            sb_mem(b, plain, ROW_W);
        } else {
            sb_mem(b, plain, 2);
            sb_str(b, S_NUM);
            sb_mem(b, plain + 2, 2);
            sb_str(b, S_LBL);
            if (vlen) {
                sb_mem(b, plain + 4, vstart - 4);
                sb_str(b, S_VAL);
                sb_mem(b, plain + vstart, ROW_W - vstart);
            } else {
                sb_mem(b, plain + 4, ROW_W - 4);
            }
        }
        sb_str(b, S_RESET);
    }
}

static void name_line(const atc_menu_ctx_t *ctx, sb_t *b)
{
    const char *nm = ctx->info->name ? ctx->info->name : "";
    const char *ver = ctx->info->version;
    unsigned vlen = ver ? (unsigned)strlen(ver) : 0;
    unsigned nmax, nlen = 0;

    if (vlen > 12)
        vlen = 12;
    nmax = ROW_W - (vlen ? vlen + 1 : 0);

    sb_str(b, S_TITLE);
    while (nm[nlen] && nlen < nmax)
        nlen++;
    sb_mem(b, nm, nlen);
    if (vlen) {
        sb_fill(b, ' ', ROW_W - vlen - nlen);
        sb_str(b, S_VER);
        sb_mem(b, ver, vlen);
    }
    sb_str(b, S_RESET EOL);
}

/* Total page count, found by walking windows from the top. */
static unsigned page_count_total(const atc_menu_ctx_t *ctx)
{
    atc_menu_u8 total = ATC_MENU_COUNT(ctx), pos = 0;
    unsigned pages = 0;

    if (!total)
        return 1;
    do {
        pos = (atc_menu_u8)(pos + atc_menu_page_items(ctx, pos));
        pages++;
    } while (pos < total);
    return pages;
}

/* 1-based number of the page currently shown. */
static unsigned page_number(const atc_menu_ctx_t *ctx)
{
    atc_menu_u8 s = cur_start(ctx), pos = 0;
    unsigned page = 1;

    while (pos < s) {
        pos = (atc_menu_u8)(pos + atc_menu_page_items(ctx, pos));
        page++;
    }
    return page;
}

/* " (cur/total)" built into a fixed scratch buffer - not through sb_t, whose
 * bound check is sized for the real line buffer, not this 16-byte one. */
static unsigned page_suffix(const atc_menu_ctx_t *ctx, char *pg)
{
    unsigned n = 0, total = page_count_total(ctx);

    if (total <= 1)
        return 0;

    pg[n++] = ' ';
    pg[n++] = '(';
    n += atc_menu_fmt_u32(pg + n, page_number(ctx));
    pg[n++] = '/';
    n += atc_menu_fmt_u32(pg + n, total);
    pg[n++] = ')';
    return n;
}

static void crumb_line(const atc_menu_ctx_t *ctx, sb_t *b)
{
    unsigned d, total = 0;
    const char *t;
    char pg[16];
    unsigned pglen = page_suffix(ctx, pg);

    sb_str(b, S_TITLE);
    for (d = 0; d <= ctx->depth; d++) {
        t = ctx->nav[d].page->title;
        total += (t ? (unsigned)strlen(t) : 0) + (d ? 3 : 0);
    }
    if (total + pglen <= ROW_W) {
        for (d = 0; d <= ctx->depth; d++) {
            if (d)
                sb_str(b, " / ");
            t = ctx->nav[d].page->title;
            if (t)
                sb_str(b, t);
        }
        sb_mem(b, pg, pglen);
    } else {
        t = ctx->nav[ctx->depth].page->title;
        sb_str(b, "... / ");
        sb_mem(b, t ? t : "", t && strlen(t) > 34 ? 34 : (unsigned)(t ? strlen(t) : 0));
    }
    sb_str(b, S_RESET EOL);
}

/* Each hint is dropped whole, not truncated, once the rest no longer fit in
 * ROW_W visible columns - so the footer never spills past the box above it.
 * Priority is oldest/most-essential first, so a narrow ROW_W keeps
 * navigation over the newer Items/page hint. */
static void footer_line(const atc_menu_ctx_t *ctx, sb_t *b)
{
    const struct { const char *ansi; unsigned vis; int show; } seg[] = {
        { S_NUM " 0" S_DIM " Back",        7, 1 },
        { S_NUM "  r" S_DIM " Refresh",   11, 1 },
        { S_NUM "  ?" S_DIM " Help",       8, 1 },
        { S_NUM "  n/p" S_DIM " Page",    10, page_count_total(ctx) > 1 },
        { S_NUM "  i" S_DIM " Items/page", 14, 1 },
    };
    unsigned vis = 0, i;

    for (i = 0; i < sizeof seg / sizeof seg[0]; i++) {
        if (!seg[i].show || vis + seg[i].vis > ROW_W)
            continue;
        sb_str(b, seg[i].ansi);
        vis += seg[i].vis;
    }
    sb_str(b, S_RESET EOL);
}

/* Full-draw steps: 0 clear, 1 name, 2 author, 3 '=', 4 breadcrumb,
 * 5..4+nrows item rows, 5+nrows '-', 6+nrows footer (last). Steps that
 * build nothing (missing header fields) are skipped. */
static unsigned build_full(atc_menu_ctx_t *ctx, unsigned step, unsigned *last)
{
    sb_t b = { ctx->line, 0 };
    unsigned nrows = vis_rows(ctx);

    *last = 6 + nrows;
    if (step == 0) {
        sb_str(&b, "\x1b[2J\x1b[H");
    } else if (step == 1) {
        if (ctx->info && (ctx->info->name || ctx->info->version))
            name_line(ctx, &b);
    } else if (step == 2) {
        if (ctx->info && ctx->info->author) {
            sb_str(&b, S_AUTH);
            sb_str(&b, ctx->info->author);
            sb_str(&b, S_RESET EOL);
        }
    } else if (step == 3) {
        sb_str(&b, S_FRAME);
        sb_fill(&b, '=', ROW_W);
        sb_str(&b, S_RESET EOL);
    } else if (step == 4) {
        crumb_line(ctx, &b);
    } else if (step <= 4 + nrows) {
        unsigned line;
        atc_menu_u8 idx = row_to_item(ctx, step - 5, &line);

        row_content(ctx, idx, line, &b);
        sb_str(&b, EOL);
    } else if (step == 5 + nrows) {
        sb_str(&b, S_FRAME);
        sb_fill(&b, '-', ROW_W);
        sb_str(&b, S_RESET EOL);
    } else {
        footer_line(ctx, &b);
    }
    return b.len;
}

/* Edit prompt: `label [cur] (hint)> `. The hint is staged in a scratch buffer
 * and dropped whole if the line would overflow, so "> " always survives. */
static void edit_prompt(const atc_menu_ctx_t *ctx, sb_t *b)
{
    const atc_menu_item_t *it;
    const char *label;
    char vb[VAL_BUF];
    char hint[16];
    unsigned vlen, hlen = 0, llen = 0;

    /* Not tied to any item, so built from ctx directly. */
    if (ctx->mode == ATC_MENU_MODE_ITEMS_PER_PAGE) {
        vlen = atc_menu_fmt_u32(vb, ctx->items_per_page);
        hlen = atc_menu_fmt_u32(hint, ATC_MENU_ITEMS_PER_PAGE_MAX);

        sb_str(b, S_TITLE "Items/page" S_RESET " [" S_VAL);
        sb_mem(b, vb, vlen);
        sb_str(b, S_RESET "] " S_DIM "(1..");
        sb_mem(b, hint, hlen);
        sb_str(b, ")" S_RESET "> ");
        return;
    }

    it = &cur_items(ctx)[ctx->edit_item];
    label = it->label ? it->label : "";

    while (label[llen] && llen < 16)
        llen++;
    sb_str(b, S_TITLE);
    sb_mem(b, label, llen);
    sb_str(b, S_RESET);

    /* A CUSTOM row with no show() has nothing to put in the brackets. */
    if (it->kind != ATC_MENU_KIND_CUSTOM ||
        ((const atc_menu_custom_t *)it->detail)->show) {
        if (it->kind == ATC_MENU_KIND_CHOICE) {
            const atc_menu_choice_t *d = (const atc_menu_choice_t *)it->detail;
            vlen = copy_trunc(vb, d->items[ctx->choice_val]);
        } else {
            vlen = value_text(it, vb);
        }
        sb_str(b, " [" S_VAL);
        sb_mem(b, vb, vlen);
        sb_str(b, S_RESET "]");

        /* HEX is the only kind with a range of its own to hint at. */
        if (it->kind == ATC_MENU_KIND_HEX) {
            const atc_menu_hex_t *d = (const atc_menu_hex_t *)it->detail;
            memcpy(hint, "(hex, ", 6);
            hlen = 6;
            hlen += atc_menu_fmt_u32(hint + hlen, d->bits);
            memcpy(hint + hlen, " bit)", 5);
            hlen += 5;
        }
        if (hlen &&
            b->len + hlen + sizeof(S_DIM S_RESET) + 4 + ctx->input_len
                <= ATC_MENU_LINE_BUF) {
            sb_str(b, " " S_DIM);
            sb_mem(b, hint, hlen);
            sb_str(b, S_RESET);
        }
    }
    sb_str(b, "> ");
}

/* Prompt block steps: 0 = position + clear-below + message line,
 * 1 = select line, 2 = edit line (edit or choice mode only, last). */
static unsigned build_prompt(atc_menu_ctx_t *ctx, unsigned step, unsigned *last)
{
    sb_t b = { ctx->line, 0 };
    unsigned msg_row = hdr_lines(ctx) + 5 + vis_rows(ctx);
    int editing = ctx->mode == ATC_MENU_MODE_EDIT ||
                  ctx->mode == ATC_MENU_MODE_CHOICE ||
                  ctx->mode == ATC_MENU_MODE_ITEMS_PER_PAGE;

    *last = editing ? 2 : 1;
    if (step == 0) {
        sb_pos(&b, msg_row);
        sb_str(&b, S_RESET "\x1b[J");
        if (ctx->msg) {
            sb_str(&b, ctx->msg_err ? S_ERR : S_DIM);
            sb_str(&b, ctx->msg);
            sb_str(&b, S_RESET);
        }
    } else if (step == 1) {
        sb_pos(&b, msg_row + 1);
        sb_str(&b, S_TITLE "Select> " S_RESET);
        /* No edit_num behind a bare command key. */
        if (editing && ctx->mode != ATC_MENU_MODE_ITEMS_PER_PAGE)
            sb_u32(&b, ctx->edit_num);
        else if (!editing)
            sb_mem(&b, ctx->input, ctx->input_len);
    } else {
        sb_pos(&b, msg_row + 2);
        edit_prompt(ctx, &b);
        sb_mem(&b, ctx->input, ctx->input_len);
    }
    return b.len;
}

/* Earlier items may be multi-row TEXT, so sum heights instead of subtracting indices. */
static unsigned build_row_update(atc_menu_ctx_t *ctx)
{
    sb_t b = { ctx->line, 0 };
    const atc_menu_item_t *it = cur_items(ctx);
    atc_menu_u8 s = cur_start(ctx), i;
    unsigned row_off = 0;

    for (i = s; i < ctx->dirty_row; i++)
        row_off += atc_menu_item_rows(&it[i]);

    sb_pos(&b, hdr_lines(ctx) + 3 + row_off);
    row_content(ctx, ctx->dirty_row, 0, &b);
    sb_str(&b, "\x1b[K");
    return b.len;
}

void atc_menu_request_full(atc_menu_ctx_t *ctx)
{
    ctx->draw_pos = 0;
    ctx->dirty_row = 0xFF;
    ctx->prompt_pos = 0xFF;
}

void atc_menu_render_flush(atc_menu_ctx_t *ctx)
{
    unsigned last, len;

    while (ctx->draw_pos != 0xFF) {
        len = build_full(ctx, ctx->draw_pos, &last);
        if (len && !ctx->sink(ctx->user, ctx->line, len))
            return;
        if (ctx->draw_pos == last) {
            ctx->draw_pos = 0xFF;
            ctx->dirty_row = 0xFF;
            ctx->prompt_pos = 0;
        } else {
            ctx->draw_pos++;
        }
    }

    if (ctx->dirty_row != 0xFF) {
        atc_menu_u8 s = cur_start(ctx);

        if (ctx->dirty_row >= s && ctx->dirty_row < s + vis_items(ctx)) {
            len = build_row_update(ctx);
            if (len && !ctx->sink(ctx->user, ctx->line, len))
                return;
        }
        ctx->dirty_row = 0xFF;
        ctx->prompt_pos = 0;
    }

    while (ctx->prompt_pos != 0xFF) {
        len = build_prompt(ctx, ctx->prompt_pos, &last);
        if (len && !ctx->sink(ctx->user, ctx->line, len))
            return;
        if (ctx->prompt_pos == last)
            ctx->prompt_pos = 0xFF;
        else
            ctx->prompt_pos++;
    }
}
