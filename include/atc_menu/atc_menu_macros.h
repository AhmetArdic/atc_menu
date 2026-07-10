/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file atc_menu_macros.h
 * @brief Compile-time syntactic sugar for declaring Flash-resident menus.
 *
 * The macros expand to plain designated initializers so the entire menu
 * lives in .rodata with zero runtime cost.
 *
 *     ATC_MENU(home,
 *         ATC_GROUP ("Quick view"),
 *         ATC_VALUE ('t', "MCU Temp", "C", rd_temp),
 *         ATC_ACTION('1', "Self-test", act_self_test),
 *         ATC_NOTE  ("Press hotkeys to interact."),
 *     );
 */

#ifndef ATC_MENU_MACROS_H
#define ATC_MENU_MACROS_H

#include "atc_menu/atc_menu.h"

/* ----- row item literals -------------------------------------------------- */

#define ATC_GROUP(label_) \
    { .type = ATC_ROW_GROUP, .label = (label_) }

#define ATC_GROUP_REFRESH(key_, label_) \
    { .type = ATC_ROW_GROUP, .key = (key_), .label = (label_) }

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

#define ATC_NOTE(text_) \
    { .type = ATC_ROW_NOTE, .label = (text_) }

/* ----- top-level menu declaration ----------------------------------------- */

#define ATC_MENU(name_, ...)                                                    \
    static const atc_menu_item_t name_##_items[] = { __VA_ARGS__ };             \
    static const atc_menu_table_t name_ = {                                     \
        .items = name_##_items,                                                 \
        .count = sizeof(name_##_items) / sizeof((name_##_items)[0]),            \
    }

#endif /* ATC_MENU_MACROS_H */
