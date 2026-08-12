/* SPDX-License-Identifier: MIT */
/**
 * @file menu_internal.h
 * @brief What the menu's translation units share; not part of the distribution
 *
 * One design, cut along the seams the code already had:
 *
 *   menu_buf.c     bytes and numbers into a line buffer
 *   menu_draw.c    which row a thing lands on, and what it looks like
 *   menu_edit.c    the editor's state and its slice of the caller's buffer
 *   menu_input.c   a received byte, turned into navigation or a keystroke
 *   menu_widget.c  what the application declares, frame by frame
 *   menu_core.c    the context's life: init, terminal, frame, teardown
 *
 * Whatever one file owns stays static inside it, so what is declared here is
 * the whole of what the pieces say to each other, and none of it is installed.
 * The atc_menu_ prefix is left to the API: a name without it is a name no
 * application is meant to reach for.
 */
#ifndef MENU_INTERNAL_H
#define MENU_INTERNAL_H

#include "atc_menu/menu.h"

/*---------------------------------------------------------------------------
 * Layout
 *-------------------------------------------------------------------------*/

#define NUMW 7u  /* "   1   " */
#define VALW 12u /* widest formatted value: -214748.3648 */

/* Most the chrome can spend: banner (2) + rule + breadcrumb + closing rule +
   footer + message + prompt. An empty banner line is not painted and not
   charged, so the real cost is 6, 7 or 8 — head_rows() in menu_draw.c has the
   exact figure. Only the floor atc_menu_init() enforces needs the worst
   case. */
#define CHROME_ROWS 8u

/*---------------------------------------------------------------------------
 * The caller's buffer, and where it divides
 *
 *   [ one row ][ editor title \0 ][ keystrokes \0 ]
 *              ^ edit_area        ^ edit_head
 *
 * Nothing but arithmetic on cols, buf_cap and edit_head, so it is here rather
 * than behind a call: menu_draw.c wants the first part, menu_edit.c the rest,
 * and neither should be asking the other where the line is.
 *-------------------------------------------------------------------------*/

/* Bounding a row is what keeps the two halves apart, and the split needs no
   field of its own: cols already says where it is. */
static inline size_t row_cap(const atc_menu_ctx_t *c)
{
    return ATC_MENU_ROW_BYTES(c->cols);
}

/* Whatever the caller passed beyond one row. atc_menu_init() has already
   refused anything smaller than ATC_MENU_BUF_BYTES(cols). */
static inline size_t edit_room(const atc_menu_ctx_t *c)
{
    return c->buf_cap - row_cap(c);
}

static inline char *edit_area(atc_menu_ctx_t *c)
{
    return c->buf + row_cap(c);
}

static inline const char *edit_area_c(const atc_menu_ctx_t *c)
{
    return c->buf + row_cap(c);
}

static inline char *edit_text(atc_menu_ctx_t *c)
{
    return edit_area(c) + c->edit_head;
}

static inline const char *edit_text_c(const atc_menu_ctx_t *c)
{
    return edit_area_c(c) + c->edit_head;
}

/* The title is fixed once the editor opens, so what is left is what can be
   typed — a short label simply buys room. */
static inline unsigned text_room(const atc_menu_ctx_t *c)
{
    return (unsigned)(edit_room(c) - c->edit_head - 1u);
}

/*---------------------------------------------------------------------------
 * Context flags
 *-------------------------------------------------------------------------*/

#define F_EDIT    0x0001u
#define F_COMMIT  0x0002u
#define F_NEG     0x0004u
#define F_SAVED   0x0008u
#define F_DISABLE 0x0010u
#define F_STOP    0x0020u
#define F_OVF     0x0040u
#define F_TEXT    0x0080u
/* A value left this frame and the application has not answered yet: the editor
   state is still whole, so atc_menu_reject() can put it back on screen. */
#define F_DELIVERED 0x0100u
#define F_CHOICE    0x0200u

/* F_TEXT and F_CHOICE say which editor F_EDIT means; they outlive it by design,
   since a delivered value keeps its kind until frame_end retires it. */
#define F_EDIT_KIND (F_TEXT | F_CHOICE)

/* edit_frac when no decimal point has been typed. */
#define NO_FRAC 0xFFu

/* Numbers run 1..247, and an item scrolled off the page reports 0 — which is
   why 0 cannot stand for the items-per-page editor: an off-page widget would
   answer its commit and write the typed value into the application. */
#define EDIT_PAGE 0xFFu

/*---------------------------------------------------------------------------
 * menu_buf.c — a line, built straight into the caller's buffer
 *-------------------------------------------------------------------------*/

/* len keeps counting past cap, so an overflow is detectable without ever
   writing out of bounds. */
typedef struct {
    char  *p;
    size_t cap;
    size_t len;
    size_t body; /* offset where the content after the ANSI prefix starts */
    size_t sig;  /* offset the signature starts at: past the cursor address */
    size_t vis;  /* columns written; escape sequences do not count */
} buf_t;

/* Every byte of every row goes through this one, which is why it is here and
   not behind a call: the call would cost more than the body does. */
static inline void bput(buf_t *b, char ch)
{
    if (b->len < b->cap)
        b->p[b->len] = ch;
    b->len++;
    b->vis++;
}

void bstr(buf_t *b, const char *s);
void bclip(buf_t *b, const char *s, size_t max);
void bsgr(buf_t *b, const char *s);
void bpad(buf_t *b, size_t n);
void bu32(buf_t *b, uint32_t v);
void bnum(buf_t *b, uint32_t mag, bool neg, unsigned decimals);
void bhexdigits(buf_t *b, uint32_t v, unsigned digits);

/* A value's magnitude and sign travel separately from the widget all the way to
   bnum, so the most negative int32_t keeps its magnitude instead of
   overflowing on the way. */
static inline uint32_t magnitude(int32_t v)
{
    return (v < 0) ? (uint32_t)0u - (uint32_t)v : (uint32_t)v;
}

/*---------------------------------------------------------------------------
 * menu_draw.c — where a row goes and what it looks like
 *
 * Item rows are addressed by declaration index, not by screen row: the page
 * offset and the chrome above it are the drawing side's business, so no caller
 * has to know how tall the banner came out.
 *-------------------------------------------------------------------------*/

unsigned page_items_max(const atc_menu_ctx_t *c);

void row_item(atc_menu_ctx_t *c, unsigned item_i, unsigned number,
              const char *label, const char *value, bool dim, unsigned style);
void row_separator(atc_menu_ctx_t *c, unsigned item_i);
void paint_chrome(atc_menu_ctx_t *c);
void blank_tail(atc_menu_ctx_t *c);
void restore_cursor(atc_menu_ctx_t *c);

/*---------------------------------------------------------------------------
 * menu_edit.c — the editor's state and its slice of the caller's buffer
 *-------------------------------------------------------------------------*/

void set_edit_title(atc_menu_ctx_t *c, const char *label, const char *value);

void begin_edit(atc_menu_ctx_t *c, unsigned index, unsigned base,
                unsigned decimals, const char *label, const char *value);
void begin_edit_text(atc_menu_ctx_t *c, unsigned index, const char *label,
                     const char *value);
void begin_edit_choice(atc_menu_ctx_t *c, unsigned index, unsigned first,
                       unsigned count, const char *label, const char *value);
void end_edit(atc_menu_ctx_t *c);

bool commit_ready(atc_menu_ctx_t *c, unsigned index);
void deliver(atc_menu_ctx_t *c);
bool refuse(atc_menu_ctx_t *c, const char *msg);

#endif /* MENU_INTERNAL_H */
