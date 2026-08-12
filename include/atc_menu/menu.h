/* SPDX-License-Identifier: MIT */
/**
 * @file menu.h
 * @brief Immediate-mode terminal menu, number-driven, transport-agnostic
 *
 * Open a frame, declare the items, close it. Nothing is retained between frames
 * but the context, so every string need only stay valid for the call that used
 * it. Only changed rows are sent, so an idle frame costs nothing on the wire.
 *
 * @code
 * ATC_MENU_SCREEN(vt100, 80, 24);
 * static atc_menu_ctx_t menu;
 *
 * atc_menu_init(&menu, &info, &vt100, uart_sink, NULL);
 * atc_menu_term_begin(&menu);
 * for (;;) {
 *     atc_menu_key(&menu, uart_getbyte());   // -1 when nothing arrived
 *     atc_menu_frame_begin(&menu);
 *     atc_menu_uint16(&menu, "PWM (Hz)", &pwm_hz);
 *     if (atc_menu_action(&menu, "Save"))
 *         settings_store();
 *     atc_menu_frame_end(&menu);
 * }
 * @endcode
 */
#ifndef ATC_MENU_MENU_H
#define ATC_MENU_MENU_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------
 * Types and limits
 *-------------------------------------------------------------------------*/

/**
 * @brief Smallest addressable unsigned type
 *
 * Not uint8_t: a C2000 has CHAR_BIT == 16 and no such type. Everywhere else the
 * two are the same type, so a uint8_t variable still passes.
 */
typedef unsigned char atc_menu_u8;

/**
 * @brief Submenu nesting limit
 *
 * Sizes four arrays in atc_menu_ctx_t, so the library and its caller must be
 * compiled with the same value.
 */
#ifndef ATC_MENU_MAX_DEPTH
#define ATC_MENU_MAX_DEPTH 6u
#endif

/** @brief What an entry point returns, and what a frame records */
typedef enum {
    ATC_MENU_OK        = 0,  /**< nothing went wrong */
    ATC_MENU_ERR_PARAM = -1, /**< a NULL pointer, or an impossible screen */
    ATC_MENU_ERR_STATE = -2, /**< the menu was declared wrongly this frame */
    ATC_MENU_ERR_IO    = -3  /**< the sink refused bytes */
} atc_menu_status_t;

/**
 * @brief Write one row of bytes to the terminal — all of it, or none of it
 *
 * Called once per row, so the port must take ATC_MENU_ROW_BYTES(cols) bytes at
 * a time. A C2000 port must mask each byte with `& 0xFF`.
 *
 * @param buf  the bytes; not NUL-terminated, not valid after the call
 * @param len  how many, always at least 1
 * @param user whatever was handed to atc_menu_init()
 * @retval 1 all @p len bytes taken
 * @retval 0 none taken; the row is rebuilt and retried next frame
 */
typedef int (*atc_menu_sink_t)(const char *buf, size_t len, void *user);

/**
 * @brief Banner above the menu; a NULL field costs no row
 *
 * None of the strings is copied.
 */
typedef struct {
    const char *name;    /**< left of the first line */
    const char *version; /**< right of the first line */
    const char *owner;   /**< the second line */
} atc_menu_info_t;

/*---------------------------------------------------------------------------
 * The buffers the caller owns — the library allocates nothing
 *-------------------------------------------------------------------------*/

/**
 * @brief Bytes one painted row can take
 *
 * ESC[255;1H(8) + ESC[0m(4) + ESC[48;5;236m(11) + ESC[1;33m(7)
 * + ESC[22;39m(8) + ESC[1;3;4;7;31m(13) + cols + ESC[22;31m(8) + ESC[K(3)
 * + ESC[0m(4) + NUL(1).
 *
 * @param cols terminal width
 */
#define ATC_MENU_ROW_BYTES(cols) ((size_t)(cols) + 70u)

/**
 * @brief Bytes the buffer needs: one row, then the editor's scratch
 *
 *     [ one row ][ editor title \0 ][ keystrokes \0 ]
 *
 * The row is rebuilt every frame; the tail is not, since the title and the
 * keystrokes under it must outlive the widget call that opened the editor.
 * Nothing here is configured — a bigger buffer is simply a longer edit line.
 *
 * @param cols terminal width
 */
#define ATC_MENU_BUF_BYTES(cols) (ATC_MENU_ROW_BYTES(cols) + 48u)

/**
 * @brief The screen a context paints on: two buffers and their sizes
 *
 * Declare it with ATC_MENU_SCREEN, which writes each number once — `rows`
 * disagreeing with the length of `row_sig` is the one error the library cannot
 * catch. atc_menu_init() copies it, so it may be a temporary.
 */
typedef struct {
    char         *buf;     /**< one row, then the editor's scratch */
    size_t        buf_cap; /**< at least ATC_MENU_BUF_BYTES(cols) */
    uint16_t     *row_sig; /**< the line cache: exactly `rows` entries */
    unsigned char cols, rows; /**< the terminal, not the menu: 23.. and 9.. */
} atc_menu_screen_t;

/**
 * @brief Declare a cols×rows screen and its descriptor, at file scope
 *
 * @param name identifier for the resulting `static const atc_menu_screen_t`;
 *             also takes `<name>_buf` and `<name>_sig`
 * @param cols terminal width, 23 or more
 * @param rows terminal height, 9 or more
 *
 * @code
 * ATC_MENU_SCREEN(vt100, 80, 24);
 * atc_menu_init(&ctx, &info, &vt100, uart_sink, NULL);
 * @endcode
 */
#define ATC_MENU_SCREEN(name, cols, rows)                    \
    static char     name##_buf[ATC_MENU_BUF_BYTES(cols)];    \
    static uint16_t name##_sig[rows];                        \
    static const atc_menu_screen_t name = {                  \
        name##_buf, sizeof name##_buf, name##_sig, (cols), (rows) }

/*---------------------------------------------------------------------------
 * The context
 *-------------------------------------------------------------------------*/

/**
 * @brief One menu's whole state
 *
 * Declare one, hand it to atc_menu_init(), then only ever pass its address.
 * Every field belongs to the library.
 */
typedef struct {
    atc_menu_sink_t sink;
    void           *user;
    char           *buf;
    uint16_t       *row_sig;
    size_t          buf_cap;

    /* private below — do not touch */
    const atc_menu_info_t *info;
    const char   *crumb[ATC_MENU_MAX_DEPTH];
    const char   *msg;
    uint32_t      acc;
    /* rows and cols are the screen; the item counts are menu content, and a
       level may declare far more items than the screen has rows. */
    unsigned char rows, cols, page_items, shown_items;
    unsigned char path[ATC_MENU_MAX_DEPTH];
    unsigned char top[ATC_MENU_MAX_DEPTH];
    unsigned char item[ATC_MENU_MAX_DEPTH];
    unsigned char nav_depth, decl_depth, pending, act, enter_req;
    /* level_items: declared this frame; level_numbered: how many got a number,
       which is fewer whenever a label, a rule or another page is in the way. */
    unsigned char level_items, level_numbered;
    /* edit_head: where the keystrokes start in the editor's slice of `buf`;
       edit_vpos: where the title's bracketed value starts, so the prompt can
       colour it like the column it came from. */
    unsigned char edit_item, edit_base, edit_dec, edit_frac, edit_len;
    unsigned char edit_head, edit_vpos;
    uint16_t      chrome_sig; /* what the chrome was last painted from */
    uint16_t      flags;
    signed char   status;
    /* atc_menu_item_style()'s, for the next item. Last on purpose: here it
       lands in padding the struct already had. */
    unsigned char item_style;
} atc_menu_ctx_t;

/* Measured: 96 bytes with 32-bit pointers, 152 on a 64-bit host. The editor
   scratch lives in the caller's buffer, not here, which is what keeps this
   below what it cost when it held one. Gated on an 8-bit byte, since a C2000
   counts this struct in 16-bit words and the same budget would be a different
   number there. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && CHAR_BIT == 8
_Static_assert(sizeof(atc_menu_ctx_t) <= 160u, "atc_menu_ctx_t grew past its budget");
#endif

/*---------------------------------------------------------------------------
 * Setting up, and the terminal
 *-------------------------------------------------------------------------*/

/**
 * @brief Bind a context to its screen and its sink
 *
 * `rows` is the terminal, not the menu: 6 to 8 go to the chrome — banner, rule,
 * breadcrumb, closing rule, footer, message line, prompt. The rest is paged, so
 * a level may declare far more items than fit.
 *
 * @param c      context to initialise; zeroed by the call
 * @param info   banner; may be NULL, pointed at rather than copied
 * @param screen buffers and sizes; copied, so it may be a temporary
 * @param sink   called once per changed row
 * @param user   passed to @p sink untouched
 * @retval ATC_MENU_OK        ready to use
 * @retval ATC_MENU_ERR_PARAM a NULL pointer, `cols` under 23, `rows` under 9,
 *                            or `buf_cap` under ATC_MENU_BUF_BYTES(cols)
 */
atc_menu_status_t atc_menu_init(atc_menu_ctx_t *c, const atc_menu_info_t *info,
                                const atc_menu_screen_t *screen,
                                atc_menu_sink_t sink, void *user);

/**
 * @brief Take the terminal: clear it, hide the cursor, repaint from scratch
 *
 * Placement and erasing are VT100; hiding the cursor is ESC[?25l, which is
 * VT220 and up. No alternate screen, no scrolling region.
 *
 * @param c the context
 * @retval ATC_MENU_OK        the terminal is the menu's
 * @retval ATC_MENU_ERR_PARAM @p c is NULL or was never initialised
 * @retval ATC_MENU_ERR_IO    the sink refused the sequence
 */
atc_menu_status_t atc_menu_term_begin(atc_menu_ctx_t *c);

/**
 * @brief Give it back: show the cursor and park it below the menu
 *
 * The screen is not cleared.
 *
 * @param c the context
 * @retval ATC_MENU_OK        the cursor is back
 * @retval ATC_MENU_ERR_PARAM @p c is NULL or was never initialised
 * @retval ATC_MENU_ERR_IO    the sink refused the sequence
 */
atc_menu_status_t atc_menu_term_end(atc_menu_ctx_t *c);

/*---------------------------------------------------------------------------
 * Between frames
 *-------------------------------------------------------------------------*/

/**
 * @brief Feed one received byte
 *
 * Only moves state; the next frame is what shows the result.
 *
 * No editor open: `1`-`9` pick an item and Enter confirms a multi-digit one,
 * `0` and Esc go back a level, Backspace drops a half-typed number, `n` and `p`
 * page, `r` repaints, `i` sets the page size.
 *
 * Editor open: digits, and `a`-`f` on a hex row; `-` for the sign, `.` for the
 * fraction; Backspace deletes, Enter commits, Esc abandons. A choice steps
 * forward on any printable key, back on Backspace.
 *
 * @param c    the context
 * @param byte 0..255; a negative value is ignored, so the -1 of a non-blocking
 *             read can be passed straight in
 */
void atc_menu_key(atc_menu_ctx_t *c, int byte);

/**
 * @brief Repaint everything on the next frame — what the `r` key does
 * @param c the context; NULL is ignored
 */
void atc_menu_refresh(atc_menu_ctx_t *c);

/**
 * @brief Return to the root level
 *
 * Drops the page offsets, a half-typed number and any open editor. Paints
 * nothing.
 *
 * @param c the context; NULL is ignored
 */
void atc_menu_reset(atc_menu_ctx_t *c);

/**
 * @brief Set how many items one page shows — what the `i` key does
 *
 * Everything the frame declared counts, labels and separators included. Takes
 * effect next frame, with every level back on its first page.
 *
 * @param c the context; NULL is ignored
 * @param n items per page; never refused — 0 becomes 1, too large is clamped
 */
void atc_menu_set_items_per_page(atc_menu_ctx_t *c, unsigned n);

/**
 * @brief Items one page shows now
 * @param c the context
 * @return the page size, or 0 if @p c is NULL
 */
unsigned atc_menu_items_per_page(const atc_menu_ctx_t *c);

/**
 * @brief Show text on the message line until the next key
 *
 * The same line the library uses for "out of range" and the like.
 *
 * @param c   the context; NULL is ignored
 * @param msg not copied, so it must stay valid while displayed; NULL clears it
 */
void atc_menu_message(atc_menu_ctx_t *c, const char *msg);

/**
 * @brief True while a value is being typed
 * @param c the context
 * @return true if an editor is open; false if none is, or @p c is NULL
 */
bool atc_menu_editing(const atc_menu_ctx_t *c);

/**
 * @brief Refuse a value a widget just returned, and reopen its editor
 *
 * Call it on the same frame the widget returned true; the keystrokes come back
 * intact. The widget has already written through your pointer by then, so keep
 * a local and copy it out only once it passes:
 *
 * @code
 * uint16_t hz = pwm_hz;
 * if (atc_menu_uint16(c, "PWM (Hz)", &hz)) {
 *     if (hz < 100u) atc_menu_reject(c, "too low");
 *     else           pwm_hz = hz;
 * }
 * @endcode
 *
 * @param c   the context
 * @param msg why; not copied. NULL reopens the editor with no message.
 * @note Ignored unless a widget delivered a value earlier in this same frame.
 *       atc_menu_bool() never delivers one — it writes as it toggles — so this
 *       does nothing after a bool, quietly.
 */
void atc_menu_reject(atc_menu_ctx_t *c, const char *msg);

/*---------------------------------------------------------------------------
 * A frame
 *-------------------------------------------------------------------------*/

/**
 * @brief Open a frame; every widget call must follow it
 * @param c the context
 * @retval ATC_MENU_OK        declare away
 * @retval ATC_MENU_ERR_PARAM @p c is NULL or was never initialised
 */
atc_menu_status_t atc_menu_frame_begin(atc_menu_ctx_t *c);

/**
 * @brief Close a frame: paint the chrome, blank orphan rows, enter a submenu
 *
 * Everything deferred lands here, now that the level's height is known — a
 * delivered value included, so this is where atc_menu_reject() stops working.
 *
 * @param c the context
 * @retval ATC_MENU_OK        the frame is on screen
 * @retval ATC_MENU_ERR_PARAM @p c is NULL or was never initialised
 * @retval ATC_MENU_ERR_STATE a submenu was left open, a level declared over 254
 *                            items, or nesting passed ATC_MENU_MAX_DEPTH
 * @retval ATC_MENU_ERR_IO    the sink refused a row; it is retried next frame
 */
atc_menu_status_t atc_menu_frame_end(atc_menu_ctx_t *c);

/*---------------------------------------------------------------------------
 * Decoration — a row, but no number
 *-------------------------------------------------------------------------*/

/**
 * @brief Declare a group heading
 * @param c    the context, mid-frame
 * @param text not copied, and truncated to the label column
 */
void atc_menu_label(atc_menu_ctx_t *c, const char *text);

/**
 * @brief Declare a horizontal rule between items
 * @param c the context, mid-frame
 */
void atc_menu_separator(atc_menu_ctx_t *c);

/**
 * @brief Draw the next item dim and refuse to select it
 *
 * Applies to the one item after it. Picking a dim row says "not available now".
 * A separator is that item if one comes first: it spends the flag without
 * showing anything, so put the call after the rule rather than before it.
 *
 * @param c       the context, mid-frame
 * @param enabled false to dim the next item, true to clear the flag again
 */
void atc_menu_item_enable(atc_menu_ctx_t *c, bool enabled);

/**
 * @name Style bits
 *
 * ORed together for atc_menu_item_style(). The colour is a packed field rather
 * than a bit each, so name at most one: ORing two gives a third, unrelated
 * colour — ATC_MENU_FG_RED | ATC_MENU_FG_YELLOW is magenta. A terminal without
 * an attribute drops it. Italic is not VT100 — the Linux console ignores it.
 * @{
 */
#define ATC_MENU_BOLD      0x01u /**< SGR 1 */
#define ATC_MENU_ITALIC    0x02u /**< SGR 3; not VT100 */
#define ATC_MENU_UNDERLINE 0x04u /**< SGR 4 */
#define ATC_MENU_REVERSE   0x08u /**< SGR 7 */

#define ATC_MENU_FG_BLACK   0x10u
#define ATC_MENU_FG_RED     0x20u
#define ATC_MENU_FG_GREEN   0x30u
#define ATC_MENU_FG_YELLOW  0x40u
#define ATC_MENU_FG_BLUE    0x50u
#define ATC_MENU_FG_MAGENTA 0x60u
#define ATC_MENU_FG_CYAN    0x70u
#define ATC_MENU_FG_WHITE   0x80u
/** @} */

/**
 * @brief Style the next item's label and value
 *
 * Applies to the one item after it and is spent there — including an item that
 * is scrolled off the page, so nothing leaks onto the next row. The number
 * column keeps its own colour, since it is what the user types; a colour here
 * replaces the green of the value column. A dim item ignores the style, so "not
 * available now" outranks decoration, and a separator has no content to wear
 * it.
 *
 * @code
 * atc_menu_item_style(c, ATC_MENU_BOLD | ATC_MENU_FG_RED);
 * atc_menu_text_ro(c, "State", "FAULT");
 * @endcode
 *
 * @param c     the context, mid-frame; NULL is ignored
 * @param flags ATC_MENU_BOLD and friends, ORed; 0 clears what was set
 */
void atc_menu_item_style(atc_menu_ctx_t *c, unsigned flags);

/*---------------------------------------------------------------------------
 * Read-only rows
 *-------------------------------------------------------------------------*/

/**
 * @name Read-only values
 *
 * A numbered row that cannot be selected; picking one says "read-only". The
 * `hex` forms print `0x` and a fixed width, `bool_ro` draws `[X]` or `[ ]`.
 *
 * @param c     the context, mid-frame
 * @param label not copied, and truncated to the label column
 * @param v     the value to show (`text` for atc_menu_text_ro)
 * @{
 */
void atc_menu_uint8_ro (atc_menu_ctx_t *c, const char *label, atc_menu_u8 v);
void atc_menu_uint16_ro(atc_menu_ctx_t *c, const char *label, uint16_t v);
void atc_menu_uint32_ro(atc_menu_ctx_t *c, const char *label, uint32_t v);
void atc_menu_int16_ro (atc_menu_ctx_t *c, const char *label, int16_t  v);
void atc_menu_int32_ro (atc_menu_ctx_t *c, const char *label, int32_t  v);
void atc_menu_hex8_ro  (atc_menu_ctx_t *c, const char *label, atc_menu_u8 v);
void atc_menu_hex16_ro (atc_menu_ctx_t *c, const char *label, uint16_t v);
void atc_menu_hex32_ro (atc_menu_ctx_t *c, const char *label, uint32_t v);
void atc_menu_bool_ro  (atc_menu_ctx_t *c, const char *label, bool     v);
void atc_menu_text_ro  (atc_menu_ctx_t *c, const char *label, const char *text);
/** @} */

/**
 * @brief Read-only fixed-point value
 *
 * A plain scaled integer, no float: 12345 at 3 decimals reads `12.345`.
 *
 * @param c        the context, mid-frame
 * @param label    not copied
 * @param v        the scaled integer
 * @param decimals 0..4; more is clamped to 4
 */
void atc_menu_fix_ro(atc_menu_ctx_t *c, const char *label, int32_t v,
                     unsigned decimals);

/*---------------------------------------------------------------------------
 * Editable rows
 *-------------------------------------------------------------------------*/

/**
 * @name Editable values
 *
 * Picking one opens a prompt — "PWM (Hz) [1000]> " — and Enter is what writes.
 * Out of the row's range is refused with "out of range", the editor staying
 * open over what was typed.
 *
 * atc_menu_bool is the exception: it has no editor and no prompt, so the
 * keystroke that picks the row is also the one that writes. Nothing stands
 * between the two, which means atc_menu_reject() has nothing to reopen and
 * cannot take the value back — put a bool behind atc_menu_item_enable() if it
 * needs a guard, or use atc_menu_choice(), which shows a candidate first for
 * exactly this reason.
 *
 * @param c     the context, mid-frame
 * @param label not copied; also the prompt title
 * @param v     read to draw the row, written only when this returns true
 * @return true on the one frame the value was written
 * @{
 */
bool atc_menu_uint8 (atc_menu_ctx_t *c, const char *label, atc_menu_u8 *v);
bool atc_menu_uint16(atc_menu_ctx_t *c, const char *label, uint16_t *v);
bool atc_menu_uint32(atc_menu_ctx_t *c, const char *label, uint32_t *v);
bool atc_menu_int16 (atc_menu_ctx_t *c, const char *label, int16_t  *v);
bool atc_menu_int32 (atc_menu_ctx_t *c, const char *label, int32_t  *v);
bool atc_menu_hex8  (atc_menu_ctx_t *c, const char *label, atc_menu_u8 *v);
bool atc_menu_hex16 (atc_menu_ctx_t *c, const char *label, uint16_t *v);
bool atc_menu_hex32 (atc_menu_ctx_t *c, const char *label, uint32_t *v);
bool atc_menu_bool  (atc_menu_ctx_t *c, const char *label, bool     *v);
/** @} */

/**
 * @brief Editable fixed-point value
 *
 * A plain scaled integer, no float: 12.345 at 3 decimals is 12345. Typing fewer
 * fraction digits scales up, so `12.3` also gives 12300.
 *
 * @param c        the context, mid-frame
 * @param label    not copied
 * @param v        the scaled integer, written only when this returns true
 * @param decimals 0..4; more is clamped to 4
 * @return true on the one frame @p v was written
 */
bool atc_menu_fix(atc_menu_ctx_t *c, const char *label, int32_t *v,
                  unsigned decimals);

/**
 * @brief Editable text; @p buf changes only on Enter
 *
 * The keystrokes live in the library's scratch until then, so an abandoned edit
 * leaves @p buf untouched. Printable ASCII only.
 *
 * @param c     the context, mid-frame
 * @param label not copied
 * @param buf   NUL-terminated; written only when this returns true
 * @param cap   size of @p buf including the terminator; longer input is refused
 *              with "too long"
 * @return true on the one frame @p buf was written
 */
bool atc_menu_text(atc_menu_ctx_t *c, const char *label, char *buf, size_t cap);

/**
 * @brief Pick from a list; the row opens a preview rather than writing
 *
 * The prompt shows the candidate — "Sys Mode [SLEEP]> " — and any printable key
 * steps forward, Backspace back, Enter commits, Esc cancels. The row keeps
 * showing the committed choice, so no one keystroke changes a live setting.
 *
 * @param c       the context, mid-frame
 * @param label   not copied
 * @param index   0-based, written only when this returns true; a value at or
 *                past @p count is reset to 0
 * @param choices @p count strings; nothing is copied
 * @param count   how many options; 0 does nothing and returns false
 * @return true on the one frame @p index was written
 */
bool atc_menu_choice(atc_menu_ctx_t *c, const char *label, unsigned *index,
                     const char *const *choices, unsigned count);

/**
 * @brief A row that does something when picked
 * @param c     the context, mid-frame
 * @param label not copied
 * @return true on the one frame this item was selected
 */
bool atc_menu_action(atc_menu_ctx_t *c, const char *label);

/*---------------------------------------------------------------------------
 * Levels
 *-------------------------------------------------------------------------*/

/**
 * @brief Open a nested level
 *
 * Declared like an `if`. True means this level is on screen: declare its body,
 * then atc_menu_submenu_end(). False means the row is just an item on the level
 * above, and the body is skipped — one pass per frame, whatever the depth.
 *
 * @code
 * if (atc_menu_submenu(c, "Diagnostics")) {
 *     atc_menu_int16_ro(c, "Board temp", temp_c);
 *     atc_menu_submenu_end(c);
 * }
 * @endcode
 *
 * @param c     the context, mid-frame
 * @param label the row, and the breadcrumb once inside; not copied
 * @return true while this level is open
 * @note Selecting the row descends on the next frame.
 */
bool atc_menu_submenu(atc_menu_ctx_t *c, const char *label);

/**
 * @brief Close the body opened by a submenu that returned true
 * @param c the context, mid-frame
 */
void atc_menu_submenu_end(atc_menu_ctx_t *c);

#ifdef __cplusplus
}
#endif
#endif /* ATC_MENU_MENU_H */
