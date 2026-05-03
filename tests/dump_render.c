/* One-off visual inspection — dumps rendered output to stdout so a human
 * can eyeball the layout. Not a CTest target. */

#include "atc_menu/atc_menu.h"
#include "mock_port.h"

#include <stdint.h>
#include <stdio.h>

static int g_pct = 78;
static void rd_bar(char *b, size_t n, atc_status_t *st) {
    snprintf(b, n, "%d", g_pct);
    *st = ATC_ST_OK;
}

static void rd_temp(char *b, size_t n, atc_status_t *st) {
    snprintf(b, n, "24.6");
    *st = ATC_ST_OK;
}

static void rd_led(char *b, size_t n, atc_status_t *st) {
    (void)b; (void)n; *st = ATC_ST_ON;
}

static const char *modes[]  = { "ECO", "NORMAL", "TURBO" };
static uint8_t     mode_idx = 1;

static int32_t pwm = 50;
static void rd_pwm(char *b, size_t n, atc_status_t *st) {
    snprintf(b, n, "%ld", (long)pwm); *st = ATC_ST_OK;
}
static bool commit_pwm(const char *s) { (void)s; return true; }

static const atc_menu_item_t leaf[] = {
    { .type = ATC_ROW_GROUP, .label = "Inner sub-page" },
};
static const atc_menu_table_t leaf_tbl = {
    .items = leaf, .count = 1,
};

static const atc_menu_item_t items[] = {
    { .type = ATC_ROW_GROUP, .label = "Sensors" },
    { .type = ATC_ROW_VALUE,   .key = 't', .label = "Temperature", .unit = "C", .read = rd_temp },
    { .type = ATC_ROW_STATE,   .key = 'L', .label = "LED",          .unit = "",  .read = rd_led },
    { .type = ATC_ROW_GROUP,   .label = "Levels (BAR)" },
    { .type = ATC_ROW_BAR,     .key = 'B', .label = "Battery",      .read = rd_bar },
    { .type = ATC_ROW_GROUP,   .label = "Modes (CHOICE)" },
    { .type = ATC_ROW_CHOICE,  .key = 'm', .label = "Power Mode",
      .choices = modes, .choice_count = 3, .choice_idx = &mode_idx },
    { .type = ATC_ROW_GROUP,   .label = "Setpoints (INPUT)" },
    { .type = ATC_ROW_INPUT,   .key = 'd', .label = "PWM Duty", .unit = "%",
      .read = rd_pwm, .input_type = ATC_INPUT_INT,
      .input_min = 0, .input_max = 100, .input_commit = commit_pwm },
    { .type = ATC_ROW_GROUP,   .label = "Drill into" },
    { .type = ATC_ROW_SUBMENU, .key = 'i', .label = "Inner page",
      .submenu = &leaf_tbl },
};
static const char *const items_notes[] = {
    "Kitchen-sink demo: every row widget on one screen.",
    "Press hotkeys to interact; 'b' pops back from sub-pages.",
};
static const atc_menu_table_t items_tbl = {
    .items = items, .count = sizeof items / sizeof items[0],
    .notes = items_notes, .note_count = sizeof items_notes / sizeof items_notes[0],
};

static size_t visible_len(const char *s) {
    size_t n = 0;
    while (*s) {
        if (s[0] == '\x1b' && s[1] == '[') {
            s += 2;
            while (*s && (*s < '@' || *s > '~')) s++;
            if (*s) s++;
            continue;
        }
        if (*s == '\n' || *s == '\r') return n;
        n++;
        s++;
    }
    return n;
}

int main(void) {
    static const atc_menu_info_t info = {
        .project = "ATC Menu", .version = "1.0.0",
    };
    mock_reset();
    atc_menu_init(&items_tbl, &mock_port, &info);
    mock_reset();
    atc_menu_render();
    fputs(mock_buffer(), stdout);

    fprintf(stderr, "\n--- line widths ---\n");
    const char *p = mock_buffer();
    int line = 0;
    while (*p) {
        size_t w = visible_len(p);
        fprintf(stderr, "line %2d: %zu cols\n", line, w);
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
        line++;
    }

    fprintf(stderr, "\n--- newline audit ---\n");
    const char *src = mock_buffer();
    int crlf_count = 0;
    int bare_lf    = 0;
    int bare_cr    = 0;
    for (size_t i = 0; src[i]; i++) {
        if (src[i] == '\n') {
            if (i == 0 || src[i - 1] != '\r') bare_lf++;
            else                              crlf_count++;
        } else if (src[i] == '\r' && src[i + 1] != '\n') {
            bare_cr++;
        }
    }
    fprintf(stderr, "CRLF pairs: %d, bare LF: %d, bare CR: %d\n",
            crlf_count, bare_lf, bare_cr);
    return 0;
}
