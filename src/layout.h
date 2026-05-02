/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ATC_LAYOUT_H
#define ATC_LAYOUT_H

/*
 *   |  K   LABEL                      VALUE         UNIT     STAT  |
 *      ^   ^^^^^                      ^^^^^^        ^^^^     ^^^^^
 *     KEY  LABEL_COL                  VALUE_COL     UNIT     STATUS
 */
#define MENU_KEY_COL       1
#define MENU_LABEL_COL    24
#define MENU_VALUE_COL    10
#define MENU_UNIT_COL      5
#define MENU_STATUS_COL    4

#define MENU_PAD_W         0
#define MENU_FIELD_GAP_W   3

#define MENU_INNER_W   (MENU_PAD_W + MENU_KEY_COL +          \
                        MENU_FIELD_GAP_W + MENU_LABEL_COL +  \
                        MENU_FIELD_GAP_W + MENU_VALUE_COL +  \
                        MENU_FIELD_GAP_W + MENU_UNIT_COL +   \
                        MENU_FIELD_GAP_W + MENU_STATUS_COL + \
                        MENU_PAD_W)

#define MENU_GROUP_LABEL_W                                   \
    (MENU_INNER_W - MENU_PAD_W - MENU_KEY_COL -              \
     MENU_FIELD_GAP_W - MENU_PAD_W)

#define MENU_SUBMENU_PROMPT_W  2
#define MENU_SUBMENU_LABEL_W                                 \
    (MENU_INNER_W - MENU_PAD_W - MENU_KEY_COL -              \
     MENU_FIELD_GAP_W - MENU_SUBMENU_PROMPT_W - MENU_PAD_W)

#define MENU_INPUT_EDIT_W                                    \
    (MENU_VALUE_COL + MENU_FIELD_GAP_W + MENU_UNIT_COL +     \
     MENU_FIELD_GAP_W + MENU_STATUS_COL)

#define MENU_NOTE_W  (MENU_INNER_W - 2 * MENU_PAD_W)

#define MENU_BUF_SIZE    16
#define MENU_CMD_BUF     64
#define MENU_INPUT_BUF   16
#define MENU_ROW_BUF    256

#endif
