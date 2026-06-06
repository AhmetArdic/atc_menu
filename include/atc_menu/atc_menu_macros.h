/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file atc_menu_macros.h
 * @brief Compile-time syntactic sugar for declaring Flash-resident menus.
 *
 * The verb set mirrors the runtime builder API (atc_menu_group(),
 * atc_menu_value(), etc.); the macros expand to plain designated
 * initializers so the entire menu lives in .rodata with zero runtime
 * cost. Use these for stable menus; use the builder API when the menu
 * must change at runtime.
 *
 *     static const char *const home_notes[] = {
 *         "Press hotkeys to interact.",
 *     };
 *
 *     ATC_MENU(home, ATC_WITH_NOTES(home_notes),
 *         ATC_GROUP ("Quick view"),
 *         ATC_VALUE ('t', "MCU Temp", "C", rd_temp),
 *         ATC_ACTION('1', "Self-test", act_self_test),
 *     );
 */

#ifndef ATC_MENU_MACROS_H
#define ATC_MENU_MACROS_H

#include "atc_menu/atc_menu.h"

/* ----- row item literals -------------------------------------------------- */

#define ATC_GROUP(label_) \
    { .type = ATC_ROW_GROUP, .label = (label_) }

#define ATC_VALUE(key_, label_, unit_, read_) \
    { .type = ATC_ROW_VALUE, .key = (key_), .label = (label_), \
      .unit = (unit_), .read = (read_) }

#define ATC_STATE(key_, label_, read_, action_) \
    { .type = ATC_ROW_STATE, .key = (key_), .label = (label_), \
      .read = (read_), .action = (action_) }

#define ATC_ACTION(key_, label_, action_) \
    { .type = ATC_ROW_ACTION, .key = (key_), .label = (label_), \
      .action = (action_) }

#define ATC_SUBMENU(key_, label_, sub_) \
    { .type = ATC_ROW_SUBMENU, .key = (key_), .label = (label_), \
      .submenu = (sub_) }

#define ATC_BAR(key_, label_, read_) \
    { .type = ATC_ROW_BAR, .key = (key_), .label = (label_), .read = (read_) }

#define ATC_CHOICE(key_, label_, choices_, count_, idx_ptr_, commit_) \
    { .type = ATC_ROW_CHOICE, .key = (key_), .label = (label_), \
      .choices = (choices_), .choice_count = (count_), \
      .choice_idx = (idx_ptr_), .choice_commit = (commit_) }

#define ATC_INPUT(key_, label_, unit_, read_, type_, min_, max_, commit_) \
    { .type = ATC_ROW_INPUT, .key = (key_), .label = (label_), .unit = (unit_), \
      .read = (read_), .input_type = (type_), .input_min = (min_), \
      .input_max = (max_), .input_commit = (commit_) }

/* ----- notes shorthand ---------------------------------------------------- */

#define ATC_NO_NOTES           NULL, 0
#define ATC_WITH_NOTES(arr_)   (arr_), (sizeof(arr_) / sizeof((arr_)[0]))

/* ----- top-level menu declaration ----------------------------------------- */

/* The double-indirection on _ATC_NOTES_FROM lets a single token like
 * ATC_WITH_NOTES(home_notes) supply both struct fields after one extra
 * rescan splits it on its inner comma. */
#define _ATC_NOTES_FROM(...)       _ATC_NOTES_FROM_(__VA_ARGS__)
#define _ATC_NOTES_FROM_(p_, n_)   .notes = (p_), .note_count = (n_)

#define ATC_MENU(name_, notes_args_, ...)                                       \
    static const atc_menu_item_t name_##_items[] = { __VA_ARGS__ };             \
    static const atc_menu_table_t name_ = {                                     \
        .items = name_##_items,                                                 \
        .count = sizeof(name_##_items) / sizeof((name_##_items)[0]),            \
        _ATC_NOTES_FROM(notes_args_)                                            \
    }

#endif /* ATC_MENU_MACROS_H */
