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
 * Whatever one file owns stays static inside it, so this is the whole of what
 * the pieces say to each other. The atc_menu_ prefix is left to the API.
 */
#ifndef MENU_INTERNAL_H
#define MENU_INTERNAL_H

#include "atc_menu/menu.h"

/*--- Layout -----------------------------------------------------------------*/

#define NUMW 7u  /* "   1   " */
#define VALW 12u /* widest formatted value: -214748.3648 */

/* Most the chrome can spend: banner (2) + rule + breadcrumb + closing rule +
   footer + message + prompt. The real cost is 6, 7 or 8 and measure_head() has
   it; only the floor atc_menu_init() enforces needs the worst case. */
#define CHROME_ROWS 8u

/*---------------------------------------------------------------------------
 * The caller's buffer, and where it divides
 *
 *   [ one row ][ editor title \0 ][ keystrokes \0 ]
 *              ^ edit_area        ^ edit_head
 *
 * Nothing but arithmetic on cols, buf_cap and edit_head: menu_draw.c wants the
 * first part and menu_edit.c the rest.
 *-------------------------------------------------------------------------*/

static inline size_t row_cap(const atc_menu_ctx_t *c)
{
    return ATC_MENU_ROW_BYTES(c->cols);
}

/* atc_menu_init() has already refused less than ATC_MENU_BUF_BYTES(cols). */
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

/* the title is fixed once the editor opens, so the rest is typing room */
static inline unsigned text_room(const atc_menu_ctx_t *c)
{
    return (unsigned)(edit_room(c) - c->edit_head - 1u);
}

/*--- Context flags ----------------------------------------------------------*/

#define F_EDIT    0x0001u
#define F_COMMIT  0x0002u
#define F_NEG     0x0004u
#define F_SAVED   0x0008u
#define F_DISABLE 0x0010u
#define F_STOP    0x0020u
#define F_OVF     0x0040u
#define F_TEXT    0x0080u
/* A value left this frame and is unanswered; the editor state is still whole,
   so atc_menu_reject() can put it back. */
#define F_DELIVERED 0x0100u
#define F_CHOICE    0x0200u
/* atc_menu_static_labels(): a label's address is as good as its text */
#define F_LABEL_PTR 0x0400u
/* atc_menu_fast_fill(): the terminal repeats a character on demand */
#define F_FAST_FILL 0x0800u

/* Which editor F_EDIT means. They outlive it: a delivered value keeps its kind
   until frame_end retires it. */
#define F_EDIT_KIND (F_TEXT | F_CHOICE)

/* edit_frac when no decimal point has been typed. */
#define NO_FRAC 0xFFu

/* Not 0: an item scrolled off the page reports 0 too, and would answer this
   editor's commit by writing the typed value into the application. */
#define EDIT_PAGE 0xFFu

/*--- menu_buf.c — a line, built straight into the caller's buffer -----------*/

/* len keeps counting past cap, so an overflow is detectable without ever
   writing out of bounds. */
typedef struct {
    char  *p;
    size_t cap;
    size_t len;
    size_t body; /* offset where the content after the ANSI prefix starts */
    size_t sig;  /* offset the signature starts at: past the cursor address */
    size_t vis;  /* columns written; escape sequences do not count */
    bool   solid; /* the row wears an attribute that shows in an empty cell */
} buf_t;

/* every byte of every row goes through here, so the call would cost more */
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

/* Magnitude and sign travel separately all the way to bnum, so the most
   negative int32_t does not overflow on the way. */
static inline uint32_t magnitude(int32_t v)
{
    return (v < 0) ? (uint32_t)0u - (uint32_t)v : (uint32_t)v;
}

/*---------------------------------------------------------------------------
 * menu_draw.c — where a row goes and what it looks like
 *
 * Item rows are addressed by declaration index, not by screen row: the page
 * offset and the chrome above it are the drawing side's business.
 *-------------------------------------------------------------------------*/

/* One declared row. Passed rather than eight loose arguments, half of which a
   16-bit MCU would stack twice: once to sign the row, once to paint it. */
typedef struct {
    unsigned char item_i;
    unsigned char num;   /* 0 when off the page, or when the row takes no number */
    unsigned char style;
    bool          dim;
    bool          page;  /* on the page, so the row is worth signing */
    bool          draw;  /* it moved, so it has to be built */
    uint16_t      key;
} slot_t;

unsigned page_items_max(const atc_menu_ctx_t *c);
void measure_head(atc_menu_ctx_t *c);

/* The whole of the signature: three instructions on a 16-bit MCU, against the
   thirteen Fletcher's two mod-255 folds cost. */
static inline uint16_t sig_mix(uint16_t sig, uint16_t v)
{
    return (uint16_t)((uint16_t)((sig << 1) | (sig >> 15)) + v);
}

uint16_t sig_text(const char *s, size_t max);
bool row_sign(const atc_menu_ctx_t *c, slot_t *s, const char *label,
              uint16_t vkey);
void row_item(atc_menu_ctx_t *c, const slot_t *s, const char *label,
              const char *value);
void row_separator(atc_menu_ctx_t *c, unsigned item_i);
uint16_t chrome_key(const atc_menu_ctx_t *c);
void paint_chrome(atc_menu_ctx_t *c);
void blank_tail(atc_menu_ctx_t *c);
void restore_cursor(atc_menu_ctx_t *c);

/*--- menu_edit.c — the editor's state and its slice of the buffer ----------*/

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
