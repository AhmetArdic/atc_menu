/* SPDX-License-Identifier: MIT */
/**
 * @file menu_draw.c
 * @brief Where a row goes and what it looks like
 *
 * The only file that names a screen row or an SGR sequence. Callers address an
 * item by the index it was declared with; the banner's height and the page
 * offset turn that into a row here, so nothing above this layer has to know how
 * tall the chrome came out this frame.
 *
 * A row is signed by what it is drawn from rather than by the bytes it would
 * come out as, and built only if that moved — so an idle frame lays out
 * nothing, an unchanged value is never formatted, and a row goes out only when
 * it changed, which is what keeps a 9600-baud link quiet between keystrokes.
 */
#include "menu_internal.h"

#include <string.h>

#define SGR_RESET "\x1b[0m"
#define SGR_ZEBRA "\x1b[48;5;236m"
#define SGR_NUM   "\x1b[1;33m"
#define SGR_TEXT  "\x1b[22;39m"
#define SGR_HEAD  "\x1b[36m"
#define SGR_VAL   "\x1b[1;32m"
#define SGR_DIM   "\x1b[2m"
#define SGR_NAME  "\x1b[1;37m"
#define SGR_VER   "\x1b[1;36m"
#define SGR_OWNER "\x1b[37m"
#define SGR_HINT  "\x1b[90m"

/*---------------------------------------------------------------------------
 * The line cache
 *-------------------------------------------------------------------------*/

/* Fletcher-16, no multiply: an MSP430 without a hardware multiplier would pay
   a software routine per byte. The two sums pack back into the 16 bits this
   returns, so one signature can be carried across several pieces. */
static uint16_t sig_more(uint16_t sig, const char *s, size_t n)
{
    uint16_t a = (uint16_t)(sig & 0xFFu), b = (uint16_t)(sig >> 8);
    size_t   i;

    for (i = 0u; i < n; ++i) {
        a = (uint16_t)(a + (unsigned char)s[i]);
        a = (uint16_t)((a & 0xFFu) + (a >> 8));
        b = (uint16_t)(b + a);
        b = (uint16_t)((b & 0xFFu) + (b >> 8));
    }
    return (uint16_t)((b << 8) | a);
}

static uint16_t sig_end(uint16_t sig)
{
    return (sig == 0u) ? 1u : sig; /* 0 means "never painted" */
}

static uint16_t sig_row(const char *s, size_t n)
{
    return sig_end(sig_more(0xFFFFu, s, n));
}

/* Only for pointers the library compares rather than reads: a caller's string
   is signed by its bytes, since labels get built into scratch buffers. */
static uint16_t sig_ptr(uint16_t sig, const void *p)
{
    const void *q = p;

    return sig_more(sig, (const char *)&q, sizeof q);
}

/* Nothing past the column a string is clipped to can change the row. */
static size_t clip_len(const char *s, size_t max)
{
    size_t n = 0u;

    while (n < max && s[n] != '\0')
        n++;
    return n;
}

static uint16_t sig_text_more(uint16_t sig, const char *s, size_t max)
{
    return (s == NULL) ? sig_more(sig, "\x00", 1u)
                       : sig_more(sig, s, clip_len(s, max));
}

uint16_t sig_text_bytes(const char *s, size_t n)
{
    return sig_end(sig_more(0xFFFFu, s, n));
}

uint16_t sig_text(const char *s, size_t max)
{
    return (s == NULL) ? 0u : sig_end(sig_more(0xFFFFu, s, clip_len(s, max)));
}

static size_t label_width(const atc_menu_ctx_t *c)
{
    return (size_t)c->cols - NUMW - VALW;
}

/* An item row is a function of what the frame declared for it, so signing the
   inputs recognises an unchanged row without building it. */
uint16_t item_key(const atc_menu_ctx_t *c, unsigned item_i, unsigned number,
                  const char *label, uint16_t vkey, bool dim, unsigned style)
{
    char     h[7];
    uint16_t k;

    if (dim)
        style = 0u; /* the row is drawn that way, so it keys that way */

    h[0] = (char)number;
    h[1] = (char)(dim ? 1u : 0u);
    h[2] = (char)style;
    h[3] = (char)(item_i & 1u);        /* the stripe */
    h[4] = (char)(vkey & 0xFFu);
    h[5] = (char)(vkey >> 8);
    h[6] = '\x1f';
    k = sig_more(0xFFFFu, h, sizeof h);
    if (label != NULL)
        k = sig_more(k, label, clip_len(label, label_width(c)));
    return sig_end(k);
}

/* Everything the chrome is drawn from. An open editor is the exception: its
   prompt moves with every keystroke, so frame_end never caches it. */
uint16_t chrome_key(const atc_menu_ctx_t *c)
{
    char     h[7];
    uint16_t k;
    unsigned d;

    h[0] = (char)c->level_items;
    h[1] = (char)c->page_items;
    h[2] = (char)c->top[c->nav_depth];
    h[3] = (char)c->nav_depth;
    h[4] = (char)c->shown_items;
    h[5] = (char)c->pending;
    h[6] = (char)c->rows;
    k = sig_more(0xFFFFu, h, sizeof h);
    k = sig_ptr(k, c->info);
    if (c->info != NULL) {
        k = sig_text_more(k, c->info->name, c->cols);
        k = sig_text_more(k, c->info->version, c->cols);
        k = sig_text_more(k, c->info->owner, c->cols);
    }
    for (d = 0u; d < c->nav_depth; ++d)
        k = sig_text_more(k, c->crumb[d], c->cols);
    k = sig_text_more(k, c->msg, c->cols);
    return sig_end(k);
}

static bool save_cursor(atc_menu_ctx_t *c)
{
    if ((c->flags & F_SAVED) != 0u)
        return true;
    if (c->sink("\x1b" "7", 2u, c->user) != 1) {
        c->status = (signed char)ATC_MENU_ERR_IO;
        c->flags |= F_STOP;
        return false;
    }
    c->flags |= F_SAVED;
    return true;
}

/* Pairs with the save the frame's first painted row did: the cursor goes back
   where the application left it, so the menu can share the terminal with
   whatever else is printing. */
void restore_cursor(atc_menu_ctx_t *c)
{
    if ((c->flags & F_SAVED) == 0u)
        return;
    c->flags &= (uint16_t)~F_SAVED;
    if (c->sink("\x1b" "8", 2u, c->user) != 1)
        c->status = (signed char)ATC_MENU_ERR_IO;
}

static void line_begin(atc_menu_ctx_t *c, buf_t *b, unsigned row,
                       const char *sgr)
{
    b->p = c->buf;
    b->cap = row_cap(c);
    b->len = 0u;

    bstr(b, "\x1b[");
    bu32(b, row);
    bstr(b, ";1H");
    /* The address is this row's own and never varies, so the signature starts
       after it. The stripe that follows does vary — a page turn can flip it. */
    b->sig = b->len;
    bstr(b, SGR_RESET);
    bstr(b, sgr);
    b->body = b->len;
    b->vis = 0u;
}

/* pad_to > 0 fills out to that column before closing; a striped row needs it so
   the background stops at the menu edge rather than the terminal's. A caller
   that signed the row's inputs passes that key instead of 0. */
static void line_end(atc_menu_ctx_t *c, buf_t *b, unsigned row, bool styled,
                     size_t pad_to, uint16_t key)
{
    uint16_t sig;

    if (pad_to > 0u) {
        while (b->vis < pad_to)
            bput(b, ' ');
    } else {
        while (b->len > b->body && b->len <= b->cap && b->p[b->len - 1u] == ' ') {
            b->len--;
            b->vis--;
        }
    }

    if (styled)
        bstr(b, SGR_RESET);
    bstr(b, "\x1b[K");

    if (b->len > b->cap) {
        c->status = (signed char)ATC_MENU_ERR_PARAM;
        return;
    }

    sig = (key != 0u) ? key : sig_row(b->p + b->sig, b->len - b->sig);
    if (c->row_sig[row - 1u] == sig)
        return;
    if (!save_cursor(c))
        return;
    if (c->sink(b->p, b->len, c->user) != 1) {
        c->status = (signed char)ATC_MENU_ERR_IO;
        c->flags |= F_STOP;
        return;
    }
    c->row_sig[row - 1u] = sig;
}

/*---------------------------------------------------------------------------
 * Geometry
 *-------------------------------------------------------------------------*/

/* A banner line with nothing in it is not painted, so it is not charged. */
static unsigned head_rows(const atc_menu_ctx_t *c)
{
    unsigned n = 0u;

    if (c->info == NULL)
        return 0u;
    if (c->info->name != NULL || c->info->version != NULL)
        n++;
    if (c->info->owner != NULL)
        n++;
    return n;
}

static unsigned first_item_row(const atc_menu_ctx_t *c)
{
    return head_rows(c) + 3u; /* the banner, then the rule and the breadcrumb */
}

/* Where a visible item lands: the first item row, offset by how far the item is
   past the top of the page. */
static unsigned item_row(const atc_menu_ctx_t *c, unsigned item_i)
{
    return first_item_row(c) + (item_i - c->top[c->nav_depth]);
}

/* Two rows above the items — the rule and the breadcrumb — and four below. */
unsigned page_items_max(const atc_menu_ctx_t *c)
{
    return (unsigned)c->rows - head_rows(c) - 6u;
}

/* The closing rule goes straight after the last item row, not at the bottom of
   the window, so a level three rows long is three rows tall. */
static unsigned rule_row(const atc_menu_ctx_t *c)
{
    return first_item_row(c) + c->shown_items;
}

static unsigned footer_row(const atc_menu_ctx_t *c) { return rule_row(c) + 1u; }
static unsigned msg_row(const atc_menu_ctx_t *c)    { return rule_row(c) + 2u; }
static unsigned prompt_row(const atc_menu_ctx_t *c) { return rule_row(c) + 3u; }

static bool row_ok(const atc_menu_ctx_t *c, unsigned row)
{
    return (c->flags & F_STOP) == 0u && row >= 1u && row <= c->rows;
}

/*---------------------------------------------------------------------------
 * Item rows
 *-------------------------------------------------------------------------*/

/* A style that names nothing emits nothing: a bare ESC[m means ESC[0m, which
   would wipe the colour the row is already wearing. */
static void bstyle(buf_t *b, unsigned style)
{
    static const char CODE[4] = { '1', '3', '4', '7' }; /* bold italic ul rev */
    unsigned          fg = (style >> 4) & 0x0Fu;
    unsigned          i;
    size_t            vis = b->vis;
    bool              first = true;

    if ((style & 0x0Fu) == 0u && (fg < 1u || fg > 8u))
        return;

    bstr(b, "\x1b[");
    for (i = 0u; i < 4u; ++i) {
        if ((style & (1u << i)) == 0u)
            continue;
        if (!first)
            bput(b, ';');
        bput(b, CODE[i]);
        first = false;
    }
    if (fg >= 1u && fg <= 8u) {
        if (!first)
            bput(b, ';');
        bput(b, '3');
        bput(b, (char)('0' + (int)(fg - 1u)));
    }
    bput(b, 'm');
    b->vis = vis;
}

/* The green the value column has always worn, or the application's colour in
   its place. */
static void bvalue_sgr(buf_t *b, unsigned style)
{
    unsigned fg = (style >> 4) & 0x0Fu;
    size_t   vis;

    if (fg < 1u || fg > 8u) {
        bsgr(b, SGR_VAL);
        return;
    }
    vis = b->vis;
    bstr(b, "\x1b[");
    bstr(b, ((style & ATC_MENU_BOLD) != 0u) ? "1" : "22");
    bput(b, ';');
    bput(b, '3');
    bput(b, (char)('0' + (int)(fg - 1u)));
    bput(b, 'm');
    b->vis = vis;
}

/* Asked before a value is formatted, which is the point of asking. */
bool row_needs(const atc_menu_ctx_t *c, unsigned item_i, uint16_t key)
{
    unsigned row = item_row(c, item_i);

    return row_ok(c, row) && c->row_sig[row - 1u] != key;
}

void row_item(atc_menu_ctx_t *c, unsigned item_i, unsigned number,
              const char *label, const char *value, bool dim, unsigned style,
              uint16_t key)
{
    buf_t    b;
    unsigned row = item_row(c, item_i);
    size_t   lw = label_width(c);
    size_t   used = 0u;
    size_t   vlen = (value != NULL) ? clip_len(value, VALW) : 0u;
    bool     striped;

    if (!row_ok(c, row))
        return;

    if (dim)
        style = 0u; /* "not available now" outranks decoration */

    striped = (item_i & 1u) == 0u;
    line_begin(c, &b, row, striped ? SGR_ZEBRA : "");
    if (dim)
        bsgr(&b, SGR_DIM);
    else if (number > 0u)
        bsgr(&b, SGR_NUM);
    else
        bsgr(&b, SGR_HEAD);

    if (number > 0u) {
        char     nb[8];
        buf_t    n;
        unsigned i;
        n.p = nb; n.cap = sizeof nb; n.len = 0u; n.body = 0u; n.sig = 0u;
        n.vis = 0u;
        bu32(&n, number);
        if (n.len > 4u)
            n.len = 4u;
        bpad(&b, 4u - n.len);
        for (i = 0u; i < (unsigned)n.len; ++i)
            bput(&b, nb[i]);
        bpad(&b, 3u);
        if (!dim)
            bsgr(&b, SGR_TEXT);
    } else {
        bpad(&b, NUMW);
    }

    bstyle(&b, style);

    if (label != NULL) {
        while (label[used] != '\0' && used < lw) {
            bput(&b, label[used]);
            used++;
        }
    }

    if (vlen > 0u) {
        size_t i;
        bpad(&b, lw - used);
        if (vlen > VALW)
            vlen = VALW;
        bpad(&b, VALW - vlen);
        /* The value column carries the colour, whatever the widget: it is the
           part of the row that changes, and the eye should land on it. A dim
           row keeps its one attribute so "unavailable" outranks it. */
        if (!dim)
            bvalue_sgr(&b, style);
        for (i = 0u; i < vlen; ++i)
            bput(&b, value[i]);
    }

    line_end(c, &b, row, true, striped ? (size_t)c->cols : 0u, key);
}

/* A rule inside the item area. It takes its stripe from the same alternation
   the rows use, so it sits in the sequence instead of interrupting it. */
void row_separator(atc_menu_ctx_t *c, unsigned item_i)
{
    unsigned row = item_row(c, item_i);
    bool     striped = (item_i & 1u) == 0u;
    char     h[2];
    uint16_t key;
    buf_t    b;

    if (!row_ok(c, row))
        return;

    h[0] = (char)(item_i & 1u);
    h[1] = '-'; /* a rule, so it cannot key like an item row */
    key = sig_row(h, sizeof h);
    if (c->row_sig[row - 1u] == key)
        return;

    line_begin(c, &b, row, striped ? SGR_ZEBRA : "");
    bsgr(&b, SGR_HEAD);
    bpad(&b, 3u);
    while (b.vis < (size_t)c->cols - 2u)
        bput(&b, '-');
    line_end(c, &b, row, true, striped ? (size_t)c->cols : 0u, key);
}

/*---------------------------------------------------------------------------
 * The chrome around them
 *-------------------------------------------------------------------------*/

/* "Freq (Hz) [1000]> 50" — which row, what it held, what is being typed over
   it. A choice has no keystrokes: its candidate is the bracket, rewritten by
   the widget as the user steps through. The bracket wears the value colour, so
   it reads as the column it came from rather than as part of the label. */
static void prompt_editor(atc_menu_ctx_t *c, buf_t *b)
{
    const char *t = edit_area_c(c);
    /* the closing bracket, which the NUL sits two past */
    size_t vend = (c->edit_head >= 2u) ? (size_t)c->edit_head - 2u : 0u;
    size_t k = 0u;

    while (t[k] != '\0' && b->vis < (size_t)c->cols) {
        if (c->edit_vpos != 0u) {
            if (k == (size_t)c->edit_vpos)
                bsgr(b, SGR_VAL);
            else if (k == vend)
                bsgr(b, SGR_NAME);
        }
        bput(b, t[k]);
        k++;
    }
    bstr(b, "> ");

    if ((c->flags & F_CHOICE) != 0u) {
        /* nothing to echo */
    } else if ((c->flags & F_TEXT) != 0u) {
        bclip(b, edit_text_c(c), (size_t)c->cols);
    } else {
        if ((c->flags & F_NEG) != 0u)
            bput(b, '-');
        if (c->edit_base == 16u && c->edit_len > 0u) {
            /* Digits typed, but never more than a uint32_t has: leading zeros
               can push edit_len past eight. */
            bstr(b, "0x");
            bhexdigits(b, c->acc, (c->edit_len > 8u) ? 8u : c->edit_len);
        } else if (c->edit_frac != NO_FRAC) {
            bnum(b, c->acc, false, c->edit_frac);
            if (c->edit_frac == 0u)
                bput(b, '.'); /* typed, but no fraction digits yet */
        } else if (c->edit_len > 0u) {
            bu32(b, c->acc);
        }
    }
}

/* Only the keys that would do something here: at the root there is nowhere to
   go back to, and on a single page there is no other page. A legend advertising
   a key that answers nothing is worse than a shorter legend. Each hint also
   costs two escape sequences on top of its columns, so a narrow line runs out
   before the list does; one that does not fit is dropped whole, and the order
   is most-essential first. */
static void paint_footer(atc_menu_ctx_t *c, unsigned pages)
{
    static const char *const KEYS[4] = { " 0", "  r", "  n/p", "  i" };
    static const char *const TXT[4] = { " Back", " Refresh", " Page",
                                        " Items" };
    /* what line_end still has to append: SGR_RESET + ESC[K + NUL */
    const size_t tail = sizeof SGR_RESET + 3u;
    unsigned     k;
    buf_t        b;

    line_begin(c, &b, footer_row(c), "");
    for (k = 0u; k < 4u; ++k) {
        size_t vis = strlen(KEYS[k]) + strlen(TXT[k]);
        size_t bytes = vis + (sizeof SGR_NUM - 1u) + (sizeof SGR_HINT - 1u);

        if (k == 0u && c->nav_depth == 0u)
            continue; /* already at the root */
        if (k == 2u && pages <= 1u)
            continue; /* the level fits one page */
        if (b.vis + vis > (size_t)c->cols || b.len + bytes + tail > b.cap)
            continue;
        bsgr(&b, SGR_NUM);
        bstr(&b, KEYS[k]);
        bsgr(&b, SGR_HINT);
        bstr(&b, TXT[k]);
    }
    line_end(c, &b, footer_row(c), true, 0u, 0u);
}

void paint_chrome(atc_menu_ctx_t *c)
{
    buf_t    b;
    unsigned pages;
    unsigned page;
    unsigned row = 1u;

    pages = (c->level_items == 0u)
                ? 1u
                : ((unsigned)c->level_items + c->page_items - 1u) / c->page_items;
    page = (unsigned)c->top[c->nav_depth] / c->page_items + 1u;

    if (c->info != NULL && (c->info->name != NULL || c->info->version != NULL)) {
        if (row_ok(c, row)) {
            line_begin(c, &b, row, SGR_NAME);
            if (c->info->name != NULL)
                bclip(&b, c->info->name, (size_t)c->cols);
            if (c->info->version != NULL) {
                size_t vlen = strlen(c->info->version);
                /* vlen sits on the left of the comparison: on the right it
                   would wrap for a version longer than the line and turn the
                   padding into an endless loop. */
                if (vlen < (size_t)c->cols) {
                    while (b.vis + vlen < (size_t)c->cols)
                        bput(&b, ' ');
                }
                bsgr(&b, SGR_VER);
                bclip(&b, c->info->version, (size_t)c->cols);
            }
            line_end(c, &b, row, true, 0u, 0u);
        }
        row++;
    }

    if (c->info != NULL && c->info->owner != NULL) {
        if (row_ok(c, row)) {
            line_begin(c, &b, row, SGR_OWNER);
            bclip(&b, c->info->owner, (size_t)c->cols);
            line_end(c, &b, row, true, 0u, 0u);
        }
        row++;
    }

    if (row_ok(c, row)) {
        line_begin(c, &b, row, SGR_HEAD);
        while (b.vis < (size_t)c->cols)
            bput(&b, '=');
        line_end(c, &b, row, true, 0u, 0u);
    }
    row++;

    if (row_ok(c, row)) {
        unsigned d;
        line_begin(c, &b, row, SGR_NAME);
        bclip(&b, "Main", (size_t)c->cols);
        for (d = 0u; d < c->nav_depth; ++d) {
            bclip(&b, " / ", (size_t)c->cols);
            bclip(&b, (c->crumb[d] != NULL) ? c->crumb[d] : "?",
                  (size_t)c->cols);
        }
        /* " (254/254)" is the widest counter; it goes whole or not at all. */
        if (pages > 1u && b.vis + 10u <= (size_t)c->cols) {
            bput(&b, ' ');
            bput(&b, '(');
            bu32(&b, page);
            bput(&b, '/');
            bu32(&b, pages);
            bput(&b, ')');
        }
        line_end(c, &b, row, true, 0u, 0u);
    }

    if (row_ok(c, rule_row(c))) {
        line_begin(c, &b, rule_row(c), SGR_HEAD);
        while (b.vis < (size_t)c->cols)
            bput(&b, '-');
        line_end(c, &b, rule_row(c), true, 0u, 0u);
    }

    if (row_ok(c, footer_row(c)))
        paint_footer(c, pages);

    if (row_ok(c, msg_row(c))) {
        line_begin(c, &b, msg_row(c), SGR_HINT);
        if (c->msg != NULL)
            bclip(&b, c->msg, (size_t)c->cols);
        line_end(c, &b, msg_row(c), true, 0u, 0u);
    }

    if (row_ok(c, prompt_row(c))) {
        line_begin(c, &b, prompt_row(c), SGR_NAME);
        if ((c->flags & F_EDIT) != 0u && c->edit_item == EDIT_PAGE) {
            bstr(&b, "Items [");
            bu32(&b, c->page_items);
            bstr(&b, "] (1-");
            bu32(&b, page_items_max(c));
            bstr(&b, ")> ");
            if (c->edit_len > 0u)
                bu32(&b, c->acc);
        } else if ((c->flags & F_EDIT) != 0u) {
            prompt_editor(c, &b);
        } else {
            bstr(&b, "Select> ");
            if (c->pending != 0u)
                bu32(&b, c->pending);
        }
        bput(&b, '_');
        line_end(c, &b, prompt_row(c), true, 0u, 0u);
    }
}

/* Whatever the menu reached before it shrank is blanked. */
void blank_tail(atc_menu_ctx_t *c)
{
    unsigned r;

    for (r = prompt_row(c) + 1u; r <= (unsigned)c->rows; ++r) {
        buf_t b;
        if (!row_ok(c, r))
            break;
        line_begin(c, &b, r, "");
        line_end(c, &b, r, false, 0u, 0u);
    }
}
