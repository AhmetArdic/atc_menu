/* SPDX-License-Identifier: MIT */
/**
 * @file fake_sink.c
 * @brief Scriptable sink that records every byte the menu emits
 */
#include "fake_sink.h"

#include <string.h>

void fake_reset(fake_t *f)
{
    memset(f, 0, sizeof *f);
    f->refuse_after = -1;
}

int fake_sink(const char *buf, size_t len, void *user)
{
    fake_t *f = (fake_t *)user;

    f->calls++;
    if (f->refuse_always)
        return 0;
    if (f->refuse_after >= 0 && f->calls > f->refuse_after)
        return 0;
    if (f->len + len > FAKE_CAP)
        return 0;

    memcpy(f->out + f->len, buf, len);
    f->len += len;
    return 1;
}

int fake_count(const fake_t *f, const char *needle)
{
    size_t n = strlen(needle);
    int    hits = 0;
    size_t i;

    if (n == 0u || f->len < n)
        return 0;

    for (i = 0; i + n <= f->len; ++i) {
        if (memcmp(f->out + i, needle, n) == 0)
            hits++;
    }
    return hits;
}

int fake_has(const fake_t *f, const char *needle)
{
    return fake_count(f, needle) > 0;
}

/* Drops ESC [ ... <final> and the two-byte ESC 7 / ESC 8, which is every
   sequence this library emits. */
int fake_has_text(const fake_t *f, const char *needle)
{
    static char plain[FAKE_CAP + 1];
    size_t      n = strlen(needle);
    size_t      i = 0u;
    size_t      w = 0u;

    while (i < f->len) {
        if (f->out[i] == '\x1b') {
            i++;
            if (i < f->len && f->out[i] == '[') {
                i++;
                while (i < f->len &&
                       !((unsigned char)f->out[i] >= 0x40u &&
                         (unsigned char)f->out[i] <= 0x7Eu))
                    i++;
            }
            if (i < f->len) /* the final byte, or the lone 7/8 */
                i++;
            continue;
        }
        plain[w++] = f->out[i++];
    }
    plain[w] = '\0';

    if (n == 0u || w < n)
        return 0;
    for (i = 0u; i + n <= w; ++i) {
        if (memcmp(plain + i, needle, n) == 0)
            return 1;
    }
    return 0;
}
