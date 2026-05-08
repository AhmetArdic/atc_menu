/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ATC_LAYOUT_H
#define ATC_LAYOUT_H

/*
 *   |  K   LABEL                      VALUE         UNIT     STAT  |
 *      ^   ^^^^^                      ^^^^^^        ^^^^     ^^^^^
 *      KEY LABEL                      VALUE         UNIT     STATUS
 *
 * Override any default by defining the macro before including this header.
 */

#ifndef MENU_REGION_KEY_W
#define MENU_REGION_KEY_W       1
#endif
#ifndef MENU_REGION_LABEL_W
#define MENU_REGION_LABEL_W    24
#endif
#ifndef MENU_REGION_VALUE_W
#define MENU_REGION_VALUE_W    10
#endif
#ifndef MENU_REGION_UNIT_W
#define MENU_REGION_UNIT_W      5
#endif
#ifndef MENU_REGION_STATUS_W
#define MENU_REGION_STATUS_W    1
#endif
#ifndef MENU_REGION_GAP_W
#define MENU_REGION_GAP_W       3
#endif

#define MENU_INNER_W   (MENU_REGION_KEY_W +                          \
                        MENU_REGION_GAP_W + MENU_REGION_LABEL_W +    \
                        MENU_REGION_GAP_W + MENU_REGION_VALUE_W +    \
                        MENU_REGION_GAP_W + MENU_REGION_UNIT_W +     \
                        MENU_REGION_GAP_W + MENU_REGION_STATUS_W)

#define MENU_REGION_GROUP_LABEL_W                                    \
    (MENU_INNER_W - MENU_REGION_KEY_W - MENU_REGION_GAP_W)

#define MENU_REGION_SUBMENU_PROMPT_W  2
#define MENU_REGION_SUBMENU_LABEL_W                                  \
    (MENU_INNER_W - MENU_REGION_KEY_W - MENU_REGION_GAP_W -          \
     MENU_REGION_SUBMENU_PROMPT_W)

#define MENU_REGION_INPUT_EDIT_W                                     \
    (MENU_REGION_VALUE_W + MENU_REGION_GAP_W + MENU_REGION_UNIT_W +  \
     MENU_REGION_GAP_W + MENU_REGION_STATUS_W)

#define MENU_REGION_NOTE_W   MENU_INNER_W

#define MENU_HEADER_LINES    3

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

#define MENU_REGION_VALUE_BUF      (MENU_REGION_VALUE_W * UTF8_MAX_BYTES + 1)
#define MENU_REGION_INPUT_EDIT_BUF (MENU_REGION_INPUT_EDIT_W * UTF8_MAX_BYTES \
                                    + MENU_INPUT_BUF + 1)

#endif
