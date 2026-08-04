/**
 * @file port_stdio.h
 * @brief Host port for tests and the demo: stdout sink + non-blocking keyboard.
 * @author Ahmet Talha ARDIC
 * @date   2026-07-31
 */
#ifndef ATC_MENU_PORT_STDIO_H
#define ATC_MENU_PORT_STDIO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Puts the terminal in raw/VT mode (Windows: enables VT processing). */
void atc_menu_port_stdio_init(void);

/** @brief Restores the terminal. */
void atc_menu_port_stdio_deinit(void);

/** @brief Sink writing to stdout; always accepts. Pass as atc_menu_sink_fn. */
int atc_menu_port_stdio_sink(void *user, const char *buf, size_t len);

/** @brief Non-blocking key read; returns the byte or -1 if none is pending. */
int atc_menu_port_stdio_getkey(void);

/** @brief Sleeps for the given number of milliseconds. */
void atc_menu_port_stdio_sleep_ms(unsigned ms);

/**
 * @brief Waits until a key is pending or the timeout expires, whichever comes
 *        first. An idle loop can give the CPU back without putting a sleep
 *        between the keypress and the screen.
 */
void atc_menu_port_stdio_wait_ms(unsigned ms);

#ifdef __cplusplus
}
#endif

#endif /* ATC_MENU_PORT_STDIO_H */
