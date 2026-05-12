/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "atc_menu/log_ring.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static char *slot(const atc_log_ring_t *r, uint16_t i) {
    return r->buf + (size_t)i * (size_t)r->cols;
}

static void advance(atc_log_ring_t *r) {
    r->head = (uint16_t)((r->head + 1u) % r->rows);
    if (r->count < r->rows) r->count++;
    r->seq++;
}

void atc_menu_log_init(atc_log_ring_t *r, char *buf, uint16_t rows, uint16_t cols) {
    if (!r) return;
    r->buf   = buf;
    r->rows  = rows;
    r->cols  = cols;
    r->head  = 0;
    r->count = 0;
    r->seq   = 0;
    if (!buf || rows == 0 || cols == 0) return;
    for (uint16_t i = 0; i < rows; i++) slot(r, i)[0] = '\0';
}

void atc_menu_log_push(atc_log_ring_t *r, const char *line) {
    if (!r || !r->buf || r->rows == 0 || r->cols == 0) return;

    char  *s   = slot(r, r->head);
    size_t cap = (size_t)r->cols - 1u;
    size_t n   = 0;
    if (line) while (n < cap && line[n]) { s[n] = line[n]; n++; }
    s[n] = '\0';

    advance(r);
}

int atc_menu_log_printf(atc_log_ring_t *r, const char *fmt, ...) {
    if (!r || !r->buf || r->rows == 0 || r->cols == 0) return 0;

    char   *s = slot(r, r->head);
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(s, r->cols, fmt, ap);
    va_end(ap);
    if (n < 0) { s[0] = '\0'; return n; }

    advance(r);
    return (n < (int)r->cols) ? n : (int)r->cols - 1;
}

const char *atc_menu_log_get(const atc_log_ring_t *r, uint16_t from_newest) {
    if (!r || !r->buf || r->count == 0 || from_newest >= r->count) return NULL;
    uint16_t newest = (uint16_t)((r->head + r->rows - 1u) % r->rows);
    uint16_t idx    = (uint16_t)((newest + r->rows - from_newest) % r->rows);
    return slot(r, idx);
}
