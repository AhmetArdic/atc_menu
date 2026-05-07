/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/render.h"
#include "render/row.h"
#include "widgets/widget.h"

#include <string.h>

extern const widget_ops_t widget_group_ops;
extern const widget_ops_t widget_submenu_ops;
extern const widget_ops_t widget_value_ops;
extern const widget_ops_t widget_state_ops;
extern const widget_ops_t widget_action_ops;
#if ATC_MENU_WIDGET_BAR
extern const widget_ops_t widget_bar_ops;
#endif
#if ATC_MENU_WIDGET_CHOICE
extern const widget_ops_t widget_choice_ops;
#endif
#if ATC_MENU_WIDGET_INPUT
extern const widget_ops_t widget_input_ops;
#endif

const widget_ops_t *widget_ops(atc_row_type_t type) {
    switch (type) {
        case ATC_ROW_GROUP:   return &widget_group_ops;
        case ATC_ROW_SUBMENU: return &widget_submenu_ops;
        case ATC_ROW_VALUE:   return &widget_value_ops;
        case ATC_ROW_STATE:   return &widget_state_ops;
        case ATC_ROW_ACTION:  return &widget_action_ops;
#if ATC_MENU_WIDGET_BAR
        case ATC_ROW_BAR:     return &widget_bar_ops;
#endif
#if ATC_MENU_WIDGET_CHOICE
        case ATC_ROW_CHOICE:  return &widget_choice_ops;
#endif
#if ATC_MENU_WIDGET_INPUT
        case ATC_ROW_INPUT:   return &widget_input_ops;
#endif
        default: break;
    }
    return NULL;
}

/* Stubs for disabled widgets — keep menu.c's mode-switch trivially false so
 * the surrounding code can stay branch-free. */
#if !ATC_MENU_WIDGET_INPUT
bool widget_input_active(void)        { return false; }
void widget_input_render_footer(void) {}
void widget_input_key(char k)         { (void)k; }
void widget_input_reset(void)         {}
#endif

#if !ATC_MENU_WIDGET_CHOICE
bool widget_choice_active(void)        { return false; }
void widget_choice_render_footer(void) {}
void widget_choice_key(char k)         { (void)k; }
void widget_choice_reset(void)         {}
#endif

void widget_render_scalar(int zebra_idx, const atc_menu_item_t *it) {
    char         val[MENU_BUF_SIZE] = {0};
    atc_status_t st                 = ATC_ST_NONE;
    if (it->read) it->read(val, sizeof val, &st);

    char key_buf[2] = { it->key ? it->key : ' ', 0 };
    const status_disp_t *sd = status_disp(st);

    row_t r;
    row_begin(&r, &ROW_LAYOUT_SCALAR, zebra_idx);
    row_set(&r, 0, NULL,      key_buf);
    row_set(&r, 1, NULL,      it->label);
    row_set(&r, 2, NULL,      val);
    row_set(&r, 3, NULL,      it->unit);
    row_set(&r, 4, sd->color, sd->text);
    row_end(&r);
}

void widget_render_scalar_data(int zebra_idx, const atc_menu_item_t *it,
                               size_t index) {
    (void)zebra_idx;
    char         val[MENU_BUF_SIZE] = {0};
    atc_status_t st                 = ATC_ST_NONE;
    if (it->read) it->read(val, sizeof val, &st);

    const status_disp_t *sd = status_disp(st);

    /* Region 2 = VALUE, region 4 = STATUS. KEY/LABEL/UNIT do not change. */
    menu_render_region_at(index, &ROW_LAYOUT_SCALAR, 2, NULL,      val);
    menu_render_region_at(index, &ROW_LAYOUT_SCALAR, 4, sd->color, sd->text);
}

void widget_validate_label_unit(const atc_menu_item_t *it) {
    if (it->label && strlen(it->label) > MENU_REGION_LABEL_W)
        menu_printf("WARN: label '%s' exceeds %d cols\r\n",
                        it->label, MENU_REGION_LABEL_W);
    if (it->unit && strlen(it->unit) > MENU_REGION_UNIT_W)
        menu_printf("WARN: unit '%s' exceeds %d cols\r\n",
                        it->unit, MENU_REGION_UNIT_W);
}
