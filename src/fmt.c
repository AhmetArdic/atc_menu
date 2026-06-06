/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * libc-free string/number plumbing, so the framework links no <stdio.h> and
 * no <stdlib.h>. The formatter replaces vsnprintf — typically the single
 * heaviest object in an embedded link (multi-KB parser, often dragging in
 * soft-float) — with a few hundred bytes we control.
 *
 * Formatter surface = exactly what the menu uses: %%, %c, %s, %d, %ld, with
 * optional '-' flag, '*'/numeric width, and '.*'/'.N' precision (%s only).
 * Return/termination match vsnprintf: at most cap-1 chars + NUL, and the
 * return is the length that would have been written.
 */

#include "internal.h"

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>

typedef struct {
    char  *out;
    size_t cap;
    size_t len;
} sink_t;

static void emit(sink_t *s, char c) {
    if (s->cap && s->len + 1 < s->cap) s->out[s->len] = c;
    s->len++;
}

static void emit_field(sink_t *s, const char *body, int blen, int width, bool left) {
    int pad = width - blen;
    if (!left) while (pad-- > 0) emit(s, ' ');
    for (int i = 0; i < blen; i++) emit(s, body[i]);
    if (left)  while (pad-- > 0) emit(s, ' ');
}

/* Decimal render of a long into buf (no NUL). Accumulates in unsigned to
 * stay defined at LONG_MIN. Returns the digit/sign count. */
static int long_to_str(char *buf, long v) {
    char tmp[24];
    int  i = 0;
    bool neg = v < 0;
    unsigned long u = neg ? -(unsigned long)v : (unsigned long)v;
    do { tmp[i++] = (char)('0' + (u % 10)); u /= 10; } while (u);

    int n = 0;
    if (neg) buf[n++] = '-';
    while (i > 0) buf[n++] = tmp[--i];
    return n;
}

static int parse_int(const char **fmt) {
    int v = 0;
    while (**fmt >= '0' && **fmt <= '9') v = v * 10 + (*(*fmt)++ - '0');
    return v;
}

int atc_vsnprintf(char *out, size_t cap, const char *fmt, va_list ap) {
    sink_t s = { out, cap, 0 };

    for (; *fmt; fmt++) {
        if (*fmt != '%') { emit(&s, *fmt); continue; }

        fmt++;
        if (*fmt == '%') { emit(&s, '%'); continue; }

        bool left = false;
        while (*fmt == '-') { left = true; fmt++; }

        int width;
        if (*fmt == '*') { width = va_arg(ap, int); fmt++; }
        else             width = parse_int(&fmt);
        if (width < 0) { left = true; width = -width; }

        int prec = -1;
        if (*fmt == '.') {
            fmt++;
            if (*fmt == '*') { prec = va_arg(ap, int); fmt++; }
            else             prec = parse_int(&fmt);
        }

        bool is_long = false;
        while (*fmt == 'l') { is_long = true; fmt++; }

        char numbuf[24];
        switch (*fmt) {
            case 's': {
                const char *p = va_arg(ap, const char *);
                if (!p) p = "";
                int n = 0;
                while ((prec < 0 || n < prec) && p[n]) n++;
                emit_field(&s, p, n, width, left);
                break;
            }
            case 'c': {
                char c = (char)va_arg(ap, int);
                emit_field(&s, &c, 1, width, left);
                break;
            }
            case 'd': {
                long v = is_long ? va_arg(ap, long) : va_arg(ap, int);
                int  n = long_to_str(numbuf, v);
                emit_field(&s, numbuf, n, width, left);
                break;
            }
            default: /* unknown conversion: surface it rather than swallow */
                emit(&s, '%');
                if (*fmt) emit(&s, *fmt);
                break;
        }
    }

    if (cap) out[s.len < cap ? s.len : cap - 1] = '\0';
    return (int)s.len;
}

int atc_snprintf(char *out, size_t cap, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = atc_vsnprintf(out, cap, fmt, ap);
    va_end(ap);
    return n;
}

/* Parse a base-10 or base-16 integer; accepts an optional sign and, for hex,
 * an optional "0x" prefix. Unlike bare accumulation it rejects empty input,
 * trailing garbage, and overflow — the three checks input validation needs. */
bool atc_parse_long(const char *s, int base, long *out) {
    if (!s) return false;

    bool neg = false;
    if (*s == '+' || *s == '-') neg = (*s++ == '-');
    if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    if (!*s) return false;

    unsigned long limit = neg ? (unsigned long)LONG_MAX + 1UL : (unsigned long)LONG_MAX;
    unsigned long acc   = 0;
    for (; *s; s++) {
        unsigned d;
        if      (*s >= '0' && *s <= '9')                d = (unsigned)(*s - '0');
        else if (base == 16 && *s >= 'a' && *s <= 'f')  d = (unsigned)(*s - 'a' + 10);
        else if (base == 16 && *s >= 'A' && *s <= 'F')  d = (unsigned)(*s - 'A' + 10);
        else return false;
        if (acc > (limit - d) / (unsigned)base) return false;
        acc = acc * (unsigned)base + d;
    }

    *out = (long)(neg ? 0UL - acc : acc);
    return true;
}
