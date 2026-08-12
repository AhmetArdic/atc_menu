/* SPDX-License-Identifier: MIT */
/**
 * @file menu_widget.c
 * @brief What the application declares, frame by frame
 *
 * Every entry an application can put on screen goes through item_slot(): it
 * hands out the declaration index, decides whether the item is on this page and
 * therefore whether it gets a number, paints the row, and answers whether the
 * key just pressed selected it. Everything below that is the difference between
 * one kind of value and another.
 *
 * Immediate mode: nothing is retained between frames but the editor, so a
 * label, a value string or a choice list need only stay valid for the call.
 */
#include "menu_internal.h"

#include <string.h>

/*---------------------------------------------------------------------------
 * The slot every item takes
 *-------------------------------------------------------------------------*/

static bool at_active(const atc_menu_ctx_t *c)
{
    return c->decl_depth == c->nav_depth;
}

static unsigned next_index(atc_menu_ctx_t *c)
{
    unsigned i = c->item[c->decl_depth];

    /* The count is one byte, so it has to stop before it wraps and two rows
       start sharing a number. */
    if (i < 254u)
        c->item[c->decl_depth] = (unsigned char)(i + 1u);
    else
        c->status = (signed char)ATC_MENU_ERR_STATE;
    return i;
}

/* Both are spent by the next item declared, on the active level or not:
   otherwise one set at the root survives into the first row of a submenu. */
static bool take_disable(atc_menu_ctx_t *c)
{
    bool d = (c->flags & F_DISABLE) != 0u;
    c->flags &= (uint16_t)~F_DISABLE;
    return d;
}

static unsigned take_style(atc_menu_ctx_t *c)
{
    unsigned s = c->item_style;
    c->item_style = 0u;
    return s;
}

static bool on_page(const atc_menu_ctx_t *c, unsigned item_i)
{
    unsigned top = c->top[c->nav_depth];

    return item_i >= top && item_i < top + c->page_items;
}

/* Numbers are handed out per page and start again at 1 on the next one, so a
   page never holds more of them than it has rows — which is what keeps one
   keystroke enough. Decoration takes a row and no number. */
static bool item_slot(atc_menu_ctx_t *c, const char *label, const char *value,
                      bool numbered, bool selectable, unsigned *out_num)
{
    unsigned item_i = next_index(c);
    unsigned num = 0u;
    bool     dim = take_disable(c);
    unsigned style = take_style(c);
    bool     visible;

    if (!at_active(c))
        return false;

    if (dim)
        selectable = false;

    if (item_i + 1u > c->level_items)
        c->level_items = (unsigned char)(item_i + 1u);

    visible = on_page(c, item_i);

    if (numbered && visible) {
        c->level_numbered = (unsigned char)(c->level_numbered + 1u);
        num = c->level_numbered;
    }
    if (out_num != NULL)
        *out_num = num;

    if (visible)
        row_item(c, item_i, num, label, value, dim, style);

    if (num != 0u && c->act == num && !selectable) {
        c->msg = dim ? "not available now" : "read-only";
        return false;
    }
    return selectable && num != 0u && c->act == num;
}

/*---------------------------------------------------------------------------
 * Decoration
 *-------------------------------------------------------------------------*/

void atc_menu_label(atc_menu_ctx_t *c, const char *text)
{
    if (c != NULL)
        (void)item_slot(c, text, NULL, false, false, NULL);
}

void atc_menu_separator(atc_menu_ctx_t *c)
{
    unsigned item_i;

    if (c == NULL)
        return;

    item_i = next_index(c);
    (void)take_disable(c);
    (void)take_style(c);
    if (!at_active(c))
        return;
    if (item_i + 1u > c->level_items)
        c->level_items = (unsigned char)(item_i + 1u);

    if (on_page(c, item_i))
        row_separator(c, item_i);
}

/*---------------------------------------------------------------------------
 * Numbers
 *
 * One description per value type, in flash, so the formatting and editing code
 * exists once instead of eighteen times.
 *-------------------------------------------------------------------------*/

typedef struct {
    unsigned char base;      /* 10 or 16 */
    unsigned char hexdigits; /* display width for base 16 */
    unsigned char is_signed;
    uint32_t      lo_mag;    /* magnitude of the most negative value */
    uint32_t      hi;
} numspec_t;

static const numspec_t SPEC_U8  = { 10u, 0u, 0u, 0u, 0xFFu };
static const numspec_t SPEC_U16 = { 10u, 0u, 0u, 0u, 0xFFFFu };
static const numspec_t SPEC_U32 = { 10u, 0u, 0u, 0u, 0xFFFFFFFFu };
static const numspec_t SPEC_I16 = { 10u, 0u, 1u, 32768u, 32767u };
static const numspec_t SPEC_I32 = { 10u, 0u, 1u, 2147483648u, 2147483647u };
static const numspec_t SPEC_X8  = { 16u, 2u, 0u, 0u, 0xFFu };
static const numspec_t SPEC_X16 = { 16u, 4u, 0u, 0u, 0xFFFFu };
static const numspec_t SPEC_X32 = { 16u, 8u, 0u, 0u, 0xFFFFFFFFu };

static void num_format(char *vb, size_t cap, uint32_t mag, bool neg,
                       const numspec_t *s, unsigned decimals)
{
    buf_t b;

    b.p = vb; b.cap = cap - 1u; b.len = 0u; b.body = 0u; b.vis = 0u;
    if (s->base == 16u) {
        bstr(&b, "0x");
        bhexdigits(&b, mag, s->hexdigits);
    } else {
        bnum(&b, mag, neg, decimals);
    }
    vb[(b.len < b.cap) ? b.len : b.cap] = '\0';
}

static bool num_item(atc_menu_ctx_t *c, const char *label, uint32_t mag,
                     bool neg, const numspec_t *s, unsigned decimals,
                     uint32_t *out_mag, bool *out_neg)
{
    char     vb[24];
    unsigned idx = 0u;
    unsigned frac;
    uint32_t got;
    bool     got_neg;
    bool     bad;

    if (c == NULL)
        return false;

    num_format(vb, sizeof vb, mag, neg, s, decimals);
    if (item_slot(c, label, vb, true, true, &idx))
        begin_edit(c, idx, s->base, decimals, label, vb);
    if (!at_active(c) || !commit_ready(c, idx))
        return false;

    frac = (c->edit_frac == NO_FRAC) ? 0u : c->edit_frac;
    got = c->acc;
    got_neg = (c->flags & F_NEG) != 0u;
    bad = (c->flags & F_OVF) != 0u;

    if (!bad && decimals > 0u) {
        unsigned shift = decimals - ((frac > decimals) ? decimals : frac);
        while (shift-- > 0u && !bad) {
            if (got > 429496729u)
                bad = true;
            else
                got *= 10u;
        }
    }

    if (!bad) {
        if (s->is_signed != 0u)
            bad = got > (got_neg ? s->lo_mag : s->hi);
        else
            bad = got_neg || got > s->hi;
    }

    if (bad)
        return refuse(c, "out of range");

    deliver(c);
    *out_mag = got;
    *out_neg = got_neg;
    return true;
}

static void num_ro(atc_menu_ctx_t *c, const char *label, uint32_t mag, bool neg,
                   const numspec_t *s, unsigned decimals)
{
    char vb[24];

    if (c == NULL)
        return;
    num_format(vb, sizeof vb, mag, neg, s, decimals);
    (void)item_slot(c, label, vb, true, false, NULL);
}

/* num_item() writes through the two pointers only when it returns true, so the
   zeros are never read — they are here because a compiler that inlines the call
   cannot always see that for itself. */
static bool num_edit_u(atc_menu_ctx_t *c, const char *label, uint32_t cur,
                       const numspec_t *s, uint32_t *out)
{
    uint32_t mag = 0u;
    bool     neg = false;

    if (!num_item(c, label, cur, false, s, 0u, &mag, &neg))
        return false;
    *out = mag;
    return true;
}

static bool num_edit_i(atc_menu_ctx_t *c, const char *label, int32_t cur,
                       const numspec_t *s, unsigned decimals, int32_t *out)
{
    uint32_t mag = 0u;
    bool     neg = false;

    if (!num_item(c, label, magnitude(cur), cur < 0, s, decimals, &mag, &neg))
        return false;

    if (!neg)
        *out = (int32_t)mag;
    else if (mag > (uint32_t)INT32_MAX)
        *out = INT32_MIN; /* negating it instead would be undefined */
    else
        *out = -(int32_t)mag;
    return true;
}

/*---------------------------------------------------------------------------
 * Read-only rows
 *-------------------------------------------------------------------------*/

void atc_menu_uint8_ro(atc_menu_ctx_t *c, const char *label, atc_menu_u8 v)
{
    num_ro(c, label, v, false, &SPEC_U8, 0u);
}

void atc_menu_uint16_ro(atc_menu_ctx_t *c, const char *label, uint16_t v)
{
    num_ro(c, label, v, false, &SPEC_U16, 0u);
}

void atc_menu_uint32_ro(atc_menu_ctx_t *c, const char *label, uint32_t v)
{
    num_ro(c, label, v, false, &SPEC_U32, 0u);
}

void atc_menu_int16_ro(atc_menu_ctx_t *c, const char *label, int16_t v)
{
    num_ro(c, label, magnitude(v), v < 0, &SPEC_I16, 0u);
}

void atc_menu_int32_ro(atc_menu_ctx_t *c, const char *label, int32_t v)
{
    num_ro(c, label, magnitude(v), v < 0, &SPEC_I32, 0u);
}

void atc_menu_hex8_ro(atc_menu_ctx_t *c, const char *label, atc_menu_u8 v)
{
    num_ro(c, label, v, false, &SPEC_X8, 0u);
}

void atc_menu_hex16_ro(atc_menu_ctx_t *c, const char *label, uint16_t v)
{
    num_ro(c, label, v, false, &SPEC_X16, 0u);
}

void atc_menu_hex32_ro(atc_menu_ctx_t *c, const char *label, uint32_t v)
{
    num_ro(c, label, v, false, &SPEC_X32, 0u);
}

void atc_menu_fix_ro(atc_menu_ctx_t *c, const char *label, int32_t v,
                     unsigned decimals)
{
    num_ro(c, label, magnitude(v), v < 0, &SPEC_I32,
           (decimals > 4u) ? 4u : decimals);
}

void atc_menu_bool_ro(atc_menu_ctx_t *c, const char *label, bool v)
{
    if (c != NULL)
        (void)item_slot(c, label, v ? "[X]" : "[ ]", true, false, NULL);
}

void atc_menu_text_ro(atc_menu_ctx_t *c, const char *label, const char *text)
{
    if (c != NULL)
        (void)item_slot(c, label, text, true, false, NULL);
}

/*---------------------------------------------------------------------------
 * Editable rows
 *-------------------------------------------------------------------------*/

bool atc_menu_uint8(atc_menu_ctx_t *c, const char *label, atc_menu_u8 *v)
{
    uint32_t nv;

    if (v == NULL || !num_edit_u(c, label, *v, &SPEC_U8, &nv))
        return false;
    *v = (atc_menu_u8)nv;
    return true;
}

bool atc_menu_uint16(atc_menu_ctx_t *c, const char *label, uint16_t *v)
{
    uint32_t nv;

    if (v == NULL || !num_edit_u(c, label, *v, &SPEC_U16, &nv))
        return false;
    *v = (uint16_t)nv;
    return true;
}

bool atc_menu_uint32(atc_menu_ctx_t *c, const char *label, uint32_t *v)
{
    uint32_t nv;

    if (v == NULL || !num_edit_u(c, label, *v, &SPEC_U32, &nv))
        return false;
    *v = nv;
    return true;
}

bool atc_menu_hex8(atc_menu_ctx_t *c, const char *label, atc_menu_u8 *v)
{
    uint32_t nv;

    if (v == NULL || !num_edit_u(c, label, *v, &SPEC_X8, &nv))
        return false;
    *v = (atc_menu_u8)nv;
    return true;
}

bool atc_menu_hex16(atc_menu_ctx_t *c, const char *label, uint16_t *v)
{
    uint32_t nv;

    if (v == NULL || !num_edit_u(c, label, *v, &SPEC_X16, &nv))
        return false;
    *v = (uint16_t)nv;
    return true;
}

bool atc_menu_hex32(atc_menu_ctx_t *c, const char *label, uint32_t *v)
{
    uint32_t nv;

    if (v == NULL || !num_edit_u(c, label, *v, &SPEC_X32, &nv))
        return false;
    *v = nv;
    return true;
}

bool atc_menu_int16(atc_menu_ctx_t *c, const char *label, int16_t *v)
{
    int32_t nv;

    if (v == NULL || !num_edit_i(c, label, *v, &SPEC_I16, 0u, &nv))
        return false;
    *v = (int16_t)nv;
    return true;
}

bool atc_menu_int32(atc_menu_ctx_t *c, const char *label, int32_t *v)
{
    int32_t nv;

    if (v == NULL || !num_edit_i(c, label, *v, &SPEC_I32, 0u, &nv))
        return false;
    *v = nv;
    return true;
}

bool atc_menu_fix(atc_menu_ctx_t *c, const char *label, int32_t *v,
                  unsigned decimals)
{
    int32_t nv;

    if (v == NULL)
        return false;
    if (!num_edit_i(c, label, *v, &SPEC_I32, (decimals > 4u) ? 4u : decimals, &nv))
        return false;
    *v = nv;
    return true;
}

bool atc_menu_bool(atc_menu_ctx_t *c, const char *label, bool *v)
{
    unsigned idx = 0u;

    if (c == NULL || v == NULL)
        return false;
    if (item_slot(c, label, *v ? "[X]" : "[ ]", true, true, &idx)) {
        *v = !*v;
        return true;
    }
    return false;
}

bool atc_menu_text(atc_menu_ctx_t *c, const char *label, char *buf, size_t cap)
{
    unsigned    idx = 0u;
    const char *shown = buf;

    if (c == NULL || buf == NULL || cap == 0u)
        return false;

    /* level_numbered has not counted this item yet, so its number is level_numbered + 1 */
    if (at_active(c) && (c->flags & (F_EDIT | F_TEXT)) == (F_EDIT | F_TEXT) &&
        c->edit_item == (unsigned)c->level_numbered + 1u)
        shown = edit_text_c(c);

    if (item_slot(c, label, shown, true, true, &idx))
        begin_edit_text(c, idx, label, buf);
    if (!at_active(c) || !commit_ready(c, idx) || (c->flags & F_TEXT) == 0u)
        return false;

    if ((size_t)c->edit_len + 1u > cap)
        return refuse(c, "too long");

    deliver(c);
    memcpy(buf, edit_text_c(c), (size_t)c->edit_len + 1u);
    return true;
}

bool atc_menu_choice(atc_menu_ctx_t *c, const char *label, unsigned *index,
                     const char *const *choices, unsigned count)
{
    unsigned idx = 0u;

    if (c == NULL || index == NULL || choices == NULL || count == 0u)
        return false;
    if (*index >= count)
        *index = 0u;

    /* The row keeps showing what is committed; the candidate lives in the
       prompt, so both are on screen while stepping through. */
    if (item_slot(c, label, choices[*index], true, true, &idx)) {
        unsigned first = (*index + 1u) % count;
        begin_edit_choice(c, idx, first, count, label, choices[first]);
        return false;
    }

    /* idx is 0 for a row scrolled off the page, which is what keeps this from
       matching an editor opened on some other row that happens to share it. */
    if (idx == 0u || c->edit_item != idx ||
        (c->flags & (F_EDIT | F_CHOICE)) != (F_EDIT | F_CHOICE))
        return false;

    if (c->acc >= count)
        c->acc = 0u; /* the list shrank under an open editor */

    /* Immediate mode redraws the title every frame, which is what lets the
       bracket follow the candidate without the label being kept alive. */
    set_edit_title(c, label, choices[c->acc]);
    if (!commit_ready(c, idx))
        return false;

    deliver(c);
    *index = (unsigned)c->acc;
    return true;
}

bool atc_menu_action(atc_menu_ctx_t *c, const char *label)
{
    if (c == NULL)
        return false;
    return item_slot(c, label, NULL, true, true, NULL);
}

/*---------------------------------------------------------------------------
 * Levels
 *-------------------------------------------------------------------------*/

bool atc_menu_submenu(atc_menu_ctx_t *c, const char *label)
{
    unsigned i;
    unsigned num;
    unsigned style;
    bool     dim;

    if (c == NULL)
        return false;

    i = next_index(c);
    dim = take_disable(c);
    style = take_style(c);

    if (c->decl_depth < c->nav_depth) {
        if (i != c->path[c->decl_depth])
            return false;
        if ((unsigned)c->decl_depth + 1u >= ATC_MENU_MAX_DEPTH) {
            c->status = (signed char)ATC_MENU_ERR_STATE;
            return false;
        }
        c->decl_depth++;
        c->item[c->decl_depth] = 0u;
        c->crumb[c->decl_depth - 1u] = label;
        return true;
    }
    if (c->decl_depth > c->nav_depth)
        return false;

    if (i + 1u > c->level_items)
        c->level_items = (unsigned char)(i + 1u);

    num = 0u;
    if (on_page(c, i)) {
        c->level_numbered = (unsigned char)(c->level_numbered + 1u);
        num = c->level_numbered;
        row_item(c, i, num, label, ">", dim, style);
    }

    /* Descending is deferred to frame_end so the rest of this frame keeps
       describing the level the user is still looking at. */
    if (!dim && num != 0u && c->act == num) {
        if ((unsigned)c->nav_depth + 1u >= ATC_MENU_MAX_DEPTH)
            c->status = (signed char)ATC_MENU_ERR_STATE;
        else
            c->enter_req = (unsigned char)(i + 1u);
        c->act = 0u;
    }
    return false;
}

void atc_menu_submenu_end(atc_menu_ctx_t *c)
{
    if (c == NULL)
        return;
    if (c->decl_depth == 0u) {
        c->status = (signed char)ATC_MENU_ERR_STATE;
        return;
    }
    c->decl_depth--;
}
