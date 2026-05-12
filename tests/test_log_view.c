/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "atc_menu/atc_menu.h"
#include "atc_menu/atc_menu_macros.h"
#include "atc_menu/log_ring.h"
#include "mock_port.h"
#include "testing.h"

static char           storage[4][32];
static atc_log_ring_t event_log;

ATC_MENU(menu, ATC_NO_NOTES,
    ATC_GROUP   ("Test"),
    ATC_LOG_VIEW('L', "Events", &event_log),
);

int main(void) {
    atc_menu_log_init(&event_log, &storage[0][0], 4, 32);
    atc_menu_log_push(&event_log, "boot");
    atc_menu_log_push(&event_log, "ready");
    atc_menu_log_push(&event_log, "tick");

    mock_reset();
    atc_menu_init(&menu, &mock_port, NULL);
    atc_menu_render();
    EXPECT_CONTAINS(mock_buffer(), "Events");
    EXPECT_NOT_CONTAINS(mock_buffer(), "EVENT LOG");

    /* Pressing the row key opens the fullscreen log view. */
    mock_reset();
    atc_menu_handle_key('L');
    EXPECT_CONTAINS(mock_buffer(), "EVENT LOG");
    EXPECT_CONTAINS(mock_buffer(), "boot");
    EXPECT_CONTAINS(mock_buffer(), "ready");
    EXPECT_CONTAINS(mock_buffer(), "tick");
    EXPECT_CONTAINS(mock_buffer(), "offset=0");

    /* k scrolls back one line. */
    mock_reset();
    atc_menu_handle_key('k');
    EXPECT_CONTAINS(mock_buffer(), "offset=1");

    /* g jumps to the oldest entry. */
    mock_reset();
    atc_menu_handle_key('g');
    EXPECT_CONTAINS(mock_buffer(), "offset=2");

    /* G jumps back to the bottom (auto-follow). */
    mock_reset();
    atc_menu_handle_key('G');
    EXPECT_CONTAINS(mock_buffer(), "offset=0");

    /* New pushes are visible once the view re-renders. */
    atc_menu_log_push(&event_log, "later");
    mock_reset();
    atc_menu_render();
    EXPECT_CONTAINS(mock_buffer(), "later");

    /* q exits and brings the menu back. */
    mock_reset();
    atc_menu_handle_key('q');
    EXPECT_CONTAINS(mock_buffer(), "Events");
    EXPECT_NOT_CONTAINS(mock_buffer(), "EVENT LOG");

    TEST_RESULT();
}
