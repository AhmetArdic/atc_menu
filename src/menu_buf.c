/* SPDX-License-Identifier: MIT */
/**
 * @file menu_buf.c
 * @brief Bytes and numbers into a line buffer
 *
 * Nothing here touches the context or the sink. Every routine either fits
 * inside cap or only advances len past it, so the caller checks for an overflow
 * once, at the end of the line.
 */
#include "menu_internal.h"

void bstr(buf_t *b, const char *s)
{
    while (*s != '\0')
        bput(b, *s++);
}

/* Labels, banners and messages are the application's strings; the row half of
   the buffer is sized for `cols` and must not overrun. */
void bclip(buf_t *b, const char *s, size_t max)
{
    size_t i;

    for (i = 0u; s[i] != '\0' && b->vis < max; ++i)
        bput(b, s[i]);
}

/* An escape sequence occupies bytes but no columns. */
void bsgr(buf_t *b, const char *s)
{
    size_t vis = b->vis;

    bstr(b, s);
    b->vis = vis;
}

void bpad(buf_t *b, size_t n)
{
    while (n-- > 0u)
        bput(b, ' ');
}

void bu32(buf_t *b, uint32_t v)
{
    char     tmp[10];
    unsigned n = 0u;

    do {
        tmp[n++] = (char)('0' + (int)(v % 10u));
        v /= 10u;
    } while (v != 0u);

    while (n-- > 0u)
        bput(b, tmp[n]);
}

static const uint32_t POW10[5] = { 1u, 10u, 100u, 1000u, 10000u };

/* Magnitude and sign are carried separately so values above INT32_MAX keep
   their full range; a decimal point goes `decimals` places from the right. */
void bnum(buf_t *b, uint32_t mag, bool neg, unsigned decimals)
{
    uint32_t div;

    if (decimals > 4u)
        decimals = 4u;
    div = POW10[decimals];

    if (neg)
        bput(b, '-');
    bu32(b, mag / div);
    if (decimals > 0u) {
        uint32_t frac = mag % div;
        unsigned i;
        bput(b, '.');
        for (i = decimals; i-- > 0u;)
            bput(b, (char)('0' + (int)((frac / POW10[i]) % 10u)));
    }
}

void bhexdigits(buf_t *b, uint32_t v, unsigned digits)
{
    static const char tbl[] = "0123456789ABCDEF";
    unsigned          i;

    for (i = digits; i-- > 0u;)
        bput(b, tbl[(v >> (i * 4u)) & 0xFu]);
}
