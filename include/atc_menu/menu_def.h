/**
 * @file menu_def.h
 * @brief ATC Menu toolbox macros - define the menu tree as `const` data at
 *        file scope.
 *
 * Included automatically by menu.h. All macros use C99 compound literals; at
 * file scope these have static storage duration and are placed in ROM.
 *
 * @code
 * ATC_MENU_CHOICES(mode_items, "SLOW", "NORMAL", "FAST");
 *
 * ATC_MENU_PAGE_BEGIN(settings_page, "Settings")
 *     ATC_MENU_READOUT ("Temp (C)", rd_adc, CH_TEMP, ATC_MENU_FIX1)
 *     ATC_MENU_CHECKBOX("LED", gpio_get, gpio_set, PIN_LED)
 *     ATC_MENU_NUMBER  ("Baud/100", baud_get, baud_set, 0)
 *     ATC_MENU_CHOICE  ("Mode", mode_get, mode_set, 0, mode_items)
 *     ATC_MENU_CUSTOM  ("Uptime", uptime_show, 0, 0)
 * ATC_MENU_PAGE_END(settings_page)
 * @endcode
 *
 * Every value tool binds the same way - `label, rd, wr, arg` - then whatever
 * that one tool needs on top (a format, a decimal count, an option list).
 * ATC_MENU_CUSTOM is the way out when a value is not an integer the library
 * can print.
 *
 * Every `label` is a string literal and must be ASCII. A page referenced by
 * SUBMENU must be defined earlier in the same file (subpages first).
 *
 * @author Ahmet Talha ARDIC
 * @date   2026-07-31
 */
#ifndef ATC_MENU_MENU_DEF_H
#define ATC_MENU_MENU_DEF_H

#include "atc_menu/menu.h"

/** @brief Element count of an array; the array must not have decayed. */
#define ATC_MENU_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

/** @brief Defines a CHOICE option list. */
#define ATC_MENU_CHOICES(name, ...) \
    static const char *const name[] = { __VA_ARGS__ }

/** @brief Starts a page; items go between this and PAGE_END. `title` must be a
 *         string literal (it is stored as an array, so the page below can be a
 *         constant initializer). */
#define ATC_MENU_PAGE_BEGIN(name, title) \
    static const char name##_title_[] = title; \
    static const atc_menu_item_t name##_items_[] = {

/**
 * @brief Ends a page; `name` must match the PAGE_BEGIN above.
 *
 * The name is what lets sizeof count the items here - an anonymous compound
 * literal could not be measured. Over ATC_MENU_ITEMS_MAX items is a compile
 * error, not a page that silently loses its tail.
 */
#define ATC_MENU_PAGE_END(name) \
        }; \
        typedef char atc_menu_page_too_big_##name \
            [ATC_MENU_COUNT_OF(name##_items_) <= ATC_MENU_ITEMS_MAX ? 1 : -1]; \
        static const atc_menu_page_t name = { \
            name##_title_, name##_items_, \
            (atc_menu_u8)ATC_MENU_COUNT_OF(name##_items_) };

/** @brief Non-selectable section heading. */
#define ATC_MENU_LABEL(label) \
        { label, ATC_MENU_KIND_LABEL, 0, 0 },

/** @brief Horizontal separator line. */
#define ATC_MENU_SEPARATOR() \
        { 0, ATC_MENU_KIND_SEPARATOR, 0, 0 },

/** @brief Non-selectable text; wraps across rows and keeps one zebra shade. */
#define ATC_MENU_TEXT(text) \
        { text, ATC_MENU_KIND_TEXT, 0, 0 },

/**
 * @name Accessors for a plain variable
 *
 * Write the pair for a value with nothing to validate. Unlike every other
 * ATC_MENU_ macro these produce function definitions, so they go at file scope,
 * outside PAGE_BEGIN/END:
 *
 * @code
 * ATC_MENU_DEFINE_ACCESSORS(freq, pwm_freq)   // freq_get + freq_set
 * ATC_MENU_DEFINE_GETTER(ticks, sys_ticks)    // ticks_get
 * @endcode
 *
 * `var` is any lvalue (`cfg.baud`, `regs[2]`), accessed with its own type, so a
 * value that will not fit is a compile error rather than a truncation.
 * @{
 */
#define ATC_MENU_DEFINE_GETTER(name, var) \
    static int32_t name##_get(int32_t arg) \
    { \
        (void)arg; \
        return (int32_t)(var); \
    }

#define ATC_MENU_DEFINE_ACCESSORS(name, var) \
    ATC_MENU_DEFINE_GETTER(name, var) \
    static int name##_set(int32_t arg, int32_t value) \
    { \
        (void)arg; \
        (var) = value; \
        return 1; \
    }
/** @} */

/**
 * @brief Read-only value; the one value tool that takes no setter.
 * @param rd    getter; NULL reads as 0
 * @param arg   passed to `rd`
 * @param fmt   an @ref atc_menu_fmt_t, optionally | ATC_MENU_UNSIGNED
 */
#define ATC_MENU_READOUT(label, rd, arg, fmt) \
        { label, ATC_MENU_KIND_READOUT, (atc_menu_u8)(fmt), \
          &(const atc_menu_bind_t){ rd, 0, (arg) } },

/**
 * @brief [X]/[ ] state; selecting toggles it.
 * @param rd    getter; nonzero shows [X]
 * @param wr    setter, called with 0 or 1; NULL makes the row read-only
 * @param arg   passed to both, typically the bit index
 */
#define ATC_MENU_CHECKBOX(label, rd, wr, arg) \
        { label, ATC_MENU_KIND_CHECKBOX, 0, \
          &(const atc_menu_bind_t){ rd, wr, (arg) } },

/**
 * @brief Integer input; the editor guards only what an int32_t can hold, so the
 *        range is `wr`'s to enforce.
 * @param rd    getter
 * @param wr    setter; return 0 to refuse
 * @param arg   passed to both
 */
#define ATC_MENU_NUMBER(label, rd, wr, arg) \
        { label, ATC_MENU_KIND_NUMBER, 0, \
          &(const atc_menu_num_t){ { rd, wr, (arg) }, 0 } },

/**
 * @brief Fixed-point input; the bound value is scaled by 10^decimals.
 * @param rd       getter, returning the scaled value (3300 -> "33.00")
 * @param wr       setter, receiving the scaled value
 * @param arg      passed to both
 * @param decimals digits after the point, 1..3
 */
#define ATC_MENU_FIXED(label, rd, wr, arg, decimals) \
        { label, ATC_MENU_KIND_NUMBER, 0, \
          &(const atc_menu_num_t){ { rd, wr, (arg) }, (atc_menu_u8)(decimals) } },

/**
 * @brief Hexadecimal input.
 * @param rd    getter
 * @param wr    setter; return 0 to refuse
 * @param arg   passed to both, typically the register id
 * @param bits  significant bit count, 4..32; shown as bits/4 digits, and the
 *              only bound on what may be entered
 */
#define ATC_MENU_HEX(label, rd, wr, arg, bits) \
        { label, ATC_MENU_KIND_HEX, 0, \
          &(const atc_menu_hex_t){ { rd, wr, (arg) }, (atc_menu_u8)(bits) } },

/**
 * @brief Option list; selecting previews the next, Enter commits (so `wr` runs
 *        once, not per preview step), Esc cancels.
 * @param rd      getter, returning the index into `choices`
 * @param wr      setter, receiving the chosen index
 * @param arg     passed to both
 * @param choices an ATC_MENU_CHOICES() array
 */
#define ATC_MENU_CHOICE(label, rd, wr, arg, choices) \
        { label, ATC_MENU_KIND_CHOICE, 0, \
          &(const atc_menu_choice_t){ { rd, wr, (arg) }, choices, \
              (atc_menu_u8)ATC_MENU_COUNT_OF(choices) } },

/**
 * @brief Runs fn(arg) when selected; one handler can serve many items.
 * @param fn    handler, `void fn(int32_t arg)`
 * @param arg   passed to `fn` - which item was clicked
 */
#define ATC_MENU_ACTION(label, fn, arg) \
        { label, ATC_MENU_KIND_ACTION, 0, \
          &(const atc_menu_action_t){ fn, arg } },

/**
 * @brief A row the library does not interpret: the application renders the
 *        value and parses the entry. Anything the tools above cannot express.
 * @param show  @ref atc_menu_show_fn; NULL displays "..."
 * @param edit  @ref atc_menu_edit_fn; NULL makes the row read-only
 * @param arg   passed to both
 */
#define ATC_MENU_CUSTOM(label, show, edit, arg) \
        { label, ATC_MENU_KIND_CUSTOM, 0, \
          &(const atc_menu_custom_t){ show, edit, (arg) } },

/**
 * @brief Free text input - a CUSTOM row with nothing to show.
 * @param cb    handler, `int cb(int32_t arg, const char *text)`; 0 refuses
 * @param arg   passed to `cb`, so one handler can serve several prompts
 */
#define ATC_MENU_PROMPT(label, cb, arg) ATC_MENU_CUSTOM(label, 0, cb, arg)

/**
 * @brief Enters a subpage.
 * @param page  a page defined earlier in the same file
 */
#define ATC_MENU_SUBMENU(label, page) \
        { label, ATC_MENU_KIND_SUBMENU, 0, &(page) },

#endif /* ATC_MENU_MENU_DEF_H */
