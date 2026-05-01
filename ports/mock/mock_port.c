/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file mock_port.c
 * @brief In-memory port implementation. See mock_port.h.
 */

#include "mock_port.h"

#include <string.h>

#define MOCK_BUF_SIZE 4096
#define MOCK_CMD_SIZE  128

static char   m_buf[MOCK_BUF_SIZE];
static size_t m_len;
static char   m_last_cmd[MOCK_CMD_SIZE];
static size_t m_cmd_calls;

static void m_write(const char *buf, size_t len) {
    if (m_len + len >= sizeof m_buf) {
        if (m_len + 1 >= sizeof m_buf) return;
        len = sizeof m_buf - 1 - m_len;
    }
    memcpy(m_buf + m_len, buf, len);
    m_len += len;
    m_buf[m_len] = '\0';
}

static void m_cmd(const char *line) {
    m_cmd_calls++;
    size_t n = strlen(line);
    if (n >= sizeof m_last_cmd) n = sizeof m_last_cmd - 1;
    memcpy(m_last_cmd, line, n);
    m_last_cmd[n] = '\0';
}

const atc_menu_port_t mock_port = {
    .write = m_write,
    .cmd   = m_cmd,
};

void mock_reset(void) {
    m_len       = 0;
    m_buf[0]    = '\0';
    m_cmd_calls = 0;
    m_last_cmd[0] = '\0';
}

const char *mock_buffer(void)    { return m_buf; }
size_t      mock_len(void)       { return m_len; }
const char *mock_last_cmd(void)  { return m_last_cmd; }
size_t      mock_cmd_calls(void) { return m_cmd_calls; }
