/**
 * @file menu_fmt.c
 * @brief ATC Menu - integer-only number formatting; printf is never used.
 * @author Ahmet Talha ARDIC
 * @date   2026-07-31
 */
#include "menu_internal.h"

unsigned atc_menu_fmt_u32(char *dst, uint32_t v)
{
    char tmp[10];
    unsigned n = 0, i;

    do {
        tmp[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    } while (v);

    for (i = 0; i < n; i++)
        dst[i] = tmp[n - 1 - i];
    return n;
}

unsigned atc_menu_fmt_i32(char *dst, int32_t v)
{
    uint32_t mag = (v < 0) ? 0u - (uint32_t)v : (uint32_t)v;
    unsigned n = 0;

    if (v < 0)
        dst[n++] = '-';
    return n + atc_menu_fmt_u32(dst + n, mag);
}

unsigned atc_menu_fmt_hex(char *dst, uint32_t v, unsigned digits)
{
    static const char hexc[] = "0123456789ABCDEF";
    unsigned i;

    if (digits < 1)
        digits = 1;
    if (digits > 8)
        digits = 8;
    for (i = 0; i < digits; i++)
        dst[i] = hexc[(v >> (4u * (digits - 1 - i))) & 0xFu];
    return digits;
}

unsigned atc_menu_fmt_fix(char *dst, int32_t v, unsigned decimals, int is_signed)
{
    int neg = is_signed && v < 0;
    uint32_t mag = neg ? 0u - (uint32_t)v : (uint32_t)v;
    uint32_t scale = 1;
    unsigned i, n = 0;

    if (decimals == 0)
        return is_signed ? atc_menu_fmt_i32(dst, v)
                         : atc_menu_fmt_u32(dst, (uint32_t)v);

    for (i = 0; i < decimals; i++)
        scale *= 10u;

    if (neg)
        dst[n++] = '-';
    n += atc_menu_fmt_u32(dst + n, mag / scale);
    dst[n++] = '.';
    /* fraction, zero padded to `decimals` digits */
    for (i = 0, scale /= 10u; i < decimals; i++, scale /= 10u)
        dst[n++] = (char)('0' + (mag / (scale ? scale : 1u)) % 10u);
    return n;
}
