/* SPDX-License-Identifier: MIT */
/**
 * @file menu_input.c
 * @brief One received byte, turned into navigation or a keystroke
 *
 * Nothing here paints. A key only moves the context — the page, the pending
 * prefix, the editor's accumulator — and the next frame is what shows it.
 *
 * An open editor takes the byte first: while one is up, every key belongs to it
 * and none of the navigation keys are live.
 */
#include "menu_internal.h"

static void go_up(atc_menu_ctx_t *c)
{
    if (c->nav_depth == 0u)
        return;
    c->nav_depth--;
    c->pending = 0u;
    c->msg = NULL;
}

/* acc*10 > N means the number cannot be extended, so act on it at once. */
static void key_digit(atc_menu_ctx_t *c, unsigned d)
{
    unsigned n = c->level_numbered;
    unsigned acc;

    if (c->pending == 0u && d == 0u) {
        go_up(c);
        return;
    }

    acc = (unsigned)c->pending * 10u + d;
    if (acc > n)
        acc = d;
    if (acc == 0u || acc > n) {
        /* Say so rather than swallow it: silence reads as a dropped keystroke,
           and on a serial line that is the first thing suspected. */
        c->pending = 0u;
        c->msg = "no such item";
        return;
    }
    if (acc * 10u > n) {
        c->act = (unsigned char)acc;
        c->pending = 0u;
    } else {
        c->pending = (unsigned char)acc;
    }
}

/*--- Inside an editor -------------------------------------------------------*/

static int digit_value(int ch, unsigned base)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (base == 16u) {
        if (ch >= 'a' && ch <= 'f')
            return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F')
            return ch - 'A' + 10;
    }
    return -1;
}

static void key_text(atc_menu_ctx_t *c, int ch)
{
    char *t = edit_text(c);

    if (ch == '\r' || ch == '\n') {
        c->flags |= F_COMMIT;
        return;
    }
    if (ch == 0x1b) {
        end_edit(c);
        return;
    }
    if (ch == 0x08 || ch == 0x7f) {
        if (c->edit_len > 0u)
            t[--c->edit_len] = '\0';
        return;
    }
    if (ch >= 0x20 && ch < 0x7f && c->edit_len < text_room(c)) {
        t[c->edit_len++] = (char)ch;
        t[c->edit_len] = '\0';
    }
}

/* Any printable steps to the next option, Backspace to the previous; acc and
   edit_base are enough to wrap here, without the widget in the loop. */
static void key_choice(atc_menu_ctx_t *c, int ch)
{
    unsigned n = c->edit_base;

    if (ch == '\r' || ch == '\n')
        c->flags |= F_COMMIT;
    else if (ch == 0x1b)
        end_edit(c);
    else if (n == 0u)
        return;
    else if (ch == 0x08 || ch == 0x7f)
        c->acc = (c->acc == 0u) ? n - 1u : c->acc - 1u;
    else if (ch >= 0x20 && ch < 0x7f)
        c->acc = (c->acc + 1u) % n;
}

static void key_edit(atc_menu_ctx_t *c, int ch)
{
    unsigned base = c->edit_base;
    int      d;

    if ((c->flags & F_TEXT) != 0u) {
        key_text(c, ch);
        return;
    }
    if ((c->flags & F_CHOICE) != 0u) {
        key_choice(c, ch);
        return;
    }

    if (ch == '\r' || ch == '\n') {
        c->flags |= F_COMMIT;
        return;
    }
    if (ch == 0x1b) {
        end_edit(c);
        return;
    }
    if (ch == 0x08 || ch == 0x7f) {
        if (c->edit_frac != NO_FRAC && c->edit_frac > 0u) {
            c->edit_frac--;
            c->acc /= base;
            c->edit_len--;
        } else if (c->edit_frac == 0u) {
            c->edit_frac = NO_FRAC;
        } else if (c->edit_len > 0u) {
            c->acc /= base;
            c->edit_len--;
        } else {
            c->flags &= (uint16_t)~F_NEG;
        }
        c->flags &= (uint16_t)~F_OVF;
        return;
    }
    if (ch == '-' && c->acc == 0u && c->edit_frac == NO_FRAC) {
        c->flags ^= F_NEG;
        return;
    }
    if (ch == '.' && base == 10u && c->edit_dec > 0u && c->edit_frac == NO_FRAC) {
        c->edit_frac = 0u;
        return;
    }

    d = digit_value(ch, base);
    if (d < 0)
        return;
    if (c->edit_frac != NO_FRAC && c->edit_frac >= c->edit_dec)
        return; /* extra fraction digits would only be truncated */
    if (c->acc > (0xFFFFFFFFu - (uint32_t)d) / base) {
        c->flags |= F_OVF;
        return;
    }
    c->acc = c->acc * base + (uint32_t)d;
    /* Leading zeros leave acc alone, so nothing but this bounds the count. */
    if (c->edit_len < 254u)
        c->edit_len++;
    if (c->edit_frac != NO_FRAC)
        c->edit_frac++;
}

/*--- The one entry point ----------------------------------------------------*/

void atc_menu_key(atc_menu_ctx_t *c, int byte)
{
    if (c == NULL || byte < 0)
        return;

    if ((c->flags & F_EDIT) != 0u) {
        key_edit(c, byte);
        return;
    }

    c->msg = NULL;

    if (byte >= '0' && byte <= '9') {
        key_digit(c, (unsigned)(byte - '0'));
        return;
    }

    switch (byte) {
    case '\r':
    case '\n':
        if (c->pending != 0u) {
            c->act = c->pending;
            c->pending = 0u;
        }
        break;
    case 0x08:
    case 0x7f:
        c->pending = 0u;
        break;
    case 0x1b:
        if (c->pending != 0u)
            c->pending = 0u;
        else
            go_up(c);
        break;
    case 'r':
        atc_menu_refresh(c);
        break;
    /* Numbers are handed out per page, so a prefix typed against this page
       means something else on the next one. Paging drops it. */
    case 'n':
        c->pending = 0u;
        if ((unsigned)c->top[c->nav_depth] + c->page_items < c->level_items)
            c->top[c->nav_depth] =
                (unsigned char)(c->top[c->nav_depth] + c->page_items);
        else
            c->msg = "last page";
        break;
    case 'p':
        c->pending = 0u;
        if (c->top[c->nav_depth] >= c->page_items)
            c->top[c->nav_depth] =
                (unsigned char)(c->top[c->nav_depth] - c->page_items);
        else
            c->msg = "first page";
        break;
    case 'i':
        /* No title: the items prompt states its own range and reads page_items
           live, so it has nothing to snapshot. */
        begin_edit(c, EDIT_PAGE, 10u, 0u, NULL, NULL);
        break;
    default:
        break;
    }
}
