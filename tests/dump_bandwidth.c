/* Bandwidth probe — measures bytes emitted by interactive transitions. */

#include "atc_menu/menu.h"
#include "mock_port.h"

#include <stdint.h>
#include <stdio.h>

static int g_pct = 50;
static void rd_bar(char *b, size_t n, atc_menu_status_t *st) {
    snprintf(b, n, "%d", g_pct); *st = ATC_MENU_ST_OK;
}
static void rd_temp(char *b, size_t n, atc_menu_status_t *st) {
    snprintf(b, n, "24.6"); *st = ATC_MENU_ST_OK;
}
static void rd_led(char *b, size_t n, atc_menu_status_t *st) {
    (void)b; (void)n; *st = ATC_MENU_ST_ON;
}
static const char *modes[] = { "ECO", "NORMAL", "TURBO" };
static uint_least8_t mode_idx = 0;
static void noop(void) {}
static int32_t pwm = 50;
static void rd_pwm(char *b, size_t n, atc_menu_status_t *st) {
    snprintf(b, n, "%ld", (long)pwm); *st = ATC_MENU_ST_OK;
}
static bool commit_pwm(const char *s) { (void)s; return true; }

static const atc_menu_item_t items[] = {
    { .type = ATC_MENU_ROW_GROUP, .label = "Sensors" },
    { .type = ATC_MENU_ROW_VALUE,  .key = 't', .label = "Temp",   .unit = "C",
      .read = rd_temp },
    { .type = ATC_MENU_ROW_STATE,  .key = 'L', .label = "LED",    .unit = "",
      .read = rd_led, .action = noop },
    { .type = ATC_MENU_ROW_BAR,    .key = 'B', .label = "Battery", .read = rd_bar },
    { .type = ATC_MENU_ROW_CHOICE, .key = 'm', .label = "Mode",
      .choices = modes, .choice_count = 3, .choice_idx = &mode_idx,
      .choice_commit = noop },
    { .type = ATC_MENU_ROW_INPUT,  .key = 'd', .label = "PWM Duty", .unit = "%",
      .read = rd_pwm, .input_type = ATC_MENU_INPUT_INT,
      .input_min = 0, .input_max = 100, .input_commit = commit_pwm },
};
static const atc_menu_table_t tbl = {
    .items = items, .count = sizeof items / sizeof items[0],
};

static void label(const char *s, size_t bytes) {
    fprintf(stderr, "%-44s %5zu bytes\n", s, bytes);
}

int main(void) {
    atc_menu_init(&tbl, &mock_port, NULL);

    mock_reset();
    atc_menu_render();
    label("baseline: full atc_menu_render", mock_len());

    /* CHOICE: enter edit (with commit cb), cycle, commit. */
    mock_reset();
    atc_menu_handle_key('m');
    label("CHOICE: enter edit mode ('m')", mock_len());

    mock_reset();
    atc_menu_handle_key('m');
    label("CHOICE: cycle pending ('m')", mock_len());

    mock_reset();
    atc_menu_handle_key('\r');
    label("CHOICE: commit (Enter)", mock_len());

    mock_reset();
    atc_menu_handle_key('m');
    atc_menu_handle_key(27);
    label("CHOICE: enter+cancel (m + Esc)", mock_len());

    /* INPUT: enter, type, commit. */
    mock_reset();
    atc_menu_handle_key('d');
    label("INPUT: enter edit mode ('d')", mock_len());

    mock_reset();
    atc_menu_handle_key('7');
    label("INPUT: type one char ('7')", mock_len());

    mock_reset();
    atc_menu_handle_key(8);
    label("INPUT: backspace", mock_len());

    mock_reset();
    atc_menu_handle_key('7');
    atc_menu_handle_key('5');
    mock_reset();
    atc_menu_handle_key('\r');
    label("INPUT: commit (Enter)", mock_len());

    mock_reset();
    atc_menu_handle_key('d');
    atc_menu_handle_key(27);
    label("INPUT: enter+cancel (d + Esc)", mock_len());

    /* Default scalar widget on_key (action). */
    mock_reset();
    atc_menu_handle_key('L');
    label("STATE: toggle ('L')", mock_len());

    return 0;
}
