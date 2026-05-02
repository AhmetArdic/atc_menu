/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file testing.h
 * @brief Tiny single-binary assertion helpers for CTest.
 *
 * Each test is its own executable that returns 0 on success or 1 on
 * failure. EXPECT macros accumulate failures rather than aborting so a
 * single run can report multiple problems.
 */

#ifndef TESTING_H
#define TESTING_H

#include "atc_menu/atc_menu.h"

#include <stdio.h>
#include <string.h>

/* Wrap an item array in a static atc_menu_table_t and call atc_menu_init.
 * Each call site gets its own static thanks to lexical scoping of `static`
 * inside a block; the table outlives the framework's stored pointer. */
#define ATC_INIT_ITEMS(items_arr, port)                                    \
    do {                                                                   \
        static const atc_menu_table_t _atc_tbl_ = {                        \
            .items = (items_arr),                                          \
            .count = sizeof(items_arr) / sizeof((items_arr)[0]),           \
        };                                                                 \
        atc_menu_init(&_atc_tbl_, (port));                                 \
    } while (0)

static int testing_failures_;

#define EXPECT(cond)                                                       \
    do {                                                                   \
        if (!(cond)) {                                                     \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);\
            testing_failures_++;                                           \
        }                                                                  \
    } while (0)

#define EXPECT_CONTAINS(buf, needle)                                       \
    do {                                                                   \
        if (strstr((buf), (needle)) == NULL) {                             \
            fprintf(stderr, "FAIL %s:%d: missing %s in output\n",          \
                    __FILE__, __LINE__, #needle);                          \
            testing_failures_++;                                           \
        }                                                                  \
    } while (0)

#define EXPECT_NOT_CONTAINS(buf, needle)                                   \
    do {                                                                   \
        if (strstr((buf), (needle)) != NULL) {                             \
            fprintf(stderr, "FAIL %s:%d: unexpected %s in output\n",       \
                    __FILE__, __LINE__, #needle);                          \
            testing_failures_++;                                           \
        }                                                                  \
    } while (0)

#define EXPECT_STREQ(a, b)                                                 \
    do {                                                                   \
        if (strcmp((a), (b)) != 0) {                                       \
            fprintf(stderr, "FAIL %s:%d: %s != %s ('%s' vs '%s')\n",       \
                    __FILE__, __LINE__, #a, #b, (a), (b));                 \
            testing_failures_++;                                           \
        }                                                                  \
    } while (0)

#define TEST_RESULT()                                                      \
    do {                                                                   \
        if (testing_failures_) {                                           \
            fprintf(stderr, "%d failure(s)\n", testing_failures_);         \
            return 1;                                                      \
        }                                                                  \
        return 0;                                                          \
    } while (0)

#endif /* TESTING_H */
