/* SPDX-License-Identifier: MIT */
/**
 * @file fake_sink.h
 * @brief Scriptable sink that records every byte the menu emits
 */
#ifndef FAKE_SINK_H
#define FAKE_SINK_H

#include <stddef.h>

#define FAKE_CAP 16384

typedef struct {
    char   out[FAKE_CAP];
    size_t len;
    int    refuse_after; /* -1 = never; else refuse from the nth call on */
    int    calls;
    int    refuse_always;
} fake_t;

void fake_reset(fake_t *f);
int  fake_sink(const char *buf, size_t len, void *user);

/* Number of times needle occurs in what was written. */
int  fake_count(const fake_t *f, const char *needle);
int  fake_has(const fake_t *f, const char *needle);

/**
 * @brief As fake_has, but over the output with its escape sequences removed
 *
 * A row that changes colour part-way through — the value column, or the value
 * inside an editor prompt — puts an SGR sequence in the middle of the text, so
 * the characters a user reads are not contiguous bytes on the wire. Assertions
 * about what the screen says use this; assertions about what went over the wire
 * use fake_has.
 */
int  fake_has_text(const fake_t *f, const char *needle);

#endif /* FAKE_SINK_H */
