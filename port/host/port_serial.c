/* SPDX-License-Identifier: MIT */
/**
 * @file port_serial.c
 * @brief Host serial port, 8N1, no flow control
 */
#include "port_serial.h"

#include <string.h>

#ifdef _WIN32

#include <stdio.h>
#include <windows.h>

static HANDLE hser = INVALID_HANDLE_VALUE;

int atc_menu_port_serial_open(const char *dev, unsigned long baud)
{
    char         path[64];
    DCB          dcb;
    COMMTIMEOUTS to;

    if (strlen(dev) > 48u)
        return 0;
    /* COM10 and up need the device-namespace prefix; COM1..9 tolerate it */
    if (strncmp(dev, "\\\\", 2) != 0)
        sprintf(path, "\\\\.\\%s", dev);
    else
        strcpy(path, dev);

    hser = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, 0, NULL);
    if (hser == INVALID_HANDLE_VALUE)
        return 0;

    memset(&dcb, 0, sizeof dcb);
    dcb.DCBlength = sizeof dcb;
    GetCommState(hser, &dcb);
    dcb.BaudRate = baud;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    if (!SetCommState(hser, &dcb)) {
        atc_menu_port_serial_close();
        return 0;
    }

    memset(&to, 0, sizeof to);
    to.ReadIntervalTimeout = MAXDWORD; /* non-blocking reads */
    SetCommTimeouts(hser, &to);
    PurgeComm(hser, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return 1;
}

void atc_menu_port_serial_close(void)
{
    if (hser != INVALID_HANDLE_VALUE) {
        CloseHandle(hser);
        hser = INVALID_HANDLE_VALUE;
    }
}

int atc_menu_port_serial_sink(const char *buf, size_t len, void *user)
{
    DWORD written;

    (void)user;
    return WriteFile(hser, buf, (DWORD)len, &written, NULL) &&
           written == (DWORD)len;
}

int atc_menu_port_serial_getkey(void)
{
    unsigned char c;
    DWORD         got;

    if (ReadFile(hser, &c, 1, &got, NULL) && got == 1u)
        return c;
    return -1;
}

#else /* POSIX */

#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

static int fd = -1;

static speed_t to_speed(unsigned long baud)
{
    switch (baud) {
    case 9600ul:   return B9600;
    case 19200ul:  return B19200;
    case 38400ul:  return B38400;
    case 57600ul:  return B57600;
    case 115200ul: return B115200;
    case 230400ul: return B230400;
    default:       return 0;
    }
}

int atc_menu_port_serial_open(const char *dev, unsigned long baud)
{
    struct termios tio;
    speed_t        spd = to_speed(baud);

    if (spd == 0)
        return 0;
    fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0)
        return 0;

    memset(&tio, 0, sizeof tio);
    tio.c_cflag = CS8 | CREAD | CLOCAL;
    cfsetispeed(&tio, spd);
    cfsetospeed(&tio, spd);
    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        atc_menu_port_serial_close();
        return 0;
    }
    tcflush(fd, TCIOFLUSH);
    return 1;
}

void atc_menu_port_serial_close(void)
{
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

/* All of it or none: the menu rebuilds a refused line from its own ESC. */
int atc_menu_port_serial_sink(const char *buf, size_t len, void *user)
{
    size_t  done = 0u;
    ssize_t n;

    (void)user;
    while (done < len) {
        n = write(fd, buf + done, len - done);
        if (n > 0)
            done += (size_t)n;
        else if (n < 0 && errno != EAGAIN && errno != EINTR)
            return 0;
    }
    return 1;
}

int atc_menu_port_serial_getkey(void)
{
    unsigned char c;

    if (read(fd, &c, 1u) == 1)
        return c;
    return -1;
}

#endif
