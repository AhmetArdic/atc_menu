/**
 * @file menu_internal.h
 * @brief ATC Menu - internal interfaces shared between translation units; not installed.
 * @author Ahmet Talha ARDIC
 * @date   2026-07-31
 */
#ifndef ATC_MENU_INTERNAL_H
#define ATC_MENU_INTERNAL_H

#include "atc_menu/menu.h"

#define ATC_MENU_MODE_SELECT         0
#define ATC_MENU_MODE_EDIT           1
#define ATC_MENU_MODE_CHOICE         2
#define ATC_MENU_MODE_ITEMS_PER_PAGE 3

/* Item count of the page being shown - read from ROM, never cached, so no
 * copy can go stale on navigation. */
#define ATC_MENU_COUNT(ctx) ((ctx)->nav[(ctx)->depth].page->count)

/* menu_fmt.c - all return the number of chars written (no NUL). */
unsigned atc_menu_fmt_u32(char *dst, uint32_t v);
unsigned atc_menu_fmt_i32(char *dst, int32_t v);
unsigned atc_menu_fmt_hex(char *dst, uint32_t v, unsigned digits);
unsigned atc_menu_fmt_fix(char *dst, int32_t v, unsigned decimals, int is_signed);

/* menu_core.c - accessor calls, null-checked in one place. */
int32_t atc_menu_bind_get(const atc_menu_bind_t *b);
/* Returns 0 if the setter refused the value, or there was no setter at all. */
int atc_menu_bind_set(const atc_menu_bind_t *b, int32_t v);

/* menu_core.c */
void atc_menu_cmd(atc_menu_ctx_t *ctx, char c);
void atc_menu_enter(atc_menu_ctx_t *ctx);
void atc_menu_select_try(atc_menu_ctx_t *ctx);
void atc_menu_choice_next(atc_menu_ctx_t *ctx);
/* Not public API; also used directly by tests. */
void atc_menu_set_items_per_page(atc_menu_ctx_t *ctx, unsigned n);

/* menu_render.c */
void atc_menu_render_flush(atc_menu_ctx_t *ctx);
void atc_menu_request_full(atc_menu_ctx_t *ctx);

/* Row count of one item (1 except TEXT). */
unsigned atc_menu_item_rows(const atc_menu_item_t *it);
/* Item count of the page starting at `start` that fits ctx->items_per_page. */
atc_menu_u8 atc_menu_page_items(const atc_menu_ctx_t *ctx, atc_menu_u8 start);
/* Start index of the page before `start`. */
atc_menu_u8 atc_menu_page_before(const atc_menu_ctx_t *ctx, atc_menu_u8 start);

#endif /* ATC_MENU_INTERNAL_H */
