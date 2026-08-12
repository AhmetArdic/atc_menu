/* SPDX-License-Identifier: MIT */
/**
 * @file menu_core.c
 * @brief The context's life: init, terminal, frame, teardown
 *
 * atc_menu_frame_end() is where everything deferred lands: the chrome, once
 * the level's height is known; the rows the level no longer fills; a selected
 * submenu; and the value a widget delivered, now refusable no longer.
 */
#include "menu_internal.h"

#include <string.h>

/*--- Binding a context to a screen ------------------------------------------*/

atc_menu_status_t atc_menu_init(atc_menu_ctx_t *c, const atc_menu_info_t *info,
                                const atc_menu_screen_t *screen,
                                atc_menu_sink_t sink, void *user)
{
    if (c == NULL || screen == NULL || sink == NULL)
        return ATC_MENU_ERR_PARAM;
    if (screen->buf == NULL || screen->row_sig == NULL)
        return ATC_MENU_ERR_PARAM;
    if (screen->cols < NUMW + VALW + 4u)
        return ATC_MENU_ERR_PARAM;
    if (screen->rows <= CHROME_ROWS)
        return ATC_MENU_ERR_PARAM;
    if (screen->buf_cap < ATC_MENU_BUF_BYTES(screen->cols))
        return ATC_MENU_ERR_PARAM;

    /* Copied, not pointed at, so the descriptor may be a temporary. */
    memset(c, 0, sizeof *c);
    c->info = info;
    c->sink = sink;
    c->user = user;
    c->buf = screen->buf;
    c->buf_cap = screen->buf_cap;
    c->row_sig = screen->row_sig;
    c->rows = screen->rows;
    c->cols = screen->cols;
    measure_head(c);
    /* until 'i' says otherwise */
    c->page_items = (unsigned char)page_items_max(c);
    memset(c->row_sig, 0, (size_t)c->rows * sizeof *c->row_sig);
    return ATC_MENU_OK;
}

void atc_menu_set_items_per_page(atc_menu_ctx_t *c, unsigned n)
{
    if (c == NULL || c->rows == 0u)
        return;
    if (n == 0u)
        n = 1u;
    if (n > page_items_max(c))
        n = page_items_max(c);
    c->page_items = (unsigned char)n;
    /* an old offset need not be a page boundary under the new size */
    memset(c->top, 0, sizeof c->top);
}

unsigned atc_menu_items_per_page(const atc_menu_ctx_t *c)
{
    return (c != NULL) ? c->page_items : 0u;
}

void atc_menu_refresh(atc_menu_ctx_t *c)
{
    if (c != NULL) {
        memset(c->row_sig, 0, (size_t)c->rows * sizeof *c->row_sig);
        c->chrome_sig = 0u;
    }
}

/* What is on screen was signed under the old rule, so it goes again. */
void atc_menu_static_labels(atc_menu_ctx_t *c, bool on)
{
    if (c == NULL)
        return;
    if (on)
        c->flags |= F_LABEL_PTR;
    else
        c->flags &= (uint16_t)~F_LABEL_PTR;
    atc_menu_refresh(c);
}

/* What is on screen was painted under the old rule, so it goes again. */
void atc_menu_fast_fill(atc_menu_ctx_t *c, bool on)
{
    if (c == NULL)
        return;
    if (on)
        c->flags |= F_FAST_FILL;
    else
        c->flags &= (uint16_t)~F_FAST_FILL;
    atc_menu_refresh(c);
}

/*--- The terminal around it -------------------------------------------------*/

/* All VT100 but ESC[?25l, which a real one ignores. No alternate screen, no
   scrolling region. */
atc_menu_status_t atc_menu_term_begin(atc_menu_ctx_t *c)
{
    static const char SEQ[] = "\x1b[?6l\x1b[2J\x1b[?25l\x1b[1;1H";

    if (c == NULL || c->sink == NULL)
        return ATC_MENU_ERR_PARAM;
    if (c->sink(SEQ, sizeof SEQ - 1u, c->user) != 1)
        return ATC_MENU_ERR_IO;
    atc_menu_refresh(c);
    return ATC_MENU_OK;
}

atc_menu_status_t atc_menu_term_end(atc_menu_ctx_t *c)
{
    buf_t b;

    if (c == NULL || c->sink == NULL)
        return ATC_MENU_ERR_PARAM;

    /* the row half only: the tail behind it belongs to an open editor */
    b.p = c->buf; b.cap = row_cap(c); b.len = 0u; b.body = 0u; b.sig = 0u;
    b.vis = 0u; b.solid = false;
    bstr(&b, "\x1b[0m\x1b[?25h\x1b[");
    bu32(&b, c->rows); /* the bottom of the window the menu was given */
    bstr(&b, ";1H");

    if (b.len > b.cap)
        return ATC_MENU_ERR_PARAM;
    return (c->sink(b.p, b.len, c->user) == 1) ? ATC_MENU_OK : ATC_MENU_ERR_IO;
}

/*--- State an application can set between frames ----------------------------*/

void atc_menu_reset(atc_menu_ctx_t *c)
{
    if (c == NULL)
        return;
    c->nav_depth = 0u;
    c->pending = 0u;
    c->act = 0u;
    c->enter_req = 0u;
    c->msg = NULL;
    memset(c->top, 0, sizeof c->top);
    memset(c->path, 0, sizeof c->path);
    end_edit(c);
}

void atc_menu_message(atc_menu_ctx_t *c, const char *msg)
{
    if (c != NULL)
        c->msg = msg;
}

bool atc_menu_editing(const atc_menu_ctx_t *c)
{
    return c != NULL && (c->flags & F_EDIT) != 0u;
}

void atc_menu_item_enable(atc_menu_ctx_t *c, bool enabled)
{
    if (c == NULL)
        return;
    if (enabled)
        c->flags &= (uint16_t)~F_DISABLE;
    else
        c->flags |= F_DISABLE;
}

void atc_menu_item_style(atc_menu_ctx_t *c, unsigned flags)
{
    if (c != NULL)
        c->item_style = (unsigned char)(flags & 0xFFu);
}

/*--- The frame --------------------------------------------------------------*/

atc_menu_status_t atc_menu_frame_begin(atc_menu_ctx_t *c)
{
    if (c == NULL || c->sink == NULL)
        return ATC_MENU_ERR_PARAM;

    measure_head(c);
    c->decl_depth = 0u;
    c->level_numbered = 0u;
    c->level_items = 0u;
    c->status = (signed char)ATC_MENU_OK;
    c->flags &= (uint16_t)~(F_STOP | F_DISABLE);
    c->item_style = 0u;
    memset(c->item, 0, sizeof c->item);
    return ATC_MENU_OK;
}

atc_menu_status_t atc_menu_frame_end(atc_menu_ctx_t *c)
{
    unsigned shown;
    unsigned top;

    if (c == NULL || c->sink == NULL)
        return ATC_MENU_ERR_PARAM;
    if (c->decl_depth != 0u) {
        c->decl_depth = 0u;
        c->status = (signed char)ATC_MENU_ERR_STATE;
    }

    /* The frame the application had to answer a delivered value ends here, and
       with it atc_menu_reject()'s chance to reopen the editor. */
    c->flags &= (uint16_t)~F_DELIVERED;

    /* the level may have shrunk since this page was chosen */
    top = c->top[c->nav_depth];
    if (top >= c->level_items) {
        top = (c->level_items == 0u)
                  ? 0u
                  : (((unsigned)c->level_items - 1u) / c->page_items) * c->page_items;
        c->top[c->nav_depth] = (unsigned char)top;
    }

    shown = (unsigned)c->level_items - top;
    if (shown > c->page_items)
        shown = c->page_items;
    c->shown_items = (unsigned char)shown; /* where the closing rule goes */

    /* one signature over all seven chrome rows */
    if ((c->flags & F_EDIT) != 0u) {
        c->chrome_sig = 0u; /* the prompt moves with every keystroke */
        paint_chrome(c);
        blank_tail(c);
    } else {
        uint16_t k = chrome_key(c);

        if (k != c->chrome_sig) {
            paint_chrome(c);
            blank_tail(c);
            if ((c->flags & F_STOP) == 0u)
                c->chrome_sig = k; /* a refused row stays dirty */
        }
    }

    /* The rows above were laid out with the old page size, so the new one takes
       effect next frame. */
    if ((c->flags & (F_EDIT | F_COMMIT)) == (F_EDIT | F_COMMIT) &&
        c->edit_item == EDIT_PAGE) {
        uint32_t n = c->acc;
        bool     typed = c->edit_len > 0u;

        end_edit(c);
        if (typed)
            atc_menu_set_items_per_page(c, (n > 255u) ? 255u : (unsigned)n);
    }

    if (c->enter_req != 0u) {
        c->path[c->nav_depth] = (unsigned char)(c->enter_req - 1u);
        c->nav_depth++;
        c->top[c->nav_depth] = 0u;
        c->enter_req = 0u;
        c->pending = 0u;
    }

    c->act = 0u;
    if (c->pending > c->level_numbered)
        c->pending = 0u;

    restore_cursor(c);
    return (atc_menu_status_t)c->status;
}
