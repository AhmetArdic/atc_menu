/**
 * @file port_stdio.c
 * @brief ATC Menu - host port (Windows console / POSIX termios).
 * @author Ahmet Talha ARDIC
 * @date   2026-07-31
 */
#include <stdio.h>

#include "port_stdio.h"

int atc_menu_port_stdio_sink(void *user, const char *buf, size_t len)
{
    (void)user;
    fwrite(buf, 1, len, stdout);
    fflush(stdout);
    return 1;
}

#ifdef _WIN32

#include <conio.h>
#include <windows.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING  /* missing from old MinGW headers */
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

static DWORD orig_mode;
static HANDLE hout;

void atc_menu_port_stdio_init(void)
{
    hout = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(hout, &orig_mode);
    SetConsoleMode(hout, orig_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

void atc_menu_port_stdio_deinit(void)
{
    SetConsoleMode(hout, orig_mode);
}

int atc_menu_port_stdio_getkey(void)
{
    int c;

    if (!_kbhit())
        return -1;
    c = _getch();
    if (c == 0 || c == 0xE0) {  /* extended key: drop the second byte too */
        if (_kbhit())
            (void)_getch();
        return -1;
    }
    return c & 0xFF;
}

void atc_menu_port_stdio_sleep_ms(unsigned ms)
{
    Sleep(ms);
}

void atc_menu_port_stdio_wait_ms(unsigned ms)
{
    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;

    if (_kbhit())
        return;
    /* Only a console handle can be waited on for keys; a redirected stdin
     * (pipe or file) falls back to sleeping. The handle is also signalled by
     * mouse and focus events, which just costs one extra loop pass. */
    if (GetConsoleMode(hin, &mode))
        WaitForSingleObject(hin, ms);
    else
        Sleep(ms);
}

#else /* POSIX */

#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

static struct termios orig_tio;

void atc_menu_port_stdio_init(void)
{
    struct termios tio;

    tcgetattr(STDIN_FILENO, &orig_tio);
    tio = orig_tio;
    tio.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &tio);
}

void atc_menu_port_stdio_deinit(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_tio);
}

int atc_menu_port_stdio_getkey(void)
{
    unsigned char c;

    if (read(STDIN_FILENO, &c, 1) == 1)
        return c;
    return -1;
}

void atc_menu_port_stdio_sleep_ms(unsigned ms)
{
    usleep(ms * 1000u);
}

void atc_menu_port_stdio_wait_ms(unsigned ms)
{
    fd_set rd;
    struct timeval tv;

    FD_ZERO(&rd);
    FD_SET(STDIN_FILENO, &rd);
    tv.tv_sec = (long)(ms / 1000u);
    tv.tv_usec = (long)((ms % 1000u) * 1000u);
    select(STDIN_FILENO + 1, &rd, NULL, NULL, &tv);
}

#endif
