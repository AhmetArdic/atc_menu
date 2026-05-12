/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "atc_menu/log_ring.h"
#include "testing.h"

int main(void) {
    /* Basic init state. */
    char           buf[4][16];
    atc_log_ring_t r;
    atc_menu_log_init(&r, &buf[0][0], 4, 16);

    EXPECT(r.count == 0);
    EXPECT(r.head  == 0);
    EXPECT(r.seq   == 0);
    EXPECT(atc_menu_log_get(&r, 0) == NULL);

    /* Single push. */
    atc_menu_log_push(&r, "one");
    EXPECT(r.count == 1);
    EXPECT(r.head  == 1);
    EXPECT(r.seq   == 1);
    EXPECT_STREQ(atc_menu_log_get(&r, 0), "one");
    EXPECT(atc_menu_log_get(&r, 1) == NULL);

    /* Fill exactly. */
    atc_menu_log_push(&r, "two");
    atc_menu_log_push(&r, "three");
    atc_menu_log_push(&r, "four");
    EXPECT(r.count == 4);
    EXPECT(r.head  == 0);
    EXPECT(r.seq   == 4);
    EXPECT_STREQ(atc_menu_log_get(&r, 0), "four");
    EXPECT_STREQ(atc_menu_log_get(&r, 1), "three");
    EXPECT_STREQ(atc_menu_log_get(&r, 2), "two");
    EXPECT_STREQ(atc_menu_log_get(&r, 3), "one");
    EXPECT(atc_menu_log_get(&r, 4) == NULL);

    /* Wrap-around: oldest gets overwritten. */
    atc_menu_log_push(&r, "five");
    EXPECT(r.count == 4);
    EXPECT(r.head  == 1);
    EXPECT(r.seq   == 5);
    EXPECT_STREQ(atc_menu_log_get(&r, 0), "five");
    EXPECT_STREQ(atc_menu_log_get(&r, 1), "four");
    EXPECT_STREQ(atc_menu_log_get(&r, 2), "three");
    EXPECT_STREQ(atc_menu_log_get(&r, 3), "two");
    EXPECT(atc_menu_log_get(&r, 4) == NULL);

    /* Truncation to cols-1. */
    char           small_storage[2][8];
    atc_log_ring_t s;
    atc_menu_log_init(&s, &small_storage[0][0], 2, 8);
    atc_menu_log_push(&s, "1234567890");
    EXPECT_STREQ(atc_menu_log_get(&s, 0), "1234567");

    /* printf path: composition + seq increment. */
    char           pbuf[2][32];
    atc_log_ring_t p;
    atc_menu_log_init(&p, &pbuf[0][0], 2, 32);
    int n = atc_menu_log_printf(&p, "x=%d y=%s", 42, "hi");
    EXPECT(n == 9);
    EXPECT_STREQ(atc_menu_log_get(&p, 0), "x=42 y=hi");
    EXPECT(p.seq == 1);

    /* NULL push is stored as empty string. */
    atc_menu_log_push(&p, NULL);
    EXPECT_STREQ(atc_menu_log_get(&p, 0), "");

    /* Re-init resets all state. */
    atc_menu_log_init(&r, &buf[0][0], 4, 16);
    EXPECT(r.count == 0);
    EXPECT(r.head  == 0);
    EXPECT(r.seq   == 0);
    EXPECT(atc_menu_log_get(&r, 0) == NULL);

    TEST_RESULT();
}
