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
 *
 * Override any default by defining the macro before including this header
 * (e.g., from a project-wide -D flag or CMake target_compile_definitions).
 */

#ifndef MENU_KEY_COL
#define MENU_KEY_COL       1
#endif
#ifndef MENU_LABEL_COL
#define MENU_LABEL_COL    24
#endif
#ifndef MENU_VALUE_COL
#define MENU_VALUE_COL    10
#endif
#ifndef MENU_UNIT_COL
#define MENU_UNIT_COL      5
#endif
#ifndef MENU_STATUS_COL
#define MENU_STATUS_COL    1
#endif
#ifndef MENU_FIELD_GAP_W
#define MENU_FIELD_GAP_W   3
#endif

#define MENU_INNER_W   (MENU_KEY_COL +                       \
                        MENU_FIELD_GAP_W + MENU_LABEL_COL +  \
                        MENU_FIELD_GAP_W + MENU_VALUE_COL +  \
                        MENU_FIELD_GAP_W + MENU_UNIT_COL +   \
                        MENU_FIELD_GAP_W + MENU_STATUS_COL)

#define MENU_GROUP_LABEL_W                                   \
    (MENU_INNER_W - MENU_KEY_COL - MENU_FIELD_GAP_W)

#define MENU_SUBMENU_PROMPT_W  2
#define MENU_SUBMENU_LABEL_W                                 \
    (MENU_INNER_W - MENU_KEY_COL - MENU_FIELD_GAP_W -        \
     MENU_SUBMENU_PROMPT_W)

#define MENU_INPUT_EDIT_W                                    \
    (MENU_VALUE_COL + MENU_FIELD_GAP_W + MENU_UNIT_COL +     \
     MENU_FIELD_GAP_W + MENU_STATUS_COL)

#define MENU_NOTE_W   MENU_INNER_W

#ifndef MENU_BUF_SIZE
#define MENU_BUF_SIZE    16
#endif
#ifndef MENU_CMD_BUF
#define MENU_CMD_BUF     64
#endif
#ifndef MENU_INPUT_BUF
#define MENU_INPUT_BUF   16
#endif
#ifndef MENU_ROW_BUF
#define MENU_ROW_BUF    256
#endif

#define UTF8_MAX_BYTES   4

#define MENU_VALUE_BUF      (MENU_VALUE_COL    * UTF8_MAX_BYTES + 1)
#define MENU_INPUT_EDIT_BUF (MENU_INPUT_EDIT_W * UTF8_MAX_BYTES + MENU_INPUT_BUF + 1)

#endif
