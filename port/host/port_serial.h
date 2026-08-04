/**
 * @file port_serial.h
 * @brief Host serial port: drives the menu over a real COM/tty device.
 * @author Ahmet Talha ARDIC
 * @date   2026-07-31
 */
#ifndef ATC_MENU_PORT_SERIAL_H
#define ATC_MENU_PORT_SERIAL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opens the serial device with the given baud rate (8N1, no flow control).
 * @param dev  e.g. "COM3" (Windows) or "/dev/ttyUSB0" (POSIX)
 * @param baud e.g. 115200
 * @return 1 on success, 0 on failure
 */
int atc_menu_port_serial_open(const char *dev, unsigned long baud);

/** @brief Closes the serial device. */
void atc_menu_port_serial_close(void);

/** @brief Sink writing to the serial device; blocks until all bytes are out. */
int atc_menu_port_serial_sink(void *user, const char *buf, size_t len);

/** @brief Non-blocking read; returns the received byte or -1 if none. */
int atc_menu_port_serial_getkey(void);

#ifdef __cplusplus
}
#endif

#endif /* ATC_MENU_PORT_SERIAL_H */
