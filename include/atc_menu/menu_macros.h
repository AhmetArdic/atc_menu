/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file menu_macros.h
 * @brief Compile-time syntactic sugar for declaring Flash-resident menus.
 *
 * The macros expand to plain designated initializers so the entire menu
 * lives in .rodata with zero runtime cost.
 *
 * Every row macro accepts an optional leading hotkey. With the key the row
 * becomes hotkey-addressable; without it the row is display-only (the key
 * column renders blank):
 *
 *     ATC_MENU(home,
 *         ATC_GROUP ('e', "Quick view"),            // 'e' refreshes group
 *         ATC_VALUE ('t', "MCU Temp", "C", rd_temp),
 *         ATC_VALUE (     "Humidity", "%", rd_hum), // no hotkey
 *         ATC_ACTION('1', "Self-test", act_self_test),
 *         ATC_NOTE  ("Press hotkeys to interact."),
 *     );
 */

#ifndef ATC_MENU_MACROS_H
#define ATC_MENU_MACROS_H

#include "atc_menu/menu.h"

/* ----- arity dispatch (internal) ------------------------------------------ *
 * ATC_M_PICK(ATC_FOO_, a, b, c) expands to ATC_FOO_3(a, b, c), selecting the
 * keyed or keyless variant of each row macro by argument count (C99).
 */

#define ATC_M_CAT_(a_, b_)  a_##b_
#define ATC_M_CAT(a_, b_)   ATC_M_CAT_(a_, b_)
#define ATC_M_NARGS_(_1, _2, _3, _4, _5, _6, _7, _8, n_, ...) n_
#define ATC_M_NARGS(...)    ATC_M_NARGS_(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1)
#define ATC_M_PICK(base_, ...) \
    ATC_M_CAT(base_, ATC_M_NARGS(__VA_ARGS__))(__VA_ARGS__)

/* ----- row item literals -------------------------------------------------- */

/** ATC_GROUP([key,] label) — key triggers a group refresh. */
#define ATC_GROUP(...) ATC_M_PICK(ATC_GROUP_, __VA_ARGS__)
#define ATC_GROUP_1(label_) \
    { .type = ATC_MENU_ROW_GROUP, .label = (label_) }
#define ATC_GROUP_2(key_, label_) \
    { .type = ATC_MENU_ROW_GROUP, .key = (key_), .label = (label_) }

/** ATC_VALUE([key,] label, unit, read) */
#define ATC_VALUE(...) ATC_M_PICK(ATC_VALUE_, __VA_ARGS__)
#define ATC_VALUE_3(label_, unit_, read_) \
    { .type = ATC_MENU_ROW_VALUE, .label = (label_), \
      .unit = (unit_), .read = (read_) }
#define ATC_VALUE_4(key_, label_, unit_, read_) \
    { .type = ATC_MENU_ROW_VALUE, .key = (key_), .label = (label_), \
      .unit = (unit_), .read = (read_) }

/** ATC_STATE([key,] label, read, action) */
#define ATC_STATE(...) ATC_M_PICK(ATC_STATE_, __VA_ARGS__)
#define ATC_STATE_3(label_, read_, action_) \
    { .type = ATC_MENU_ROW_STATE, .label = (label_), \
      .read = (read_), .action = (action_) }
#define ATC_STATE_4(key_, label_, read_, action_) \
    { .type = ATC_MENU_ROW_STATE, .key = (key_), .label = (label_), \
      .read = (read_), .action = (action_) }

/** ATC_ACTION([key,] label, action) */
#define ATC_ACTION(...) ATC_M_PICK(ATC_ACTION_, __VA_ARGS__)
#define ATC_ACTION_2(label_, action_) \
    { .type = ATC_MENU_ROW_ACTION, .label = (label_), .action = (action_) }
#define ATC_ACTION_3(key_, label_, action_) \
    { .type = ATC_MENU_ROW_ACTION, .key = (key_), .label = (label_), \
      .action = (action_) }

/** ATC_SUBMENU([key,] label, sub) */
#define ATC_SUBMENU(...) ATC_M_PICK(ATC_SUBMENU_, __VA_ARGS__)
#define ATC_SUBMENU_2(label_, sub_) \
    { .type = ATC_MENU_ROW_SUBMENU, .label = (label_), .submenu = (sub_) }
#define ATC_SUBMENU_3(key_, label_, sub_) \
    { .type = ATC_MENU_ROW_SUBMENU, .key = (key_), .label = (label_), \
      .submenu = (sub_) }

/** ATC_BAR([key,] label, read) */
#define ATC_BAR(...) ATC_M_PICK(ATC_BAR_, __VA_ARGS__)
#define ATC_BAR_2(label_, read_) \
    { .type = ATC_MENU_ROW_BAR, .label = (label_), .read = (read_) }
#define ATC_BAR_3(key_, label_, read_) \
    { .type = ATC_MENU_ROW_BAR, .key = (key_), .label = (label_), \
      .read = (read_) }

/** ATC_CHOICE([key,] label, choices, count, idx_ptr, commit) */
#define ATC_CHOICE(...) ATC_M_PICK(ATC_CHOICE_, __VA_ARGS__)
#define ATC_CHOICE_5(label_, choices_, count_, idx_ptr_, commit_) \
    { .type = ATC_MENU_ROW_CHOICE, .label = (label_), \
      .choices = (choices_), .choice_count = (count_), \
      .choice_idx = (idx_ptr_), .choice_commit = (commit_) }
#define ATC_CHOICE_6(key_, label_, choices_, count_, idx_ptr_, commit_) \
    { .type = ATC_MENU_ROW_CHOICE, .key = (key_), .label = (label_), \
      .choices = (choices_), .choice_count = (count_), \
      .choice_idx = (idx_ptr_), .choice_commit = (commit_) }

/** ATC_INPUT([key,] label, unit, read, type, min, max, commit) */
#define ATC_INPUT(...) ATC_M_PICK(ATC_INPUT_, __VA_ARGS__)
#define ATC_INPUT_7(label_, unit_, read_, type_, min_, max_, commit_) \
    { .type = ATC_MENU_ROW_INPUT, .label = (label_), .unit = (unit_), \
      .read = (read_), .input_type = (type_), .input_min = (min_), \
      .input_max = (max_), .input_commit = (commit_) }
#define ATC_INPUT_8(key_, label_, unit_, read_, type_, min_, max_, commit_) \
    { .type = ATC_MENU_ROW_INPUT, .key = (key_), .label = (label_), \
      .unit = (unit_), .read = (read_), .input_type = (type_), \
      .input_min = (min_), .input_max = (max_), .input_commit = (commit_) }

#define ATC_NOTE(text_) \
    { .type = ATC_MENU_ROW_NOTE, .label = (text_) }

/* ----- top-level menu declaration ----------------------------------------- */

#define ATC_MENU(name_, ...)                                                    \
    static const atc_menu_item_t name_##_items[] = { __VA_ARGS__ };             \
    static const atc_menu_table_t name_ = {                                     \
        .items = name_##_items,                                                 \
        .count = sizeof(name_##_items) / sizeof((name_##_items)[0]),            \
    }

#endif /* ATC_MENU_MACROS_H */
