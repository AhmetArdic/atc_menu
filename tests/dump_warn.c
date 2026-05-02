/* Verifies that init validation warns on every reserved system key. */
#include "atc_menu/atc_menu.h"
#include "mock_port.h"

#include <stdio.h>

static void rd_dummy(char *b, size_t n, atc_status_t *st) {
    (void)b; (void)n; *st = ATC_ST_OK;
}

static const atc_menu_item_t bad[] = {
    { .type = ATC_ROW_VALUE, .key = 'r', .label = "Refresh-clash", .read = rd_dummy },
    { .type = ATC_ROW_VALUE, .key = 'b', .label = "Back-clash",    .read = rd_dummy },
    { .type = ATC_ROW_VALUE, .key = '?', .label = "Path-clash",    .read = rd_dummy },
    { .type = ATC_ROW_VALUE, .key = ':', .label = "Cmd-clash",     .read = rd_dummy },
    { .type = ATC_ROW_VALUE, .key = 'x', .label = "OK-key",        .read = rd_dummy },
};

int main(void) {
    static const atc_menu_table_t bad_tbl = {
        .items = bad, .count = sizeof bad / sizeof bad[0],
    };
    mock_reset();
    atc_menu_init(&bad_tbl, &mock_port);
    fputs(mock_buffer(), stdout);
    return 0;
}
