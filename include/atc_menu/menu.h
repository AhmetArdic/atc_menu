/**
 * @file menu.h
 * @brief ATC Menu - lightweight C library that draws line-based, colored menus
 *        over UART to serial terminals (Tera Term, PuTTY, minicom).
 *
 * The only header an application includes; it pulls in the ATC_MENU_* tree
 * macros (@ref menu_def.h) too.
 *
 * The tree is `const` data at file scope (ROM). Every received UART byte goes
 * to atc_menu_key() from the main loop, which acts on it at once - selections
 * need no Enter. atc_menu_update() is called periodically and does all the
 * drawing, line by line, into the sink given to atc_menu_init().
 *
 * Pure C99, never touches hardware. Needs a VT100/ANSI terminal; labels and
 * strings must be ASCII (alignment is by byte count).
 *
 * @author Ahmet Talha ARDIC
 * @date   2026-07-31
 */
#ifndef ATC_MENU_MENU_H
#define ATC_MENU_MENU_H

#include <stddef.h>
#include <stdint.h>

#include "atc_menu/menu_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Smallest integer type: C2000 (CHAR_BIT == 16) has no uint8_t. */
typedef unsigned char atc_menu_u8;

/* Hard limits, not options: both follow from indexing with atc_menu_u8, whose
 * 0xFF is reserved as a "nothing pending" sentinel. */
#define ATC_MENU_ITEMS_PER_PAGE_MAX 248 /**< Largest window the 'i' command accepts. */
#define ATC_MENU_ITEMS_MAX          254 /**< Largest page ATC_MENU_PAGE_END() takes. */

/* Derived from the two width knobs in menu_config.h, not options themselves.
 * LINE_BUF holds one full row plus its color escapes; the footer's five
 * colored hints are the worst case. */
#define ATC_MENU_ROW_W    (ATC_MENU_CFG_LABEL + ATC_MENU_CFG_VAL + 10)
#define ATC_MENU_LINE_BUF (ATC_MENU_ROW_W + 88)

/**
 * @brief Output callback - the library writes bytes to be drawn here.
 *
 * All-or-nothing: accept all `len` bytes and return 1, or take none and return
 * 0; a rejected line is rebuilt and retried on the next atc_menu_update().
 * Called once per line, so the port buffer must be able to free
 * ATC_MENU_LINE_BUF bytes. A C2000 port must mask every byte with `& 0xFF`.
 *
 * @param user user pointer given to atc_menu_init()
 * @param buf  bytes to write
 * @param len  byte count
 * @return 1 if all bytes were accepted, 0 otherwise
 */
typedef int (*atc_menu_sink_fn)(void *user, const char *buf, size_t len);

/**
 * @brief Application info shown at the top of every page.
 *
 * Lines of NULL fields are not printed at all; the whole struct may be NULL.
 */
typedef struct {
    const char *name;    /**< Application name, e.g. "Power Analyzer". */
    const char *version; /**< Version string, e.g. "v1.4.2". */
    const char *author;  /**< Author(s), e.g. "ATC / A. Ardic". */
} atc_menu_info_t;

/** @brief Return status of atc_menu_update(). */
typedef enum {
    ATC_MENU_IDLE = 0, /**< Nothing pending; if the RX queue is empty too, LPM may be entered. */
    ATC_MENU_BUSY = 1  /**< Output pending (e.g. sink rejected) or input not fully processed. */
} atc_menu_status_t;

/** @brief Item kinds. Not used directly by the application; macros produce them. */
typedef enum {
    ATC_MENU_KIND_LABEL = 0,
    ATC_MENU_KIND_SEPARATOR,
    ATC_MENU_KIND_TEXT,
    ATC_MENU_KIND_READOUT,
    ATC_MENU_KIND_CHECKBOX,
    ATC_MENU_KIND_NUMBER,
    ATC_MENU_KIND_HEX,
    ATC_MENU_KIND_CHOICE,
    ATC_MENU_KIND_ACTION,
    ATC_MENU_KIND_CUSTOM,
    ATC_MENU_KIND_SUBMENU
} atc_menu_kind_t;

/** @brief How a READOUT value is shown. Stored in atc_menu_item_t::flags. */
typedef enum {
    ATC_MENU_DEC = 0, /**< decimal, signed: -17          */
    ATC_MENU_HEX8,    /**< 0x2A                          */
    ATC_MENU_HEX16,   /**< 0x1A2B                        */
    ATC_MENU_HEX32,   /**< 0xFFFFFFFF                    */
    ATC_MENU_FIX1,    /**< one decimal,   253  -> 25.3   */
    ATC_MENU_FIX2,    /**< two decimals,  3300 -> 33.00  */
    ATC_MENU_FIX3,    /**< three decimals, 1234 -> 1.234 */
    /** OR into DEC or a FIX to read the bits unsigned: -16 -> 4294967280. */
    ATC_MENU_UNSIGNED = 0x80
} atc_menu_fmt_t;

/*
 * `arg` is an application-defined selector - a channel, a bit index, a
 * register id - deliberately not a pointer, which an int32_t cannot portably
 * hold. To reach a structure, index a ROM table: `sensor_read(&sensors[arg])`.
 *
 * int32_t is the container, not the type behind it: a uint16_t register, a
 * uint8_t flag, a bool, an enum all fit, with the cast in the accessor.
 * uint32_t fits too - the conversion is modular, so the bits survive and
 * ATC_MENU_UNSIGNED reads them back the same way.
 *
 * Getters run while a row is rendered: cheap, non-blocking, side-effect free.
 * Refusing setters keep the editor open; the menu guards only parsing, so the
 * range is the setter's to enforce.
 */

/** @brief Reads a row's value. NULL reads as 0. */
typedef int32_t (*atc_menu_get_fn)(int32_t arg);

/** @brief Writes a row's value; returns 0 to refuse. NULL is read-only. */
typedef int (*atc_menu_set_fn)(int32_t arg, int32_t value);

/** @brief ACTION handler; `arg` distinguishes items sharing one handler. */
typedef void (*atc_menu_action_fn)(int32_t arg);

/**
 * @brief CUSTOM renderer - writes the value column itself, under a getter's
 *        rules. Write at most `cap` bytes; anything longer is truncated.
 * @param arg  selector, as for a getter
 * @param out  value column scratch, no terminator
 * @param cap  bytes available
 * @return bytes written
 */
typedef unsigned (*atc_menu_show_fn)(int32_t arg, char *out, unsigned cap);

/**
 * @brief CUSTOM parser - takes what was typed, in whatever syntax the row uses.
 * @param arg  selector, as for a setter
 * @param text NUL-terminated entered text
 * @return 0 = reject (error message printed), nonzero = accept
 */
typedef int (*atc_menu_edit_fn)(int32_t arg, const char *text);

/** @brief How a value tool reaches its value. NULL `get` reads as 0, NULL
 *         `set` makes the row read-only. */
typedef struct {
    atc_menu_get_fn get;
    atc_menu_set_fn set;
    int32_t arg;        /**< Passed to both; selects which value they mean. */
} atc_menu_bind_t;

/* Item detail structs - produced by the macros, live in ROM. Function pointers
 * always sit inside a struct, never in `detail` directly: converting between
 * function and object pointers is implementation-defined. READOUT and CHECKBOX
 * add nothing to the binding, so their detail is an atc_menu_bind_t itself. */

/** @brief NUMBER/FIXED detail. FIXED values are scaled by 10^decimals. */
typedef struct {
    atc_menu_bind_t bind;
    atc_menu_u8 decimals;       /**< 0 for NUMBER. */
} atc_menu_num_t;

/** @brief HEX detail - the bound value is shown as its raw bit pattern. */
typedef struct {
    atc_menu_bind_t bind;
    atc_menu_u8 bits;           /**< Significant bit count (4..32); shown as bits/4 digits. */
} atc_menu_hex_t;

/**
 * @brief CHOICE detail - selecting previews the next option; Enter commits, so
 *        the setter runs once rather than on every preview step.
 */
typedef struct {
    atc_menu_bind_t bind;       /**< The bound value is the selected index. */
    const char *const *items;
    atc_menu_u8 count;
} atc_menu_choice_t;

/**
 * @brief CUSTOM detail - the row the library does not interpret. NULL `show`
 *        displays "..." (which is what ATC_MENU_PROMPT is); NULL `edit` makes
 *        the row read-only.
 */
typedef struct {
    atc_menu_show_fn show;
    atc_menu_edit_fn edit;
    int32_t arg;
} atc_menu_custom_t;

/** @brief ACTION detail. */
typedef struct {
    atc_menu_action_fn on_click;
    int32_t arg;
} atc_menu_action_t;

/**
 * @brief A single menu item. Produced by the macros; type-specific fields are
 *        behind `detail`.
 */
typedef struct {
    const char *label;
    atc_menu_u8 kind;   /**< atc_menu_kind_t */
    atc_menu_u8 flags;  /**< atc_menu_fmt_t for READOUT, 0 otherwise. */
    const void *detail; /**< Per kind: atc_menu_num_t etc.; atc_menu_page_t* for SUBMENU. */
} atc_menu_item_t;

/**
 * @brief A menu page. Produced by ATC_MENU_PAGE_BEGIN/END.
 *
 * `count` is folded in by ATC_MENU_PAGE_END with sizeof, so there is no
 * terminator item and nothing is counted at run time.
 */
typedef struct {
    const char *title;
    const atc_menu_item_t *items;
    atc_menu_u8 count;
} atc_menu_page_t;

/**
 * @brief Menu context - all RAM state (about 205 bytes on a 16-bit target).
 *        Allocate statically.
 *
 * All fields are private; do not access them directly.
 */
typedef struct {
    const atc_menu_info_t *info;
    atc_menu_sink_fn sink;
    void *user;

    struct {
        const atc_menu_page_t *page;
        atc_menu_u8 start;              /* paging offset */
    } nav[ATC_MENU_CFG_MAX_DEPTH];
    atc_menu_u8 depth;
    atc_menu_u8 items_per_page;         /* items shown at once, 1..248 */

    char input[ATC_MENU_CFG_INPUT_BUF];
    atc_menu_u8 input_len;
    atc_menu_u8 esc_state;
    atc_menu_u8 last_cr;

    atc_menu_u8 mode;                   /* select / edit / choice / items-per-page */
    atc_menu_u8 edit_item;
    atc_menu_u8 edit_num;
    atc_menu_u8 choice_val;             /* previewed CHOICE index; *value
                                          * untouched until commit */
    atc_menu_u8 dirty_row;              /* item index, 0xFF = none */
    atc_menu_u8 draw_pos;               /* full-draw step, 0xFF = none */
    atc_menu_u8 prompt_pos;             /* prompt draw step, 0xFF = none */
    const char *msg;
    atc_menu_u8 msg_err;

    char line[ATC_MENU_LINE_BUF];
} atc_menu_ctx_t;

/**
 * @brief Sets up the context and queues the first full draw (painted by the
 *        first atc_menu_update()).
 *
 * @param ctx  statically allocated context
 * @param info application info header; may be NULL
 * @param root root page (menu tree in ROM)
 * @param sink output callback
 * @param user pointer passed through to the sink
 */
void atc_menu_init(atc_menu_ctx_t *ctx, const atc_menu_info_t *info,
                   const atc_menu_page_t *root,
                   atc_menu_sink_fn sink, void *user);

/**
 * @brief Processes one received byte.
 *
 * Navigation, editing and the item callbacks all run inside this call; only
 * drawing is deferred to atc_menu_update(). Must run in the same context as
 * atc_menu_update(), i.e. the main loop - do NOT call it from an ISR, or a menu
 * callback would execute at interrupt level. An interrupt-driven UART hands its
 * bytes over through the application's own RX buffer.
 */
void atc_menu_key(atc_menu_ctx_t *ctx, char byte);

/**
 * @brief Periodic work: draws whatever the last keys made pending.
 *
 * All sink calls happen in this context.
 *
 * @return ATC_MENU_IDLE if nothing is pending (LPM may be entered);
 *         ATC_MENU_BUSY if it should be called again soon.
 */
atc_menu_status_t atc_menu_update(atc_menu_ctx_t *ctx);

/**
 * @brief Requests a full redraw (e.g. after many values changed at once).
 *
 * The drawing happens on the next atc_menu_update() call.
 */
void atc_menu_refresh(atc_menu_ctx_t *ctx);

/**
 * @brief Shows a feedback message in the menu's message line.
 *
 * Typical use is inside a callback to explain a rejection ("Relay needs RUN
 * mode"); when a rejecting callback has set a message, it is shown instead of
 * the generic "Rejected". May also be called from anywhere in the application
 * (e.g. an action handler or the main loop) for status feedback.
 *
 * The string is not copied and must stay valid while shown (use a literal).
 * The message disappears on the next keypress; NULL clears it immediately.
 *
 * @param ctx   menu context
 * @param msg   ASCII message, or NULL to clear
 * @param error nonzero = error style (red), 0 = info style (gray)
 */
void atc_menu_message(atc_menu_ctx_t *ctx, const char *msg, int error);

#ifdef __cplusplus
}
#endif

/* Pulled in last so its macros see the types declared above; also makes
 * menu.h the single include an application needs. */
#include "atc_menu/menu_def.h"

#endif /* ATC_MENU_MENU_H */
