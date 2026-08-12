/* SPDX-License-Identifier: MIT */
/**
 * @file port_serial.h
 * @brief Host serial port: drives the menu over a real COM/tty device
 */
#ifndef ATC_MENU_PORT_SERIAL_H
#define ATC_MENU_PORT_SERIAL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Open the device at the given baud rate, 8N1, no flow control
 * @param dev "/dev/ttyUSB0" on POSIX, "COM3" on Windows
 * @return 1 on success, 0 on failure
 */
int atc_menu_port_serial_open(const char *dev, unsigned long baud);

/** @brief Close the device */
void atc_menu_port_serial_close(void);

/** @brief Sink for atc_menu_init; blocks until every byte is out */
int atc_menu_port_serial_sink(const char *buf, size_t len, void *user);

/** @brief Non-blocking read; the received byte, or -1 when none arrived */
int atc_menu_port_serial_getkey(void);

#ifdef __cplusplus
}
#endif
#endif /* ATC_MENU_PORT_SERIAL_H */
