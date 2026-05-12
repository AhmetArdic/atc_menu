/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "atc_menu/atc_menu.h"
#include "atc_menu/atc_menu_macros.h"
#include "mock_port.h"
#include "testing.h"

static void rd_dummy(char *b, size_t n, atc_status_t *st) {
    (void)b; (void)n; (void)st;
}

/* Static (Flash-resident) leaf authored with the macro layer. */
ATC_MENU(static_leaf, ATC_NO_NOTES,
    ATC_GROUP("Static Leaf"),
    ATC_VALUE('s', "StaticVal", "u", rd_dummy),
);

/* Dynamic (RAM-resident) leaf authored with the runtime builder. */
static atc_menu_item_t  dyn_leaf_items[2];
static atc_menu_table_t dyn_leaf;

static void build_dyn_leaf(void) {
    atc_menu_begin(&dyn_leaf, dyn_leaf_items, 2);
    atc_menu_group(&dyn_leaf, "Dyn Leaf");
    atc_menu_value(&dyn_leaf, 'd', "DynVal", "u", rd_dummy);
    atc_menu_end(&dyn_leaf);
}

/* Dynamic root that mixes a static submenu and a dynamic submenu. */
static atc_menu_item_t  root_items[4];
static atc_menu_table_t root;

static void build_root(void) {
    atc_menu_begin   (&root, root_items, 4);
    atc_menu_group   (&root, "Mixed Root");
    atc_menu_submenu (&root, '1', "ToStatic", &static_leaf);
    atc_menu_submenu (&root, '2', "ToDyn",    &dyn_leaf);
    atc_menu_end     (&root);
}

int main(void) {
    build_dyn_leaf();
    build_root();

    atc_menu_init(&root, &mock_port, NULL);

    /* Root renders both submenu rows. */
    mock_reset();
    atc_menu_render();
    EXPECT_CONTAINS(mock_buffer(), "Mixed Root");
    EXPECT_CONTAINS(mock_buffer(), "ToStatic");
    EXPECT_CONTAINS(mock_buffer(), "ToDyn");

    /* Drill into the static leaf. */
    mock_reset();
    atc_menu_handle_key('1');
    EXPECT_CONTAINS(mock_buffer(), "Static Leaf");
    EXPECT_CONTAINS(mock_buffer(), "StaticVal");

    /* Back up to root. */
    mock_reset();
    atc_menu_handle_key('b');
    EXPECT_CONTAINS(mock_buffer(), "Mixed Root");

    /* Drill into the dynamic leaf. */
    mock_reset();
    atc_menu_handle_key('2');
    EXPECT_CONTAINS(mock_buffer(), "Dyn Leaf");
    EXPECT_CONTAINS(mock_buffer(), "DynVal");

    /* Back to root. */
    mock_reset();
    atc_menu_handle_key('b');
    EXPECT_CONTAINS(mock_buffer(), "Mixed Root");

    /* Rebuilding the dynamic leaf at runtime is reflected on next visit. */
    atc_menu_begin(&dyn_leaf, dyn_leaf_items, 2);
    atc_menu_group(&dyn_leaf, "Rebuilt");
    atc_menu_value(&dyn_leaf, 'd', "Updated", "u", rd_dummy);
    atc_menu_end(&dyn_leaf);

    mock_reset();
    atc_menu_handle_key('2');
    EXPECT_CONTAINS(mock_buffer(), "Rebuilt");
    EXPECT_CONTAINS(mock_buffer(), "Updated");
    EXPECT_NOT_CONTAINS(mock_buffer(), "Dyn Leaf");

    TEST_RESULT();
}
