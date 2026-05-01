/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file ansi.h
 * @brief Internal — ANSI escape sequences used by the renderer.
 *
 * Not part of the public API. Application code should not include this.
 */

#ifndef ATC_ANSI_H
#define ATC_ANSI_H

#define ANSI_RESET     "\033[0m"
#define ANSI_DIM       "\033[2m"
#define ANSI_BOLD      "\033[1m"
#define ANSI_FG_OK     "\033[32m"
#define ANSI_FG_WARN   "\033[33m"
#define ANSI_FG_ERR    "\033[31m"
#define ANSI_FG_VAL    "\033[36m"
#define ANSI_FG_KEY    "\033[1;35m"
#define ANSI_BG_ZEBRA  "\033[48;5;236m"
#define ANSI_HOME      "\033[H"
#define ANSI_EOL       "\033[K"
#define ANSI_CLR_BELOW "\033[J"
#define ANSI_GOTO_FMT  "\033[%d;1H"  /* printf: row (1-based), col 1 */

#endif /* ATC_ANSI_H */
