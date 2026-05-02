/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "widget.h"

extern const widget_ops_t widget_group_ops;
extern const widget_ops_t widget_submenu_ops;
extern const widget_ops_t widget_value_ops;
extern const widget_ops_t widget_state_ops;
extern const widget_ops_t widget_action_ops;
extern const widget_ops_t widget_bar_ops;
extern const widget_ops_t widget_choice_ops;
extern const widget_ops_t widget_input_ops;

const widget_ops_t *widget_ops(atc_row_type_t type) {
    switch (type) {
        case ATC_ROW_GROUP:   return &widget_group_ops;
        case ATC_ROW_SUBMENU: return &widget_submenu_ops;
        case ATC_ROW_VALUE:   return &widget_value_ops;
        case ATC_ROW_STATE:   return &widget_state_ops;
        case ATC_ROW_ACTION:  return &widget_action_ops;
        case ATC_ROW_BAR:     return &widget_bar_ops;
        case ATC_ROW_CHOICE:  return &widget_choice_ops;
        case ATC_ROW_INPUT:   return &widget_input_ops;
    }
    return NULL;
}
