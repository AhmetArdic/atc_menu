/**
 * @file menu_config.h
 * @brief ATC Menu compile-time options.
 *
 * Every option here is guarded by #ifndef and can be overridden from the
 * compiler command line (-DATC_MENU_CFG_X=...). These are the only knobs;
 * the sizes derived from them live in menu.h.
 *
 * @author Ahmet Talha ARDIC
 * @date   2026-07-31
 */
#ifndef ATC_MENU_CONFIG_H
#define ATC_MENU_CONFIG_H

/**
 * Row layout - the only two width knobs:
 *
 *   "   1   Voltage (mV)            1234.56   "
 *    \_____/\________________/ \__________/\_/
 *       7         LABEL       1     VAL     2
 *
 * so a row is ATC_MENU_CFG_LABEL + ATC_MENU_CFG_VAL + 10 columns wide.
 * The split is a minimum, not a fence: a value shorter than VAL hands the
 * spare columns to the label.
 */

/** Label width guaranteed when the value column is full. */
#ifndef ATC_MENU_CFG_LABEL
#define ATC_MENU_CFG_LABEL 38
#endif

/** Value width - READOUT/NUMBER/HEX/CHOICE text is truncated to this. */
#ifndef ATC_MENU_CFG_VAL
#define ATC_MENU_CFG_VAL 12
#endif

/** User input buffer (edit line, including NUL). */
#ifndef ATC_MENU_CFG_INPUT_BUF
#define ATC_MENU_CFG_INPUT_BUF 24
#endif

/** Navigation stack depth (nested page limit). */
#ifndef ATC_MENU_CFG_MAX_DEPTH
#define ATC_MENU_CFG_MAX_DEPTH 8
#endif

/** End-of-line sequence. */
#ifndef ATC_MENU_CFG_EOL
#define ATC_MENU_CFG_EOL "\r\n"
#endif

#if ATC_MENU_CFG_LABEL < 4
#error "ATC_MENU_CFG_LABEL must be at least 4"
#endif

#if ATC_MENU_CFG_VAL < 12
#error "ATC_MENU_CFG_VAL must be at least 12 (formatted numbers must fit)"
#endif

#endif /* ATC_MENU_CONFIG_H */
