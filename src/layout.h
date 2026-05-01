/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file layout.h
 * @brief Internal — fixed column widths and buffer sizes.
 *
 * Not part of the public API. Application code should not include this.
 */

#ifndef ATC_LAYOUT_H
#define ATC_LAYOUT_H

#define MENU_LABEL_COL   24
#define MENU_VALUE_COL   10
#define MENU_UNIT_COL     5
#define MENU_BUF_SIZE    16
#define MENU_CMD_BUF     64
#define MENU_PRINT_BUF  128

/*
 * Inner width (visible cols) of the box, between left/right borders.
 * Sized to fit the widest possible value row:
 *   1 lead + 1 key + 2 + LABEL + VALUE + 1 + UNIT + 2 + status("WARN")
 */
#define MENU_HDR_INNER  50

#endif /* ATC_LAYOUT_H */
