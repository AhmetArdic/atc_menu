/**
 * @file menu_core.c
 * @brief ATC Menu - navigation stack, item activation, command dispatch.
 * @author Ahmet Talha ARDIC
 * @date   2026-07-31
 */
#include <string.h>

#include "menu_internal.h"

static const char MSG_HELP[]    = "1-9 select, 0/b back, r refresh, n/p page, i items/page";
static const char MSG_BADSEL[]  = "Invalid selection";
static const char MSG_INVALID[] = "Invalid value";
static const char MSG_RANGE[]   = "Out of range";
static const char MSG_REJECT[]  = "Rejected";

/* Items per page until ATC_MENU_ITEMS_PER_PAGE says otherwise; 12 rows plus
 * six lines of chrome fit a 24-row terminal. */
#define ITEMS_PER_PAGE_INIT 12

static const atc_menu_item_t *cur_items(const atc_menu_ctx_t *ctx)
{
    return ctx->nav[ctx->depth].page->items;
}

int32_t atc_menu_bind_get(const atc_menu_bind_t *b)
{
    return b->get ? b->get(b->arg) : 0;
}

int atc_menu_bind_set(const atc_menu_bind_t *b, int32_t v)
{
    return b->set ? b->set(b->arg, v) : 0;
}

/* How many items on the visible page carry a selection number - the range a
 * typed number can address. */
static unsigned sel_count(const atc_menu_ctx_t *ctx)
{
    const atc_menu_item_t *it = cur_items(ctx);
    atc_menu_u8 i, s = ctx->nav[ctx->depth].start;
    atc_menu_u8 end = (atc_menu_u8)(s + atc_menu_page_items(ctx, s));
    unsigned n = 0;

    for (i = s; i < end; i++)
        if (it[i].kind != ATC_MENU_KIND_LABEL &&
            it[i].kind != ATC_MENU_KIND_SEPARATOR &&
            it[i].kind != ATC_MENU_KIND_TEXT)
            n++;
    return n;
}

/* Digits typed so far, capped so the multiply below cannot overflow. */
static unsigned input_num(const atc_menu_ctx_t *ctx)
{
    unsigned num = 0, i;

    for (i = 0; i < ctx->input_len && num <= 255; i++)
        num = num * 10u + (unsigned)(ctx->input[i] - '0');
    return num;
}

void atc_menu_init(atc_menu_ctx_t *ctx, const atc_menu_info_t *info,
                   const atc_menu_page_t *root,
                   atc_menu_sink_fn sink, void *user)
{
    memset(ctx, 0, sizeof *ctx);
    ctx->info = info;
    ctx->sink = sink;
    ctx->user = user;
    ctx->nav[0].page = root;
    ctx->items_per_page = ITEMS_PER_PAGE_INIT;
    ctx->dirty_row = 0xFF;
    ctx->prompt_pos = 0xFF;
    ctx->draw_pos = 0;  /* first update paints the full page */
}

void atc_menu_refresh(atc_menu_ctx_t *ctx)
{
    atc_menu_request_full(ctx);
}

void atc_menu_set_items_per_page(atc_menu_ctx_t *ctx, unsigned n)
{
    atc_menu_u8 d;

    if (!n)
        n = 1;
    if (n > ATC_MENU_ITEMS_PER_PAGE_MAX)
        n = ATC_MENU_ITEMS_PER_PAGE_MAX;
    ctx->items_per_page = (atc_menu_u8)n;

    /* Old offsets may not be valid page boundaries under the new budget. */
    for (d = 0; d < ATC_MENU_CFG_MAX_DEPTH; d++)
        ctx->nav[d].start = 0;
    atc_menu_request_full(ctx);
}

atc_menu_status_t atc_menu_update(atc_menu_ctx_t *ctx)
{
    atc_menu_render_flush(ctx);
    if (ctx->draw_pos != 0xFF || ctx->dirty_row != 0xFF ||
        ctx->prompt_pos != 0xFF)
        return ATC_MENU_BUSY;
    return ATC_MENU_IDLE;
}

static void fail(atc_menu_ctx_t *ctx, const char *msg)
{
    ctx->msg = msg;
    ctx->msg_err = 1;
}

/* A callback that refused may have explained itself via atc_menu_message();
 * only fall back to the generic wording when it did not. */
static void fail_cb(atc_menu_ctx_t *ctx)
{
    if (!ctx->msg)
        fail(ctx, MSG_REJECT);
}

void atc_menu_message(atc_menu_ctx_t *ctx, const char *msg, int error)
{
    ctx->msg = msg;
    ctx->msg_err = (atc_menu_u8)(error != 0);
    ctx->prompt_pos = 0;
}

void atc_menu_cmd(atc_menu_ctx_t *ctx, char c)
{
    atc_menu_u8 *start = &ctx->nav[ctx->depth].start;

    switch (c) {
    case '0':
    case 'b':
        if (ctx->depth) {
            ctx->depth--;
            atc_menu_request_full(ctx);
        }
        break;
    case 'r':
        atc_menu_request_full(ctx);
        break;
    case '?':
        ctx->msg = MSG_HELP;
        ctx->msg_err = 0;
        break;
    case 'n': {
        atc_menu_u8 n = atc_menu_page_items(ctx, *start);

        if (*start + n < ATC_MENU_COUNT(ctx)) {
            *start = (atc_menu_u8)(*start + n);
            atc_menu_request_full(ctx);
        }
        break;
    }
    case 'p':
        if (*start) {
            *start = atc_menu_page_before(ctx, *start);
            atc_menu_request_full(ctx);
        }
        break;
    case 'i':
        ctx->mode = ATC_MENU_MODE_ITEMS_PER_PAGE;
        ctx->input_len = 0;
        break;
    default:
        break;
    }
}

static void activate(atc_menu_ctx_t *ctx, atc_menu_u8 idx, unsigned num)
{
    const atc_menu_item_t *it = &cur_items(ctx)[idx];

    switch (it->kind) {
    case ATC_MENU_KIND_ACTION: {
        const atc_menu_action_t *d = (const atc_menu_action_t *)it->detail;
        if (d->on_click)
            d->on_click(d->arg);
        break;
    }
    case ATC_MENU_KIND_CHECKBOX: {
        const atc_menu_bind_t *b = (const atc_menu_bind_t *)it->detail;
        /* A NULL setter is read-only by construction, not a refusal, so it is
         * not reported as one - selecting it just redraws. */
        if (b->set && !atc_menu_bind_set(b, atc_menu_bind_get(b) ? 0 : 1))
            fail_cb(ctx);
        ctx->dirty_row = idx;
        break;
    }
    case ATC_MENU_KIND_CHOICE: {
        const atc_menu_choice_t *d = (const atc_menu_choice_t *)it->detail;
        int32_t v;
        if (!d->count)
            break;
        /* The bound value can sit outside the list; wrap to the first option
         * rather than index past its end. */
        v = atc_menu_bind_get(&d->bind);
        ctx->choice_val = (v >= 0 && v < (int32_t)d->count - 1) ?
                          (atc_menu_u8)(v + 1) : 0;
        ctx->mode = ATC_MENU_MODE_CHOICE;
        ctx->edit_item = idx;
        ctx->edit_num = (atc_menu_u8)num;
        break;
    }
    case ATC_MENU_KIND_SUBMENU: {
        const atc_menu_page_t *p = (const atc_menu_page_t *)it->detail;
        if (ctx->depth + 1 < ATC_MENU_CFG_MAX_DEPTH) {
            ctx->depth++;
            ctx->nav[ctx->depth].page = p;
            ctx->nav[ctx->depth].start = 0;
            atc_menu_request_full(ctx);
        }
        break;
    }
    case ATC_MENU_KIND_CUSTOM: {
        const atc_menu_custom_t *d = (const atc_menu_custom_t *)it->detail;
        if (!d->edit) {        /* nothing to type into; same as a READOUT */
            ctx->dirty_row = idx;
            break;
        }
        ctx->mode = ATC_MENU_MODE_EDIT;
        ctx->edit_item = idx;
        ctx->edit_num = (atc_menu_u8)num;
        ctx->input_len = 0;
        break;
    }
    case ATC_MENU_KIND_READOUT:
        ctx->dirty_row = idx;  /* selecting a read-only item refreshes it */
        break;
    default: /* NUMBER, HEX */
        ctx->mode = ATC_MENU_MODE_EDIT;
        ctx->edit_item = idx;
        ctx->edit_num = (atc_menu_u8)num;
        ctx->input_len = 0;
        break;
    }
}

/* No count==0 guard: activate() only enters CHOICE mode when count > 0,
 * and count is fixed ROM data, so that invariant always holds here. */
void atc_menu_choice_next(atc_menu_ctx_t *ctx)
{
    const atc_menu_item_t *it = &cur_items(ctx)[ctx->edit_item];
    const atc_menu_choice_t *d = (const atc_menu_choice_t *)it->detail;

    ctx->choice_val = (atc_menu_u8)((ctx->choice_val + 1u) % d->count);
}

static void select_commit(atc_menu_ctx_t *ctx)
{
    unsigned num = input_num(ctx), n = 0;
    atc_menu_u8 idx, s = ctx->nav[ctx->depth].start;
    atc_menu_u8 end = (atc_menu_u8)(s + atc_menu_page_items(ctx, s));
    const atc_menu_item_t *it = cur_items(ctx);

    if (!ctx->input_len)
        return;
    ctx->input_len = 0;

    for (idx = s; idx < end; idx++) {
        if (it[idx].kind == ATC_MENU_KIND_LABEL ||
            it[idx].kind == ATC_MENU_KIND_SEPARATOR ||
            it[idx].kind == ATC_MENU_KIND_TEXT)
            continue;
        if (++n == num) {
            activate(ctx, idx, num);
            return;
        }
    }
    fail(ctx, MSG_BADSEL);
}

/*
 * Instant selection: a typed number is acted on the moment no longer number
 * could still be meant. With `nsel` selectable items on the page, the digits
 * so far can only grow into a valid selection while num * 10 <= nsel; past
 * that point the number is unambiguous and waiting would buy nothing.
 *
 * So a page of up to 9 items never needs Enter at all, and on a longer one
 * only the leading digit of a two-digit number pauses - the next digit
 * resolves it (Enter still commits it, and Backspace/Ctrl-C drop it). No
 * timer and no time base are involved, which is what makes this work the same
 * on a 32 kHz MSP430 as on a host.
 */
void atc_menu_select_try(atc_menu_ctx_t *ctx)
{
    if (input_num(ctx) * 10u > sel_count(ctx))
        select_commit(ctx);
}

/* Accumulates one digit, refusing what would not fit a uint32_t; the type's
 * own narrower limit is checked once at the end. */
static int mag_push(uint32_t *mag, unsigned d)
{
    if (*mag > (0xFFFFFFFFu - d) / 10u)
        return 0;
    *mag = *mag * 10u + (uint32_t)d;
    return 1;
}

/* Decimal parse with optional '.'; the result is scaled by 10^decimals. The
 * only bound is what 32 bits hold, so an unsigned row reaches 4294967295 and
 * the setter gets the modular int32_t of the same bits. */
static int parse_num(const char *s, unsigned len, unsigned decimals,
                     int32_t *out)
{
    uint32_t mag = 0;
    unsigned i = 0, fdig = 0, any = 0, dot = 0, neg = 0;

    if (i < len && s[i] == '-') {
        neg = 1;
        i++;
    }
    for (; i < len; i++) {
        char c = s[i];
        if (c == '.') {
            if (dot || !decimals)
                return 0;
            dot = 1;
            continue;
        }
        if (c < '0' || c > '9')
            return 0;
        if (dot) {
            if (fdig == decimals)
                return 0;
            fdig++;
        }
        if (!mag_push(&mag, (unsigned)(c - '0')))
            return 0;
        any = 1;
    }
    if (!any)
        return 0;
    while (fdig < decimals) {
        if (!mag_push(&mag, 0))
            return 0;
        fdig++;
    }
    /* mag_push() held `mag` to 32 bits; a negative has to fit int32_t. */
    if (neg && mag > 0x7FFFFFFFu)
        return 0;
    *out = neg ? -(int32_t)mag : (int32_t)mag;
    return 1;
}

static int parse_hex(const char *s, unsigned len, uint32_t *out)
{
    uint32_t v = 0;
    unsigned i = 0, any = 0;

    if (len >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        i = 2;
    for (; i < len; i++) {
        char c = s[i];
        unsigned d;
        if (c >= '0' && c <= '9')
            d = (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f')
            d = (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            d = (unsigned)(c - 'A' + 10);
        else
            return 0;
        if (v > 0x0FFFFFFFu)
            return 0;
        v = (v << 4) | d;
        any = 1;
    }
    if (!any)
        return 0;
    *out = v;
    return 1;
}

static void edit_ok(atc_menu_ctx_t *ctx)
{
    ctx->mode = ATC_MENU_MODE_SELECT;
    ctx->dirty_row = ctx->edit_item;
    ctx->input_len = 0;
}

static void edit_enter(atc_menu_ctx_t *ctx)
{
    const atc_menu_item_t *it;

    /* Not tied to any item, so handled before cur_items()[ctx->edit_item]. */
    if (ctx->mode == ATC_MENU_MODE_ITEMS_PER_PAGE) {
        int32_t v;

        if (!ctx->input_len) {  /* empty enter = cancel */
            ctx->mode = ATC_MENU_MODE_SELECT;
            return;
        }
        if (!parse_num(ctx->input, ctx->input_len, 0, &v))
            fail(ctx, MSG_INVALID);
        else {
            atc_menu_set_items_per_page(ctx, (unsigned)v);  /* clamps, no reject */
            ctx->mode = ATC_MENU_MODE_SELECT;
            ctx->input_len = 0;
        }
        return;
    }

    it = &cur_items(ctx)[ctx->edit_item];

    if (ctx->mode == ATC_MENU_MODE_CHOICE) {
        /* CHOICE never buffers into ctx->input, so input_len is always 0
         * here - this must run before the "empty enter = cancel" guard
         * below, or every commit would be misread as a cancel. */
        const atc_menu_choice_t *d = (const atc_menu_choice_t *)it->detail;
        if (!atc_menu_bind_set(&d->bind, (int32_t)ctx->choice_val))
            fail_cb(ctx);   /* stays in choice mode, preview intact */
        else
            edit_ok(ctx);
        return;
    }

    if (!ctx->input_len) {  /* empty enter = cancel */
        ctx->mode = ATC_MENU_MODE_SELECT;
        return;
    }

    switch (it->kind) {
    case ATC_MENU_KIND_NUMBER: {
        const atc_menu_num_t *d = (const atc_menu_num_t *)it->detail;
        int32_t v;
        if (!parse_num(ctx->input, ctx->input_len, d->decimals, &v))
            fail(ctx, MSG_INVALID);
        else if (!atc_menu_bind_set(&d->bind, v))
            fail_cb(ctx);
        else
            edit_ok(ctx);
        break;
    }
    case ATC_MENU_KIND_HEX: {
        const atc_menu_hex_t *d = (const atc_menu_hex_t *)it->detail;
        uint32_t v;
        if (!parse_hex(ctx->input, ctx->input_len, &v))
            fail(ctx, MSG_INVALID);
        else if (d->bits < 32 && (v >> d->bits) != 0)
            fail(ctx, MSG_RANGE);
        else if (!atc_menu_bind_set(&d->bind, (int32_t)v))
            fail_cb(ctx);
        else
            edit_ok(ctx);
        break;
    }
    case ATC_MENU_KIND_CUSTOM: {
        const atc_menu_custom_t *d = (const atc_menu_custom_t *)it->detail;
        /* activate() only opens the editor when `edit` is there; edit_ok()
         * redraws the row from show(). */
        if (!d->edit(d->arg, ctx->input))
            fail_cb(ctx);
        else
            edit_ok(ctx);
        break;
    }
    default:
        ctx->mode = ATC_MENU_MODE_SELECT;
        break;
    }
}

void atc_menu_enter(atc_menu_ctx_t *ctx)
{
    ctx->input[ctx->input_len] = '\0';
    if (ctx->mode == ATC_MENU_MODE_SELECT)
        select_commit(ctx);
    else
        edit_enter(ctx);
}
