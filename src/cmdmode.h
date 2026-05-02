/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ATC_MENU_CMDMODE_H
#define ATC_MENU_CMDMODE_H

#include "internal.h"

void cmdmode_reset(void);
bool cmdmode_active(void);
void cmdmode_enter(void);
void cmdmode_key(char k);

#endif
