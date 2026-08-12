/* SPDX-License-Identifier: MIT */
/**
 * @file test_menu.c
 * @brief Host tests: every assertion is on the bytes the menu emitted
 */
#include "atc_menu/menu.h"
#include "fake_sink.h"

#include <stdio.h>
#include <string.h>

static int failures;
static int checks;

#define CHECK(cond)                                                       \
    do {                                                                  \
        checks++;                                                         \
        if (!(cond)) {                                                    \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            failures++;                                                   \
        }                                                                 \
    } while (0)

#define COLS 80u
/* The chrome a full atc_menu_info_t costs is 8 rows; the library works it out
   from the banner, so these two numbers are the test's own arithmetic. */
#define CHROME 8u
#define MROWS 17u /* 8 chrome rows + 9 item rows */
#define IROWS (MROWS - CHROME) /* 9 */

static fake_t         F;
static atc_menu_ctx_t C;

ATC_MENU_SCREEN(SCREEN, COLS, MROWS);

static const atc_menu_info_t INFO = { "TestApp", "v1.0", "tester" };

/* Numbers are per page, so a case that needs more than IROWS of them needs a
   page tall enough to hold them all. */
#define TALL_MROWS 158u
ATC_MENU_SCREEN(TALL, COLS, TALL_MROWS);

static void setup(void)
{
    fake_reset(&F);
    CHECK(atc_menu_init(&C, &INFO, &SCREEN, fake_sink, &F) == ATC_MENU_OK);
    CHECK(atc_menu_term_begin(&C) == ATC_MENU_OK);
    fake_reset(&F);
}

static void setup_tall(void)
{
    fake_reset(&F);
    CHECK(atc_menu_init(&C, &INFO, &TALL, fake_sink, &F) == ATC_MENU_OK);
    CHECK(atc_menu_term_begin(&C) == ATC_MENU_OK);
    fake_reset(&F);
}

static void keys(const char *s)
{
    while (*s != '\0')
        atc_menu_key(&C, (unsigned char)*s++);
}

/* ---- menus used by the cases ------------------------------------------ */

static unsigned g_n;
static int      g_fired;

static void frame_n_actions(void)
{
    unsigned i;
    char     name[16];

    atc_menu_frame_begin(&C);
    for (i = 0u; i < g_n; ++i) {
        sprintf(name, "Item %u", i + 1u);
        if (atc_menu_action(&C, name))
            g_fired = (int)(i + 1u);
    }
    atc_menu_frame_end(&C);
}

static void run_n(unsigned n)
{
    g_n = n;
    frame_n_actions();
}

/* ---- cases ------------------------------------------------------------ */

static void t_term_begin_bytes(void)
{
    fake_reset(&F);
    CHECK(atc_menu_init(&C, &INFO, &SCREEN, fake_sink, &F) == ATC_MENU_OK);
    CHECK(atc_menu_term_begin(&C) == ATC_MENU_OK);
    CHECK(F.len == strlen("\x1b[?6l\x1b[2J\x1b[?25l\x1b[1;1H"));
    CHECK(memcmp(F.out, "\x1b[?6l\x1b[2J\x1b[?25l\x1b[1;1H", F.len) == 0);

    fake_reset(&F);
    CHECK(atc_menu_term_end(&C) == ATC_MENU_OK);
    CHECK(memcmp(F.out, "\x1b[0m\x1b[?25h\x1b[17;1H", F.len) == 0);
}

static void t_first_frame_bytes(void)
{
    setup();
    run_n(3u);

    CHECK(fake_has(&F, "\x1b" "7"));       /* cursor saved once */
    CHECK(fake_count(&F, "\x1b" "7") == 1);
    CHECK(fake_has(&F, "\x1b" "8"));       /* and restored */
    CHECK(fake_has(&F, "TestApp"));        /* banner */
    CHECK(fake_has(&F, "v1.0"));
    CHECK(fake_has(&F, "tester"));
    CHECK(fake_has(&F, "===="));
    CHECK(fake_has(&F, "Main"));           /* breadcrumb */
    CHECK(fake_has(&F, "\x1b[5;1H"));      /* first item row */
    CHECK(fake_has(&F, "   1   "));
    CHECK(fake_has(&F, "Item 1"));
    CHECK(fake_has(&F, "Item 3"));
    CHECK(fake_has(&F, "\x1b[48;5;236m"));  /* zebra stripe */
    CHECK(fake_has(&F, "\x1b[8;1H"));      /* rule, straight after the 3 items */
    CHECK(fake_has(&F, " Refresh"));
    CHECK(fake_has(&F, "Select> _"));
}

static void t_idle_frame_is_silent(void)
{
    setup();
    run_n(3u);
    CHECK(F.len > 0u);

    fake_reset(&F);
    run_n(3u);
    CHECK(F.len == 0u); /* nothing changed: not one byte on the wire */
}

static void t_prefix_rule_n11(void)
{
    /* unambiguous: 2 cannot be extended because 20 > 11 */
    setup_tall();
    run_n(11u);
    g_fired = 0;
    keys("2");
    run_n(11u);
    CHECK(g_fired == 2);

    /* ambiguous, terminated by Enter */
    setup_tall();
    run_n(11u);
    g_fired = 0;
    keys("1\r");
    run_n(11u);
    CHECK(g_fired == 1);

    /* ambiguous, extended to 11 */
    setup_tall();
    run_n(11u);
    g_fired = 0;
    keys("11");
    run_n(11u);
    CHECK(g_fired == 11);

    /* ambiguous, extended to 10 */
    setup_tall();
    run_n(11u);
    g_fired = 0;
    keys("10");
    run_n(11u);
    CHECK(g_fired == 10);

    /* 15 is invalid, so 5 restarts the prefix and fires at once */
    setup_tall();
    run_n(11u);
    g_fired = 0;
    keys("15");
    run_n(11u);
    CHECK(g_fired == 5);

    /* backspace clears a pending prefix */
    setup_tall();
    run_n(11u);
    g_fired = 0;
    keys("1\b");
    run_n(11u);
    CHECK(g_fired == 0);
}

static void t_prefix_rule_scales(void)
{
    setup_tall();
    run_n(25u);
    g_fired = 0;
    keys("15");
    run_n(25u);
    CHECK(g_fired == 15); /* 15 <= 25 and 150 > 25 */

    setup_tall();
    run_n(150u);
    g_fired = 0;
    keys("123");
    run_n(150u);
    CHECK(g_fired == 123); /* three digits accumulate with no timeout */

    setup_tall();
    run_n(150u);
    g_fired = 0;
    keys("9");
    run_n(150u);
    CHECK(g_fired == 0); /* 9 is still extendable: 90 <= 150 */
    keys("\r");
    run_n(150u);
    CHECK(g_fired == 9);
}

static void t_pending_prefix_is_shown(void)
{
    setup_tall();
    run_n(11u);
    fake_reset(&F);
    keys("1");
    run_n(11u);
    CHECK(fake_has(&F, "Select> 1_"));
}

/* Numbers are handed out per page, so a prefix typed against one page must not
   survive into the next, where it would name a different item. */
static void t_paging_drops_the_pending_prefix(void)
{
    setup_tall();
    run_n(30u);
    atc_menu_set_items_per_page(&C, 15u);
    run_n(30u);

    keys("1"); /* ambiguous: 11..15 could still follow */
    run_n(30u);
    CHECK(fake_has(&F, "Select> 1_"));

    fake_reset(&F);
    keys("n");
    run_n(30u);
    CHECK(fake_has(&F, "(2/2)"));
    CHECK(fake_has(&F, "Select> _")); /* the prefix went with the page */

    g_fired = 0;
    keys("2\r"); /* so this names item 2 of page 2, not 12 of page 1 */
    run_n(30u);
    CHECK(g_fired == 17);

    /* and the same on the way back */
    keys("1");
    run_n(30u);
    fake_reset(&F);
    keys("p");
    run_n(30u);
    CHECK(fake_has(&F, "(1/2)"));
    CHECK(fake_has(&F, "Select> _"));
}

/* ---- submenu tree ----------------------------------------------------- */

static int      g_leaf_hits;
static unsigned g_leaf_n;

static void frame_tree(void)
{
    unsigned i;
    char     name[16];

    atc_menu_frame_begin(&C);
    atc_menu_action(&C, "Top A");
    if (atc_menu_submenu(&C, "Deep")) {
        for (i = 0u; i < g_leaf_n; ++i) {
            sprintf(name, "Leaf %u", i + 1u);
            if (atc_menu_action(&C, name))
                g_leaf_hits = (int)(i + 1u);
        }
        atc_menu_submenu_end(&C);
    }
    atc_menu_action(&C, "Top C");
    atc_menu_frame_end(&C);
}

static void t_submenu_enter_and_back(void)
{
    g_leaf_n = 12u;
    g_leaf_hits = 0;
    setup_tall();
    frame_tree();
    CHECK(fake_has(&F, "   2   \x1b[22;39mDeep"));

    keys("2");
    frame_tree(); /* frame that descends */
    fake_reset(&F);
    frame_tree(); /* first frame of the deep level */
    CHECK(fake_has(&F, "Deep"));
    CHECK(fake_has(&F, "   1   \x1b[22;39mLeaf 1"));
    CHECK(!fake_has(&F, "Top A"));

    keys("11");
    frame_tree();
    CHECK(g_leaf_hits == 11);

    keys("0"); /* back to the root */
    fake_reset(&F);
    frame_tree();
    CHECK(fake_has(&F, "   1   \x1b[22;39mTop A"));
    CHECK(fake_has(&F, "   3   \x1b[22;39mTop C"));
}

static void t_paging_and_restore(void)
{
    g_leaf_n = 12u; /* 12 items, 9 rows: two pages */
    setup();
    frame_tree();
    keys("2");
    frame_tree();
    frame_tree();

    fake_reset(&F);
    keys("n");
    frame_tree();
    CHECK(fake_has(&F, "(2/2)"));
    CHECK(fake_has(&F, "   1   \x1b[22;39mLeaf 10")); /* numbering restarts */
    CHECK(fake_has(&F, "   3   \x1b[22;39mLeaf 12"));

    /* so a number always means "this row of this page" */
    g_leaf_hits = 0;
    keys("3");
    frame_tree();
    CHECK(g_leaf_hits == 12);

    /* leaving and re-entering restores the page */
    keys("0");
    frame_tree();
    keys("2");
    frame_tree();
    fake_reset(&F);
    frame_tree();
    CHECK(fake_has(&F, "(1/2)")); /* a fresh descent starts at page 1 */
    CHECK(fake_has(&F, "Main / Deep"));  /* full breadcrumb */

    keys("p");
    frame_tree();
    CHECK(fake_has(&F, "(1/2)"));
    CHECK(fake_has(&F, "first page")); /* already there, and it says so */

    keys("n");
    frame_tree();
    keys("n");
    frame_tree();
    CHECK(fake_has(&F, "last page"));
}

/* The closing rule follows the last item, so a level that shrinks pulls the
   whole bottom block up and blanks what it used to reach. */
static void t_shrink_pulls_the_footer_up(void)
{
    setup();
    run_n(6u);                                 /* items on rows 5..10 */
    CHECK(fake_has(&F, "\x1b[11;1H"));         /* rule right after them */
    CHECK(fake_has(&F, "\x1b[14;1H"));         /* prompt three rows on */

    fake_reset(&F);
    run_n(2u);                                 /* items on rows 5..6 */
    CHECK(fake_has(&F, "\x1b[7;1H"));          /* rule moved up to 7 */
    CHECK(fake_has(&F, "Select> _"));          /* prompt is now row 10 */
    CHECK(fake_has(&F, "\x1b[11;1H\x1b[m\x1b[K")); /* and 11 is blanked */
    CHECK(fake_has(&F, "\x1b[14;1H\x1b[m\x1b[K"));
    CHECK(!fake_has(&F, "\x1b[16;1H"));        /* already blank, not repainted */
}

/* ---- widgets ---------------------------------------------------------- */

static uint8_t  w_u8 = 42u;
static uint16_t w_u16 = 1500u;
static uint32_t w_u32 = 120000u;
static int16_t  w_i16 = -40;
static int32_t  w_i32 = -120000;
static uint8_t  w_x8 = 0x2Au;
static uint16_t w_x16 = 0x05DCu;
static uint32_t w_x32 = 0x0001D4C0u;
static int32_t  w_fix = 23450;
static bool     w_bool = true;
static unsigned w_choice;
static int      w_changed;

static const char *const MODES[3] = { "Auto", "Manual", "Off" };

static void frame_widgets(void)
{
    atc_menu_frame_begin(&C);
    atc_menu_label(&C, "Group");
    w_changed = 0;
    if (atc_menu_uint8(&C, "U8", &w_u8)) w_changed = 1;
    if (atc_menu_uint16(&C, "U16", &w_u16)) w_changed = 2;
    if (atc_menu_uint32(&C, "U32", &w_u32)) w_changed = 3;
    if (atc_menu_int16(&C, "I16", &w_i16)) w_changed = 4;
    if (atc_menu_int32(&C, "I32", &w_i32)) w_changed = 5;
    if (atc_menu_hex8(&C, "X8", &w_x8)) w_changed = 6;
    if (atc_menu_hex16(&C, "X16", &w_x16)) w_changed = 7;
    if (atc_menu_hex32(&C, "X32", &w_x32)) w_changed = 8;
    atc_menu_frame_end(&C);
}

static void frame_widgets2(void)
{
    atc_menu_frame_begin(&C);
    w_changed = 0;
    if (atc_menu_fix(&C, "Fix", &w_fix, 3u)) w_changed = 1;
    if (atc_menu_bool(&C, "Bool", &w_bool)) w_changed = 2;
    if (atc_menu_choice(&C, "Mode", &w_choice, MODES, 3u)) w_changed = 3;
    atc_menu_uint8_ro(&C, "RoU8", 7u);
    atc_menu_int16_ro(&C, "RoI16", -7);
    atc_menu_int32_ro(&C, "RoI32", -70000);
    atc_menu_hex8_ro(&C, "RoX8", 0xABu);
    atc_menu_hex16_ro(&C, "RoX16", 0xBEEFu);
    atc_menu_hex32_ro(&C, "RoX32", 0xDEADBEEFu);
    atc_menu_uint32_ro(&C, "RoU32", 4000000000u);
    atc_menu_bool_ro(&C, "RoBool", false);
    atc_menu_text_ro(&C, "Serial", "A7F3-0091");
    atc_menu_fix_ro(&C, "RoFix", -1234, 2u);
    atc_menu_frame_end(&C);
}

static void t_widget_rendering(void)
{
    setup();
    frame_widgets();
    CHECK(fake_has(&F, "Group"));
    CHECK(fake_has(&F, "42"));
    CHECK(fake_has(&F, "1500"));
    CHECK(fake_has(&F, "120000"));
    CHECK(fake_has(&F, "-40"));
    CHECK(fake_has(&F, "-120000"));
    CHECK(fake_has(&F, "0x2A"));
    CHECK(fake_has(&F, "0x05DC"));
    CHECK(fake_has(&F, "0x0001D4C0"));
    /* the label takes a row but no number, so U8 is still number 1 */
    CHECK(fake_has(&F, "       Group"));
    CHECK(fake_has(&F, "   1   \x1b[22;39mU8"));
    CHECK(fake_has(&F, "   8   \x1b[22;39mX32"));

    setup();
    w_choice = 0u;
    frame_widgets2();
    CHECK(fake_has(&F, "23.450"));
    CHECK(fake_has(&F, "[X]"));
    CHECK(fake_has(&F, "Auto"));
    CHECK(fake_has(&F, "0xBEEF"));
    CHECK(fake_has(&F, "0xDEADBEEF"));
    keys("n"); /* items 10..12 sit on the second page */
    frame_widgets2();
    CHECK(fake_has(&F, "[ ]"));
    CHECK(fake_has(&F, "A7F3-0091"));
    CHECK(fake_has(&F, "-12.34"));
    CHECK(fake_has(&F, "0xAB"));
    CHECK(fake_has(&F, "4000000000")); /* above INT32_MAX: must not go negative */
}

static void t_bool_and_choice(void)
{
    setup();
    w_bool = false;
    w_choice = 0u;
    frame_widgets2();

    keys("2"); /* Bool is item 2 */
    frame_widgets2();
    CHECK(w_bool == true);
    CHECK(w_changed == 2);

    /* A choice previews rather than writing: the row keeps the committed
       option and the prompt carries the candidate until Enter. */
    keys("3"); /* Mode */
    frame_widgets2();
    CHECK(atc_menu_editing(&C));
    CHECK(w_choice == 0u);                     /* nothing written yet */
    CHECK(fake_has_text(&F, "Mode [Manual]> _"));   /* opened on the next option */
    CHECK(fake_has(&F, "Auto"));               /* the row still says Auto */

    keys("\r");
    frame_widgets2();
    CHECK(w_choice == 1u);
    CHECK(w_changed == 3);
    CHECK(!atc_menu_editing(&C));

    /* any printable steps forward, and it wraps */
    keys("3");
    frame_widgets2();
    CHECK(fake_has_text(&F, "Mode [Off]> _"));
    keys("x");
    frame_widgets2();
    CHECK(fake_has_text(&F, "Mode [Auto]> _"));
    keys("\r");
    frame_widgets2();
    CHECK(w_choice == 0u);

    /* backspace steps back, and wraps the other way */
    keys("3");
    frame_widgets2();
    CHECK(fake_has_text(&F, "Mode [Manual]> _"));
    keys("\b");
    frame_widgets2();
    CHECK(fake_has_text(&F, "Mode [Auto]> _"));
    keys("\b");
    frame_widgets2();
    CHECK(fake_has_text(&F, "Mode [Off]> _"));

    /* ESC leaves the committed option alone */
    keys("\x1b");
    frame_widgets2();
    CHECK(w_choice == 0u);
    CHECK(!atc_menu_editing(&C));
}

static void t_numeric_edit_commit_and_cancel(void)
{
    setup();
    w_u16 = 1500u;
    frame_widgets();

    keys("2"); /* U16 */
    frame_widgets();
    CHECK(atc_menu_editing(&C));

    keys("1234");
    frame_widgets();
    CHECK(fake_has_text(&F, "U16 [1500]> 1234_"));
    CHECK(w_u16 == 1500u); /* not written before Enter */

    keys("\r");
    frame_widgets();
    CHECK(w_u16 == 1234u);
    CHECK(w_changed == 2);
    CHECK(!atc_menu_editing(&C));

    /* ESC leaves the value alone */
    keys("2");
    frame_widgets();
    keys("999\x1b");
    frame_widgets();
    CHECK(w_u16 == 1234u);
    CHECK(!atc_menu_editing(&C));

    /* backspace inside the editor */
    keys("2");
    frame_widgets();
    keys("999\b\r");
    frame_widgets();
    CHECK(w_u16 == 99u);
}

static void t_type_clamps(void)
{
    setup();
    w_u8 = 10u;
    frame_widgets();
    keys("1"); /* U8 */
    frame_widgets();
    keys("300\r");
    frame_widgets();
    CHECK(w_u8 == 10u); /* refused: 300 does not fit uint8 */
    CHECK(fake_has(&F, "out of range"));
    CHECK(atc_menu_editing(&C)); /* and the entry is still there to correct */

    keys("\x1b"); /* so each case below starts from a closed editor */
    w_i16 = 5;
    frame_widgets();
    keys("4"); /* I16 */
    frame_widgets();
    keys("40000\r");
    frame_widgets();
    CHECK(w_i16 == 5);

    keys("\x1b");
    w_u16 = 7u;
    frame_widgets();
    keys("2");
    frame_widgets();
    keys("-1\r");
    frame_widgets();
    CHECK(w_u16 == 7u); /* a sign is refused for an unsigned type */

    /* the boundary value is accepted */
    keys("\x1b");
    w_u8 = 0u;
    frame_widgets();
    keys("1");
    frame_widgets();
    keys("255\r");
    frame_widgets();
    CHECK(w_u8 == 255u);
}

/* A refused value keeps its editor, so the correction is a backspace rather
   than a retype. */
static void t_refused_entry_stays_open(void)
{
    setup();
    w_u8 = 10u;
    frame_widgets();
    keys("1");
    frame_widgets();
    keys("300\r");
    frame_widgets();
    CHECK(w_u8 == 10u);
    CHECK(atc_menu_editing(&C));
    CHECK(fake_has_text(&F, "U8 [10]> 300_")); /* what was typed is still on screen */

    keys("\b\r"); /* 30 fits */
    frame_widgets();
    CHECK(w_u8 == 30u);
    CHECK(!atc_menu_editing(&C));
}

static void t_extremes(void)
{
    setup();
    w_i32 = 0;
    frame_widgets();
    keys("5"); /* I32 */
    frame_widgets();
    keys("-2147483648\r");
    frame_widgets();
    CHECK(w_i32 == (-2147483647 - 1));

    w_u32 = 0u;
    frame_widgets();
    keys("3");
    frame_widgets();
    keys("4294967295\r");
    frame_widgets();
    CHECK(w_u32 == 4294967295u);

    /* one digit past the 32-bit range is refused, not wrapped */
    w_u32 = 1u;
    frame_widgets();
    keys("3");
    frame_widgets();
    keys("99999999999\r");
    frame_widgets();
    CHECK(w_u32 == 1u);
    keys("\x1b");
}

static void t_hex_entry(void)
{
    setup();
    w_x16 = 0u;
    frame_widgets();
    keys("7"); /* X16 */
    frame_widgets();
    keys("beef\r");
    frame_widgets();
    CHECK(w_x16 == 0xBEEFu);

    w_x8 = 0u;
    frame_widgets();
    keys("6");
    frame_widgets();
    keys("A5\r");
    frame_widgets();
    CHECK(w_x8 == 0xA5u);

    w_x8 = 1u;
    frame_widgets();
    keys("6");
    frame_widgets();
    keys("1FF\r");
    frame_widgets();
    CHECK(w_x8 == 1u); /* 0x1FF does not fit uint8 */
    keys("\x1b");
}

static void t_editor_starts_empty(void)
{
    setup();

    /* decimal */
    w_u16 = 1500u;
    frame_widgets();
    keys("2");
    frame_widgets();
    CHECK(atc_menu_editing(&C));
    /* The bracket is the old value; what follows the caret is what was typed,
       and that starts empty. */
    CHECK(fake_has_text(&F, "U16 [1500]> _"));
    keys("7");
    frame_widgets();
    CHECK(fake_has_text(&F, "U16 [1500]> 7_"));
    keys("\b");
    frame_widgets();
    CHECK(fake_has_text(&F, "U16 [1500]> _"));  /* backspace gets back to empty */
    keys("\x1b");
    frame_widgets();

    /* hex */
    w_x16 = 0x1234u;
    frame_widgets();
    keys("7");
    frame_widgets();
    CHECK(fake_has_text(&F, "X16 [0x1234]> _"));
    keys("a");
    frame_widgets();
    CHECK(fake_has_text(&F, "X16 [0x1234]> 0xA_"));
    keys("\x1b");
    frame_widgets();

    /* a lone sign shows, with nothing after it */
    w_i32 = 42;
    frame_widgets();
    keys("5");
    frame_widgets();
    keys("-");
    frame_widgets();
    CHECK(fake_has_text(&F, "I32 [42]> -_"));
    keys("\x1b");
    frame_widgets();
}

/* The application's own validation gets the same second chance the library's
   range check does: the editor comes back over what was typed. */
static void t_reject_reopens_the_editor(void)
{
    setup();
    w_u16 = 100u;
    frame_widgets();
    keys("2");
    frame_widgets();
    keys("7\r");

    atc_menu_frame_begin(&C);
    atc_menu_label(&C, "Group");
    if (atc_menu_uint8(&C, "U8", &w_u8)) { }
    if (atc_menu_uint16(&C, "U16", &w_u16))
        atc_menu_reject(&C, "too small");
    atc_menu_frame_end(&C);

    CHECK(atc_menu_editing(&C));
    CHECK(fake_has(&F, "too small"));
    CHECK(fake_has_text(&F, "U16 [100]> 7_")); /* the entry survived the refusal */

    /* correcting it and committing again goes through */
    keys("00\r");
    frame_widgets();
    CHECK(w_u16 == 700u);
    CHECK(!atc_menu_editing(&C));

    /* a reject with no value in flight is ignored rather than opening one */
    atc_menu_reject(&C, "nothing to refuse");
    CHECK(!atc_menu_editing(&C));
    atc_menu_reject(NULL, "no crash");
}

/* The value inside an editor prompt is the same value, so it wears the same
   colour it had in the column it came from. */
static void t_prompt_value_is_coloured(void)
{
    setup();
    w_u16 = 1500u;
    frame_widgets();
    keys("2");
    fake_reset(&F);
    frame_widgets();
    /* label and brackets in the prompt colour, the value in the value one */
    CHECK(fake_has(&F, "U16 [\x1b[1;32m" "1500\x1b[1;37m]"));
    CHECK(fake_has_text(&F, "U16 [1500]> _"));
    keys("\x1b");
    frame_widgets();

    /* and a choice, whose bracket is the candidate rather than the old value */
    setup();
    w_choice = 0u;
    frame_widgets2();
    keys("3");
    fake_reset(&F);
    frame_widgets2();
    CHECK(fake_has(&F, "Mode [\x1b[1;32m" "Manual\x1b[1;37m]"));
}

/* A number nothing answers to is said out loud; silence reads as a keystroke
   that never arrived. */
static void t_invalid_selection_says_so(void)
{
    setup();
    run_n(6u);
    g_fired = 0;
    fake_reset(&F);

    keys("7");
    run_n(6u);
    CHECK(fake_has_text(&F, "no such item"));
    /* the prompt row is unchanged, so the diff says nothing about it — what
       matters is that no prefix was left pending to redraw */
    CHECK(!fake_has_text(&F, "Select> 7"));
    CHECK(g_fired == 0);

    /* a valid pick clears it again */
    fake_reset(&F);
    keys("3");
    run_n(6u);
    CHECK(g_fired == 3);
    CHECK(!fake_has_text(&F, "no such item"));
}

/* The value column carries the colour on every widget, not just submenus —
   it is the part of the row that changes. */
static void t_value_column_is_coloured(void)
{
    setup();
    w_u8 = 42u;
    fake_reset(&F);
    frame_widgets();
    CHECK(fake_has(&F, "\x1b[1;32m42"));

    setup();
    w_bool = true;
    fake_reset(&F);
    frame_widgets2();
    CHECK(fake_has(&F, "\x1b[1;32m[X]"));
}

/* The prompt names the row it belongs to, so an editor is never anonymous. */
static void t_editor_prompt_names_its_row(void)
{
    setup();
    w_u8 = 3u;
    w_x8 = 0xABu;
    frame_widgets();

    keys("1");
    frame_widgets();
    CHECK(fake_has_text(&F, "U8 [3]> _"));
    keys("\x1b");
    frame_widgets();

    keys("6");
    frame_widgets();
    CHECK(fake_has_text(&F, "X8 [0xAB]> _"));
    keys("\x1b");
    frame_widgets();

    /* and the row itself keeps showing the committed value throughout */
    CHECK(fake_has(&F, "0xAB"));
}

static void t_decimal_point_is_visible(void)
{
    setup();
    w_fix = 0;
    w_choice = 0u;
    frame_widgets2();

    keys("1\r");
    frame_widgets2();
    keys("0");
    frame_widgets2();
    CHECK(fake_has_text(&F, "Fix [0.000]> 0_"));

    keys(".");
    frame_widgets2();
    CHECK(fake_has_text(&F, "Fix [0.000]> 0._"));  /* the point must show at once */

    keys("5");
    frame_widgets2();
    CHECK(fake_has_text(&F, "Fix [0.000]> 0.5_"));

    keys("\r");
    frame_widgets2();
    CHECK(w_fix == 500);                /* 0.5 with three decimals */

    /* and a whole number followed by a point */
    keys("1\r");
    frame_widgets2();
    keys("5.");
    frame_widgets2();
    CHECK(fake_has_text(&F, "Fix [0.500]> 5._"));
    keys("0\r");
    frame_widgets2();
    CHECK(w_fix == 5000);
}

static void t_fixed_point(void)
{
    setup();
    w_fix = 0;
    w_choice = 0u;
    frame_widgets2();
    keys("1\r");
    frame_widgets2();
    keys("12.345\r");
    frame_widgets2();
    CHECK(w_fix == 12345);

    /* fewer fraction digits than declared are scaled up */
    frame_widgets2();
    keys("1\r");
    frame_widgets2();
    keys("7.5\r");
    frame_widgets2();
    CHECK(w_fix == 7500);

    /* no decimal point at all */
    frame_widgets2();
    keys("1\r");
    frame_widgets2();
    keys("9\r");
    frame_widgets2();
    CHECK(w_fix == 9000);

    /* negative */
    frame_widgets2();
    keys("1\r");
    frame_widgets2();
    keys("-1.5\r");
    frame_widgets2();
    CHECK(w_fix == -1500);
}

/* ---- disabled items --------------------------------------------------- */

static bool g_enabled;
static int  g_dis_fired;

static void frame_disabled(void)
{
    atc_menu_frame_begin(&C);
    atc_menu_action(&C, "One");
    atc_menu_item_enable(&C, g_enabled);
    if (atc_menu_action(&C, "Two"))
        g_dis_fired = 2;
    if (atc_menu_action(&C, "Three"))
        g_dis_fired = 3;
    atc_menu_frame_end(&C);
}

static void frame_separated(void)
{
    atc_menu_frame_begin(&C);
    atc_menu_action(&C, "One");
    atc_menu_separator(&C);
    if (atc_menu_action(&C, "Two"))
        g_dis_fired = 2;
    atc_menu_frame_end(&C);
}

static void t_separator(void)
{
    setup();
    frame_separated();
    CHECK(fake_has(&F, "   1   \x1b[22;39mOne"));
    CHECK(fake_has(&F, "   ---"));       /* a rule, indented by three */
    CHECK(fake_has(&F, "   2   \x1b[22;39mTwo"));   /* takes a row, not a number */

    g_dis_fired = 0;
    keys("2");
    frame_separated();
    CHECK(g_dis_fired == 2);
}

static void t_disabled_item(void)
{
    setup();
    g_enabled = false;
    g_dis_fired = 0;
    frame_disabled();
    CHECK(fake_has(&F, "\x1b[2m"));     /* drawn dim */
    CHECK(fake_has(&F, "   2   Two"));   /* keeps its number ... */
    CHECK(fake_has(&F, "   3   \x1b[22;39mThree")); /* ... so nothing below shifts */

    keys("2");
    frame_disabled();
    CHECK(g_dis_fired == 0);

    keys("3");
    frame_disabled();
    CHECK(g_dis_fired == 3);

    g_enabled = true;
    frame_disabled();
    keys("2");
    frame_disabled();
    CHECK(g_dis_fired == 2);
}

/* Documented, and easy to "fix" into something half-working: a bool writes as
   it toggles, so there is no delivered value for reject to take back. */
static void t_reject_does_nothing_after_a_bool(void)
{
    bool v = false;

    setup();
    atc_menu_frame_begin(&C);
    if (atc_menu_bool(&C, "Flag", &v))
        atc_menu_reject(&C, "not allowed");
    atc_menu_frame_end(&C);

    keys("1");
    fake_reset(&F);
    atc_menu_frame_begin(&C);
    if (atc_menu_bool(&C, "Flag", &v))
        atc_menu_reject(&C, "not allowed");
    CHECK(atc_menu_frame_end(&C) == ATC_MENU_OK);

    CHECK(v);                                /* the toggle stands */
    CHECK(!fake_has(&F, "not allowed"));     /* and the reason never shows */
    CHECK(!atc_menu_editing(&C));            /* nothing was reopened */
}

/* A separator is an item as far as the modifiers are concerned: it spends one
   without showing it. Letting it pass the flag through instead is the same leak
   the level-crossing case above is about. */
static void t_separator_spends_the_modifier(void)
{
    setup();
    atc_menu_frame_begin(&C);
    atc_menu_item_enable(&C, false);
    atc_menu_separator(&C);
    atc_menu_action(&C, "Next");
    CHECK(atc_menu_frame_end(&C) == ATC_MENU_OK);
    CHECK(!fake_has(&F, "\x1b[2m"));

    setup();
    atc_menu_frame_begin(&C);
    atc_menu_separator(&C);
    atc_menu_item_enable(&C, false);
    atc_menu_action(&C, "Next");
    CHECK(atc_menu_frame_end(&C) == ATC_MENU_OK);
    CHECK(fake_has(&F, "\x1b[2m"));
}

/* ---- per-item style ----------------------------------------------------- */

static void t_style_paints_label_and_value(void)
{
    setup();
    atc_menu_frame_begin(&C);
    atc_menu_item_style(&C, ATC_MENU_BOLD | ATC_MENU_ITALIC);
    atc_menu_uint16_ro(&C, "Hz", 1000u);
    CHECK(atc_menu_frame_end(&C) == ATC_MENU_OK);

    CHECK(fake_has(&F, "   1   \x1b[22;39m\x1b[1;3mHz")); /* not in the number */
    CHECK(fake_has(&F, "\x1b[1;32m1000"));                 /* no colour: green */
}

static void t_style_colour_replaces_the_value_green(void)
{
    setup();
    atc_menu_frame_begin(&C);
    atc_menu_item_style(&C, ATC_MENU_BOLD | ATC_MENU_FG_RED);
    atc_menu_text_ro(&C, "State", "FAULT");
    CHECK(atc_menu_frame_end(&C) == ATC_MENU_OK);

    /* once for the label, once to reopen the value column */
    CHECK(fake_count(&F, "\x1b[1;31m") == 2);
    CHECK(fake_has(&F, "\x1b[1;31mFAULT"));
    CHECK(!fake_has(&F, "\x1b[1;32mFAULT"));

    /* no bold: the value column names its own intensity */
    setup();
    atc_menu_frame_begin(&C);
    atc_menu_item_style(&C, ATC_MENU_FG_CYAN);
    atc_menu_text_ro(&C, "State", "IDLE");
    CHECK(atc_menu_frame_end(&C) == ATC_MENU_OK);
    CHECK(fake_has(&F, "\x1b[36mState"));
    CHECK(fake_has(&F, "\x1b[22;36mIDLE"));
}

static void t_style_is_spent_by_one_item(void)
{
    setup();
    atc_menu_frame_begin(&C);
    atc_menu_item_style(&C, ATC_MENU_UNDERLINE);
    atc_menu_action(&C, "One");
    atc_menu_action(&C, "Two");
    CHECK(atc_menu_frame_end(&C) == ATC_MENU_OK);
    CHECK(fake_count(&F, "\x1b[4m") == 1);
}

/* An unstyled row is the row it always was, byte for byte. */
static void t_no_style_changes_nothing(void)
{
    setup();
    atc_menu_frame_begin(&C);
    atc_menu_uint16_ro(&C, "Hz", 1000u);
    CHECK(atc_menu_frame_end(&C) == ATC_MENU_OK);
    CHECK(fake_has(&F,
        "\x1b[5;1H\x1b[m\x1b[48;5;236m\x1b[1;33m   1   \x1b[22;39mHz"));
    CHECK(fake_has(&F, "\x1b[1;32m1000\x1b[m"));
}

static void t_dim_outranks_style(void)
{
    setup();
    atc_menu_frame_begin(&C);
    atc_menu_item_enable(&C, false);
    atc_menu_item_style(&C, ATC_MENU_BOLD | ATC_MENU_FG_RED);
    atc_menu_uint16_ro(&C, "Hz", 1000u);
    CHECK(atc_menu_frame_end(&C) == ATC_MENU_OK);

    CHECK(!fake_has(&F, "\x1b[1;31m"));  /* "not available" wins outright */
    CHECK(fake_has(&F, "\x1b[2m   1   Hz"));
}

static void frame_nested_style(void)
{
    atc_menu_frame_begin(&C);
    atc_menu_item_style(&C, ATC_MENU_UNDERLINE);
    atc_menu_action(&C, "Root");
    if (atc_menu_submenu(&C, "Sub")) {
        atc_menu_action(&C, "Inner");
        atc_menu_submenu_end(&C);
    }
    atc_menu_frame_end(&C);
}

/* A modifier is spent by the next declaration even off the shown level — which
   is what atc_menu_item_enable failed to do before take_disable moved up. */
static void t_style_does_not_cross_a_level(void)
{
    setup();
    frame_nested_style();
    CHECK(fake_count(&F, "\x1b[4m") == 1);

    keys("2"); /* into Sub */
    frame_nested_style();
    atc_menu_refresh(&C);
    fake_reset(&F);
    frame_nested_style();

    CHECK(fake_has(&F, "Inner"));
    CHECK(fake_count(&F, "\x1b[4m") == 0);
}

/* Narrowest screen, widest value, everything on. An overflowing row is refused
   rather than truncated, so frame_end returning OK is the budget check. */
static void t_style_fits_the_row_budget(void)
{
    atc_menu_ctx_t c;
    fake_t         f;

    ATC_MENU_SCREEN(narrow, 23u, 10u);

    fake_reset(&f);
    CHECK(atc_menu_init(&c, &INFO, &narrow, fake_sink, &f) == ATC_MENU_OK);
    CHECK(atc_menu_term_begin(&c) == ATC_MENU_OK);

    atc_menu_frame_begin(&c);
    atc_menu_item_style(&c, ATC_MENU_BOLD | ATC_MENU_ITALIC | ATC_MENU_UNDERLINE |
                            ATC_MENU_REVERSE | ATC_MENU_FG_MAGENTA);
    atc_menu_int32_ro(&c, "L", INT32_MIN);
    /* without bold the value column opens with 22;3x, a byte longer: the real
       worst case */
    atc_menu_item_style(&c, ATC_MENU_ITALIC | ATC_MENU_UNDERLINE |
                            ATC_MENU_REVERSE | ATC_MENU_FG_MAGENTA);
    atc_menu_int32_ro(&c, "L", INT32_MIN);
    CHECK(atc_menu_frame_end(&c) == ATC_MENU_OK);

    CHECK(fake_has(&f, "-2147483648")); /* painted, not dropped */
}

/* ---- editable text ---------------------------------------------------- */

static char g_name[8] = "abc";
static int  g_name_hit;

static void frame_text(void)
{
    atc_menu_frame_begin(&C);
    g_name_hit = 0;
    if (atc_menu_text(&C, "Name", g_name, sizeof g_name))
        g_name_hit = 1;
    atc_menu_uint16_ro(&C, "Ro", 5u);
    atc_menu_frame_end(&C);
}

static void t_text_edit(void)
{
    setup();
    strcpy(g_name, "abc");
    frame_text();
    CHECK(fake_has(&F, "abc"));

    keys("1");
    frame_text();
    CHECK(atc_menu_editing(&C));
    CHECK(fake_has_text(&F, "Name [abc]> _"));   /* empty, not the current name */

    keys("xy");
    frame_text();
    CHECK(fake_has_text(&F, "Name [abc]> xy_"));
    CHECK(strcmp(g_name, "abc") == 0); /* not written before Enter */

    keys("\r");
    frame_text();
    CHECK(strcmp(g_name, "xy") == 0);
    CHECK(g_name_hit == 1);
    CHECK(!atc_menu_editing(&C));

    /* ESC leaves the caller's string alone */
    keys("1");
    frame_text();
    keys("zzz\x1b");
    frame_text();
    CHECK(strcmp(g_name, "xy") == 0);
    CHECK(!atc_menu_editing(&C));

    /* backspace inside the editor */
    keys("1");
    frame_text();
    keys("pqr\b\r");
    frame_text();
    CHECK(strcmp(g_name, "pq") == 0);

    /* a string that will not fit is refused, and says so */
    keys("1");
    frame_text();
    keys("0123456789\r");
    frame_text();
    CHECK(strcmp(g_name, "pq") == 0);
    CHECK(fake_has(&F, "too long"));
}

static void t_unselectable_says_why(void)
{
    setup();
    strcpy(g_name, "abc");
    frame_text();
    keys("2"); /* the read-only row is numbered but cannot be picked */
    frame_text();
    CHECK(fake_has(&F, "read-only"));

    setup();
    g_enabled = false;
    g_dis_fired = 0;
    frame_disabled();
    keys("2");
    frame_disabled();
    CHECK(g_dis_fired == 0);
    CHECK(fake_has(&F, "not available now"));
}

/* ---- sink failures ---------------------------------------------------- */

static void t_sink_refusal_leaves_row_dirty(void)
{
    setup();
    F.refuse_after = 3; /* cursor save + two rows get through */
    CHECK(atc_menu_frame_begin(&C) == ATC_MENU_OK);
    atc_menu_action(&C, "Item 1");
    atc_menu_action(&C, "Item 2");
    atc_menu_action(&C, "Item 3");
    CHECK(atc_menu_frame_end(&C) == ATC_MENU_ERR_IO);

    /* the refused row is repainted from its own ESC next frame */
    F.refuse_after = -1;
    fake_reset(&F);
    run_n(3u);
    CHECK(fake_has(&F, "\x1b[4;1H"));
    CHECK(fake_has(&F, "   3   \x1b[22;39mItem 3"));
}

static void t_sink_always_refuses(void)
{
    setup();
    F.refuse_always = 1;
    run_n(5u);
    run_n(5u); /* must not hang or loop */
    CHECK(F.len == 0u);

    F.refuse_always = 0;
    fake_reset(&F);
    run_n(5u);
    CHECK(fake_has(&F, "   5   \x1b[22;39mItem 5")); /* recovers once the sink accepts */
}

/* ---- argument and state guards ---------------------------------------- */

static void t_param_guards(void)
{
    atc_menu_ctx_t     c;
    atc_menu_screen_t  s;

    CHECK(atc_menu_init(NULL, &INFO, &SCREEN, fake_sink, &F)
          == ATC_MENU_ERR_PARAM);
    CHECK(atc_menu_init(&c, &INFO, NULL, fake_sink, &F)
          == ATC_MENU_ERR_PARAM);
    CHECK(atc_menu_init(&c, &INFO, &SCREEN, NULL, &F)
          == ATC_MENU_ERR_PARAM);

    /* Every field of the descriptor, broken one at a time. */
    s = SCREEN;
    s.buf = NULL;
    CHECK(atc_menu_init(&c, &INFO, &s, fake_sink, &F) == ATC_MENU_ERR_PARAM);

    s = SCREEN;
    s.row_sig = NULL;
    CHECK(atc_menu_init(&c, &INFO, &s, fake_sink, &F) == ATC_MENU_ERR_PARAM);

    s = SCREEN;
    s.buf_cap = 8u; /* too small for one row of COLS */
    CHECK(atc_menu_init(&c, &INFO, &s, fake_sink, &F) == ATC_MENU_ERR_PARAM);

    s = SCREEN;
    s.cols = 12u; /* narrower than the columns a row needs */
    CHECK(atc_menu_init(&c, &INFO, &s, fake_sink, &F) == ATC_MENU_ERR_PARAM);

    s = SCREEN;
    s.rows = CHROME; /* no room for even one item row */
    CHECK(atc_menu_init(&c, &INFO, &s, fake_sink, &F) == ATC_MENU_ERR_PARAM);

    /* and the descriptor the macro built is accepted as it stands */
    CHECK(atc_menu_init(&c, &INFO, &SCREEN, fake_sink, &F) == ATC_MENU_OK);

    /* NULL context must be inert, not fatal */
    atc_menu_key(NULL, 'x');
    atc_menu_refresh(NULL);
    atc_menu_reset(NULL);
    atc_menu_message(NULL, "x");
    atc_menu_label(NULL, "x");
    atc_menu_item_enable(NULL, true);
    CHECK(!atc_menu_editing(NULL));
    CHECK(!atc_menu_action(NULL, "x"));
    CHECK(!atc_menu_submenu(NULL, "x"));
    atc_menu_submenu_end(NULL);
    CHECK(atc_menu_frame_begin(NULL) == ATC_MENU_ERR_PARAM);
    CHECK(atc_menu_frame_end(NULL) == ATC_MENU_ERR_PARAM);
}

static void t_state_guards(void)
{
    setup();

    /* submenu_end without a matching open submenu */
    atc_menu_frame_begin(&C);
    atc_menu_submenu_end(&C);
    CHECK(atc_menu_frame_end(&C) == ATC_MENU_ERR_STATE);

    /* a submenu that returned true but was never closed */
    setup();
    atc_menu_frame_begin(&C);
    atc_menu_action(&C, "A");
    atc_menu_frame_end(&C);
    keys("1");
    atc_menu_frame_begin(&C);
    atc_menu_submenu(&C, "S");
    atc_menu_frame_end(&C);
    atc_menu_frame_begin(&C);
    CHECK(atc_menu_submenu(&C, "S"));
    CHECK(atc_menu_frame_end(&C) == ATC_MENU_ERR_STATE);
}

static void t_depth_overflow(void)
{
    unsigned d;

    setup();
    /* descend as far as the tree allows; nothing may write past path[] */
    for (d = 0u; d < ATC_MENU_MAX_DEPTH + 4u; ++d) {
        unsigned k;
        atc_menu_frame_begin(&C);
        for (k = 0u; k < ATC_MENU_MAX_DEPTH + 4u; ++k) {
            if (!atc_menu_submenu(&C, "S"))
                break;
        }
        while (k-- > 0u)
            atc_menu_submenu_end(&C);
        atc_menu_frame_end(&C);
        keys("1");
    }
    CHECK(C.nav_depth < ATC_MENU_MAX_DEPTH);
}

/* ---- two instances ---------------------------------------------------- */

static void t_two_instances(void)
{
    fake_t         f2;
    atc_menu_ctx_t c2;
    int            hit1 = 0;
    int            hit2 = 0;

    ATC_MENU_SCREEN(second, COLS, MROWS);

    setup();
    fake_reset(&f2);
    CHECK(atc_menu_init(&c2, &INFO, &second, fake_sink, &f2) == ATC_MENU_OK);

    atc_menu_frame_begin(&C);
    atc_menu_action(&C, "One");
    atc_menu_action(&C, "Two");
    atc_menu_frame_end(&C);

    atc_menu_frame_begin(&c2);
    atc_menu_action(&c2, "One");
    atc_menu_action(&c2, "Two");
    atc_menu_frame_end(&c2);

    atc_menu_key(&C, '1');
    atc_menu_key(&c2, '2');

    atc_menu_frame_begin(&C);
    if (atc_menu_action(&C, "One")) hit1 = 1;
    if (atc_menu_action(&C, "Two")) hit1 = 2;
    atc_menu_frame_end(&C);

    atc_menu_frame_begin(&c2);
    if (atc_menu_action(&c2, "One")) hit2 = 1;
    if (atc_menu_action(&c2, "Two")) hit2 = 2;
    atc_menu_frame_end(&c2);

    CHECK(hit1 == 1);
    CHECK(hit2 == 2);
    CHECK(f2.len > 0u);
}

/* ---- caller buffer lifetime ------------------------------------------- */

static void t_text_ro_takes_a_stack_buffer(void)
{
    setup();
    {
        char tmp[16];
        strcpy(tmp, "transient");
        atc_menu_frame_begin(&C);
        atc_menu_text_ro(&C, "T", tmp);
        atc_menu_frame_end(&C);
        memset(tmp, 'x', sizeof tmp); /* the library must not have kept it */
    }
    CHECK(fake_has(&F, "transient"));
}

static void t_refresh_and_reset(void)
{
    setup();
    run_n(3u);
    fake_reset(&F);
    run_n(3u);
    CHECK(F.len == 0u);

    atc_menu_refresh(&C);
    run_n(3u);
    CHECK(fake_has(&F, "   1   \x1b[22;39mItem 1"));

    g_leaf_n = 3u;
    setup();
    frame_tree();
    keys("2");
    frame_tree();
    frame_tree();
    CHECK(C.nav_depth == 1u);
    atc_menu_reset(&C);
    CHECK(C.nav_depth == 0u);
    fake_reset(&F);
    frame_tree();
    CHECK(fake_has(&F, "   1   \x1b[22;39mTop A"));
}

static void t_item_set_changes_completely(void)
{
    setup();
    run_n(20u);
    keys("n");
    run_n(20u);
    run_n(1u); /* the whole level collapses under a scrolled page */
    CHECK(C.top[C.nav_depth] == 0u);
    fake_reset(&F);
    run_n(1u); /* one settling frame: the clamp landed in the previous end */
    CHECK(fake_has(&F, "   1   \x1b[22;39mItem 1"));
    fake_reset(&F);
    run_n(1u);
    CHECK(F.len == 0u);
}

/* Labels keyed by address: a value still repaints the row, and so does a label
   that becomes a different string. */
static void t_static_labels(void)
{
    static bool led;
    unsigned    i;

    setup();
    atc_menu_static_labels(&C, true);

    for (i = 0u; i < 3u; ++i) {
        atc_menu_frame_begin(&C);
        atc_menu_bool_ro(&C, "LED", led);
        atc_menu_frame_end(&C);
    }
    fake_reset(&F);
    atc_menu_frame_begin(&C);
    atc_menu_bool_ro(&C, "LED", led);
    atc_menu_frame_end(&C);
    CHECK(F.len == 0u); /* nothing moved */

    led = true;
    fake_reset(&F);
    atc_menu_frame_begin(&C);
    atc_menu_bool_ro(&C, "LED", led);
    atc_menu_frame_end(&C);
    CHECK(fake_has(&F, "[X]"));

    fake_reset(&F);
    atc_menu_frame_begin(&C);
    atc_menu_bool_ro(&C, "Lamp", led);
    atc_menu_frame_end(&C);
    CHECK(fake_has(&F, "Lamp"));
}

/* The gap between label and value is 61 - strlen(label) + 12 - strlen(value)
   columns wide; what a row does with it is the whole of this. */
static void t_fast_fill(void)
{
    size_t spelt;

    setup();
    atc_menu_frame_begin(&C);
    atc_menu_uint16_ro(&C, "Hz", 1000u); /* item 1, striped */
    atc_menu_uint16_ro(&C, "ms", 20u);   /* item 2, plain */
    CHECK(atc_menu_frame_end(&C) == ATC_MENU_OK);
    CHECK(fake_has(&F, "ms\x1b[69X\x1b[69C")); /* blanked, then stepped over */
    CHECK(fake_has(&F, "Hz         "));  /* under a stripe, spaces still */
    spelt = F.len;

    fake_reset(&F);
    atc_menu_fast_fill(&C, true); /* which repaints what is already up */
    atc_menu_frame_begin(&C);
    atc_menu_uint16_ro(&C, "Hz", 1000u);
    atc_menu_uint16_ro(&C, "ms", 20u);
    CHECK(atc_menu_frame_end(&C) == ATC_MENU_OK);
    CHECK(fake_has(&F, "Hz \x1b[66b")); /* one cell, then 66 more like it */
    CHECK(fake_has(&F, "ms\x1b[69X\x1b[69C")); /* the plain row is as it was */
    CHECK(F.len < spelt);

    /* An underline shows in an empty cell, so those columns are cells too. */
    fake_reset(&F);
    atc_menu_frame_begin(&C);
    atc_menu_item_style(&C, ATC_MENU_UNDERLINE);
    atc_menu_uint16_ro(&C, "Hz", 1001u);
    atc_menu_item_style(&C, ATC_MENU_UNDERLINE);
    atc_menu_uint16_ro(&C, "ms", 20u);
    CHECK(atc_menu_frame_end(&C) == ATC_MENU_OK);
    CHECK(fake_has(&F, "ms \x1b[68b")); /* repeated, not stepped over */

    fake_reset(&F);
    atc_menu_fast_fill(&C, false);
    atc_menu_frame_begin(&C);
    atc_menu_item_style(&C, ATC_MENU_UNDERLINE);
    atc_menu_uint16_ro(&C, "Hz", 1001u);
    atc_menu_item_style(&C, ATC_MENU_UNDERLINE);
    atc_menu_uint16_ro(&C, "ms", 20u);
    CHECK(atc_menu_frame_end(&C) == ATC_MENU_OK);
    CHECK(fake_has(&F, "ms         ")); /* and spaces when REP is not offered */
}

/* A cell that ends up with something in it is written over, never cleared and
   then filled: on a slow line the gap between the two is a visible blink. */
static void t_a_changed_row_does_not_blink(void)
{
    setup();
    atc_menu_frame_begin(&C);
    atc_menu_uint16_ro(&C, "Hz", 1000u);
    atc_menu_uint16_ro(&C, "ms", 20u);
    CHECK(atc_menu_frame_end(&C) == ATC_MENU_OK);

    fake_reset(&F);
    atc_menu_frame_begin(&C);
    atc_menu_uint16_ro(&C, "Hz", 1001u);
    atc_menu_uint16_ro(&C, "ms", 21u);
    CHECK(atc_menu_frame_end(&C) == ATC_MENU_OK);
    CHECK(fake_has(&F, "1001"));
    CHECK(fake_has(&F, "21"));
    CHECK(!fake_has(&F, "\x1b[K")); /* neither row blanked itself first */
}

/* ---- fuzz ------------------------------------------------------------- */

static uint32_t rnd_state = 12345u;

static uint32_t rnd(void)
{
    rnd_state = rnd_state * 1103515245u + 12345u;
    return rnd_state >> 8;
}

static void t_fuzz_keys(void)
{
    unsigned i;

    setup();
    w_choice = 0u;
    for (i = 0u; i < 100000u; ++i) {
        atc_menu_key(&C, (int)(rnd() & 0xFFu));
        if ((i & 7u) == 0u) {
            if ((i & 8u) == 0u)
                frame_widgets();
            else
                frame_widgets2();
        }
        if (F.len > FAKE_CAP / 2u)
            fake_reset(&F);
    }
    CHECK(C.nav_depth < ATC_MENU_MAX_DEPTH);
    CHECK(C.decl_depth == 0u);
}

/* ---- narrow geometry -------------------------------------------------- */

static void t_narrow_geometry_truncates(void)
{
    atc_menu_ctx_t  c;
    fake_t          f;

    ATC_MENU_SCREEN(narrow, 40u, 12u);

    fake_reset(&f);
    CHECK(atc_menu_init(&c, &INFO, &narrow, fake_sink, &f) == ATC_MENU_OK);
    CHECK(atc_menu_term_begin(&c) == ATC_MENU_OK);
    fake_reset(&f);

    atc_menu_frame_begin(&c);
    atc_menu_uint16_ro(&c, "A very long label that cannot possibly fit", 7u);
    CHECK(atc_menu_frame_end(&c) == ATC_MENU_OK);

    CHECK(fake_has(&f, "A very long label tha")); /* clipped to its column */
    CHECK(!fake_has(&f, "possibly fit"));
}

/* ---- items-per-page ---------------------------------------------------- */

/* The legend lists the keys that would do something here, and only those: at
   the root there is nowhere to go back to, and on one page there is no other
   page to reach. */
static void t_footer_lists_the_keys_that_work(void)
{
    setup();
    run_n(3u);                             /* root, and it all fits one page */
    CHECK(!fake_has(&F, " Back"));
    CHECK(!fake_has(&F, " Page"));
    CHECK(fake_has(&F, " Refresh"));
    CHECK(fake_has(&F, " Items"));

    fake_reset(&F);
    run_n(30u);                            /* now it pages */
    CHECK(fake_has(&F, " Page"));
    CHECK(!fake_has(&F, " Back"));         /* still the root */

    /* and one level down, Back appears */
    setup();
    g_leaf_n = 2u;
    frame_tree();
    keys("2");
    fake_reset(&F);
    frame_tree();
    frame_tree();
    CHECK(fake_has(&F, " Back"));
    CHECK(!fake_has(&F, " Page"));         /* two leaves fit one page */
}

static void t_items_per_page_api(void)
{
    setup();
    CHECK(atc_menu_items_per_page(&C) == IROWS); /* the whole area by default */

    atc_menu_set_items_per_page(&C, 4u);
    CHECK(atc_menu_items_per_page(&C) == 4u);

    atc_menu_set_items_per_page(&C, 0u);         /* clamped, never refused */
    CHECK(atc_menu_items_per_page(&C) == 1u);

    atc_menu_set_items_per_page(&C, 999u);
    CHECK(atc_menu_items_per_page(&C) == IROWS);

    atc_menu_set_items_per_page(NULL, 4u);       /* no crash, no effect */
    CHECK(atc_menu_items_per_page(NULL) == 0u);
}

static void t_items_per_page_key(void)
{
    setup();
    run_n(12u);                       /* 12 items, 9 rows: two pages */
    CHECK(fake_has(&F, "(1/2)"));

    fake_reset(&F);
    keys("i");
    frame_n_actions();
    /* the size in the bracket, the range from geometry */
    CHECK(fake_has(&F, "Items [9] (1-9)> _"));

    fake_reset(&F);
    keys("4");
    frame_n_actions();
    CHECK(fake_has(&F, "Items [9] (1-9)> 4_"));

    keys("\r");
    frame_n_actions();                /* commits; the size lands next frame */
    fake_reset(&F);
    frame_n_actions();

    CHECK(atc_menu_items_per_page(&C) == 4u);
    CHECK(fake_has(&F, "(1/3)"));     /* 12 items over 4 rows */
    CHECK(!fake_has(&F, "Item 5"));   /* row 5 was erased, not redrawn */
    CHECK(fake_has(&F, "Select> _")); /* the prompt is back to selecting */

    /* Items 1..4 kept their rows, so the diff said nothing about them; what
       the page holds now takes a repaint to see. */
    atc_menu_refresh(&C);
    fake_reset(&F);
    frame_n_actions();
    CHECK(fake_has(&F, "Item 4"));
    CHECK(!fake_has(&F, "Item 5"));

    /* and the shorter page still pages */
    keys("n");
    fake_reset(&F);
    frame_n_actions();
    CHECK(fake_has(&F, "(2/3)"));
    CHECK(fake_has(&F, "Item 5"));
    CHECK(fake_has(&F, "   1   ")); /* numbering restarts on the new page */
}

static void t_items_per_page_cancel(void)
{
    setup();
    run_n(12u);

    keys("i\x1b");                    /* Esc drops the editor */
    frame_n_actions();
    frame_n_actions();
    CHECK(atc_menu_items_per_page(&C) == IROWS);
    CHECK(!atc_menu_editing(&C));

    keys("i4\x1b");                   /* even with digits typed */
    frame_n_actions();
    frame_n_actions();
    CHECK(atc_menu_items_per_page(&C) == IROWS);

    keys("i\r");                      /* an empty Enter cancels too */
    frame_n_actions();
    frame_n_actions();
    CHECK(atc_menu_items_per_page(&C) == IROWS);

    keys("i999\r");                   /* out of range is clamped, not refused */
    frame_n_actions();
    frame_n_actions();
    CHECK(atc_menu_items_per_page(&C) == IROWS);

    keys("i1\r");
    frame_n_actions();
    frame_n_actions();
    CHECK(atc_menu_items_per_page(&C) == 1u);
}

/* Once an editor is open every key belongs to it, so the command keys stop
   being command keys until it closes. */
static void t_command_keys_do_not_reach_an_open_editor(void)
{
    setup();
    frame_widgets();
    keys("1");            /* opens the U8 editor */
    frame_widgets();
    CHECK(atc_menu_editing(&C));

    keys("inr");          /* none of these is a decimal digit */
    frame_widgets();
    CHECK(atc_menu_editing(&C));
    CHECK(atc_menu_items_per_page(&C) == IROWS);

    keys("\x1b");
    frame_widgets();
    CHECK(!atc_menu_editing(&C));

    /* and the other way round: a row cannot be picked while 'i' is open */
    keys("i");
    frame_widgets();
    w_changed = 0;
    keys("1\r");          /* reads as the page size, not as item 1 */
    frame_widgets();
    frame_widgets();
    CHECK(w_changed == 0);
    CHECK(atc_menu_items_per_page(&C) == 1u);
}

/* A row that is not on the visible page carries no number, and the editor the
   'i' key opens is not on a row at all — so its commit must reach neither. */
static void t_rows_editor_does_not_write_a_scrolled_off_item(void)
{
    setup();
    frame_widgets();
    atc_menu_set_items_per_page(&C, 2u); /* U8 and U16 shown, the rest scrolled off */
    frame_widgets();

    w_u32 = 111u;
    w_i32 = 222;
    w_x32 = 333u;
    w_changed = 0;

    keys("i4\r");
    frame_widgets();
    frame_widgets();

    CHECK(atc_menu_items_per_page(&C) == 4u);
    CHECK(w_changed == 0);   /* no widget saw a commit */
    CHECK(w_u32 == 111u);
    CHECK(w_i32 == 222);
    CHECK(w_x32 == 333u);
}

/* Leading zeros leave the accumulator alone, so the digit count can run past
   the eight a uint32_t shows in hex. */
static void t_editor_bounds_the_digit_count(void)
{
    setup();
    w_x32 = 0xABu;
    frame_widgets();
    keys("8");                       /* X32 */
    frame_widgets();

    keys("00000000000000");          /* far past the 8 a uint32_t holds */
    fake_reset(&F);
    frame_widgets();
    CHECK(fake_has(&F, "0x00000000"));   /* echoed, and no more than 8 digits */

    keys("\r");
    frame_widgets();
    CHECK(w_x32 == 0u);

    /* in decimal the echo is the accumulator, so leading zeros are harmless */
    w_u32 = 7u;
    frame_widgets();
    keys("3");                       /* U32 */
    frame_widgets();
    keys("00000000000000000001\r");
    frame_widgets();
    CHECK(w_u32 == 1u);
}

/* A level deeper than the row index can count must say so rather than let two
   rows share a number. */
static void t_too_many_rows_is_a_status(void)
{
    setup_tall();
    g_n = 300u;
    atc_menu_frame_begin(&C);
    {
        unsigned i;
        char     name[16];
        for (i = 0u; i < g_n; ++i) {
            sprintf(name, "Item %u", i + 1u);
            atc_menu_action(&C, name);
        }
    }
    CHECK(atc_menu_frame_end(&C) == ATC_MENU_ERR_STATE);
}

/* A banner line with nothing in it is not painted, so it must not be charged
   either — the row belongs to the items. */
static void t_empty_banner_gives_its_rows_to_the_items(void)
{
    static const atc_menu_info_t NAME_ONLY = { "TestApp", NULL, NULL };
    atc_menu_ctx_t c;
    fake_t         f;

    ATC_MENU_SCREEN(banner, COLS, MROWS);

    /* no banner at all: both header rows go to the items */
    fake_reset(&f);
    CHECK(atc_menu_init(&c, NULL, &banner, fake_sink, &f) == ATC_MENU_OK);
    CHECK(atc_menu_items_per_page(&c) == IROWS + 2u);

    atc_menu_frame_begin(&c);
    atc_menu_uint16_ro(&c, "A", 1u);
    CHECK(atc_menu_frame_end(&c) == ATC_MENU_OK);
    CHECK(fake_has(&f, "\x1b[1;1H"));  /* the rule is the first row now */
    CHECK(fake_has(&f, "\x1b[3;1H"));  /* and the items start two rows up */

    /* a name but no owner: one row back */
    fake_reset(&f);
    CHECK(atc_menu_init(&c, &NAME_ONLY, &banner, fake_sink, &f) == ATC_MENU_OK);
    CHECK(atc_menu_items_per_page(&c) == IROWS + 1u);

    /* the full banner is the worst case, and the floor init enforces */
    fake_reset(&f);
    CHECK(atc_menu_init(&c, &INFO, &banner, fake_sink, &f) == ATC_MENU_OK);
    CHECK(atc_menu_items_per_page(&c) == IROWS);
}

/* A banner string longer than the menu is wide must be clipped, not allowed to
   run the right-alignment loop backwards. */
static void t_oversized_banner_is_clipped(void)
{
    static const char LONG[] =
        "0123456789012345678901234567890123456789ABCDEFGHIJ"; /* 50 > 40 */
    static const atc_menu_info_t BIG = { LONG, LONG, LONG };
    atc_menu_ctx_t c;
    fake_t         f;

    ATC_MENU_SCREEN(small, 40u, 12u);

    fake_reset(&f);
    CHECK(atc_menu_init(&c, &BIG, &small, fake_sink, &f) == ATC_MENU_OK);
    atc_menu_frame_begin(&c);
    atc_menu_uint16_ro(&c, "A", 1u);
    CHECK(atc_menu_frame_end(&c) == ATC_MENU_OK); /* terminates, and fits */
    CHECK(!fake_has(&f, "ABCDEFGHIJ"));
}

/* A page shorter than the item area leaves rows behind it; they have to go. */
static void t_items_per_page_erases_the_rest(void)
{
    setup();
    run_n(9u);
    CHECK(fake_has(&F, "Item 9"));

    atc_menu_set_items_per_page(&C, 2u);
    fake_reset(&F);
    frame_n_actions();
    CHECK(!fake_has(&F, "Item 3"));
    CHECK(fake_has(&F, "\x1b[7;1H"));  /* first row past the page: erased */
    CHECK(fake_has(&F, "\x1b[13;1H")); /* and the last one before the rule */

    atc_menu_refresh(&C);
    fake_reset(&F);
    frame_n_actions();
    CHECK(fake_has(&F, "Item 2"));
    CHECK(!fake_has(&F, "Item 3"));
}

/* ---- narrow terminals -------------------------------------------------- */

static atc_menu_ctx_t NC;
ATC_MENU_SCREEN(NSCREEN, 40u, 12u);
static fake_t         NF;

static void setup_narrow(void)
{
    fake_reset(&NF);
    CHECK(atc_menu_init(&NC, &INFO, &NSCREEN, fake_sink, &NF) == ATC_MENU_OK);
    CHECK(atc_menu_term_begin(&NC) == ATC_MENU_OK);
    fake_reset(&NF);
}

/* All four hints want 37 columns, so a 30-column row runs out before the list
   does. Whole hints go, in reverse priority — which needs a screen where all
   four are relevant in the first place. */
static void t_narrow_footer_drops_hints(void)
{
    atc_menu_ctx_t c;
    fake_t         f;
    unsigned       i;
    char           name[16];

    ATC_MENU_SCREEN(tight, 30u, 12u);

    fake_reset(&f);
    CHECK(atc_menu_init(&c, &INFO, &tight, fake_sink, &f) == ATC_MENU_OK);
    CHECK(atc_menu_term_begin(&c) == ATC_MENU_OK);
    fake_reset(&f);

    /* A frame has to declare the submenu before its number means anything, and
       the descent lands at frame_end, so the level itself paints a frame later. */
    for (i = 0u; i < 4u; ++i) {
        unsigned j;

        if (i == 1u)
            atc_menu_key(&c, '1');            /* descend, so Back applies */
        if (i == 3u) {
            /* an unchanged footer emits nothing, so ask for the whole screen */
            atc_menu_refresh(&c);
            fake_reset(&f);
        }
        atc_menu_frame_begin(&c);
        if (atc_menu_submenu(&c, "Sub")) {
            for (j = 0u; j < 30u; ++j) {       /* and enough to page */
                sprintf(name, "I%u", j + 1u);
                atc_menu_uint16_ro(&c, name, 1u);
            }
            atc_menu_submenu_end(&c);
        }
        CHECK(atc_menu_frame_end(&c) == ATC_MENU_OK); /* never ERR_PARAM */
    }

    CHECK(fake_has(&f, " Back"));                 /* navigation survives */
    CHECK(fake_has(&f, " Refresh"));
    CHECK(fake_has(&f, " Page"));
    CHECK(!fake_has(&f, " Items"));               /* the last one does not fit */
}

/* A message is the application's string; a long one must not wrap and push the
   prompt off the bottom of the menu. */
static void t_long_message_is_clipped(void)
{
    static const char LONG[] =
        "0123456789012345678901234567890123456789ABCDEFGHIJ"; /* 50 > 40 */

    setup_narrow();
    atc_menu_frame_begin(&NC);
    atc_menu_message(&NC, LONG);
    atc_menu_uint16_ro(&NC, "A", 1u);
    CHECK(atc_menu_frame_end(&NC) == ATC_MENU_OK);

    CHECK(fake_has(&NF, "0123456789012345678901234567890123456789"));
    CHECK(!fake_has(&NF, "ABCDEFGHIJ"));
}

int main(void)
{
    t_term_begin_bytes();
    t_first_frame_bytes();
    t_idle_frame_is_silent();
    t_prefix_rule_n11();
    t_prefix_rule_scales();
    t_pending_prefix_is_shown();
    t_submenu_enter_and_back();
    t_paging_and_restore();
    t_shrink_pulls_the_footer_up();
    t_widget_rendering();
    t_bool_and_choice();
    t_numeric_edit_commit_and_cancel();
    t_paging_drops_the_pending_prefix();
    t_invalid_selection_says_so();
    t_value_column_is_coloured();
    t_prompt_value_is_coloured();
    t_type_clamps();
    t_refused_entry_stays_open();
    t_reject_reopens_the_editor();
    t_extremes();
    t_hex_entry();
    t_fixed_point();
    t_editor_starts_empty();
    t_editor_prompt_names_its_row();
    t_decimal_point_is_visible();
    t_separator();
    t_disabled_item();
    t_separator_spends_the_modifier();
    t_reject_does_nothing_after_a_bool();
    t_style_paints_label_and_value();
    t_style_colour_replaces_the_value_green();
    t_style_is_spent_by_one_item();
    t_no_style_changes_nothing();
    t_dim_outranks_style();
    t_style_does_not_cross_a_level();
    t_style_fits_the_row_budget();
    t_text_edit();
    t_unselectable_says_why();
    t_sink_refusal_leaves_row_dirty();
    t_sink_always_refuses();
    t_param_guards();
    t_state_guards();
    t_depth_overflow();
    t_two_instances();
    t_text_ro_takes_a_stack_buffer();
    t_refresh_and_reset();
    t_item_set_changes_completely();
    t_narrow_geometry_truncates();
    t_footer_lists_the_keys_that_work();
    t_items_per_page_api();
    t_items_per_page_key();
    t_items_per_page_cancel();
    t_command_keys_do_not_reach_an_open_editor();
    t_empty_banner_gives_its_rows_to_the_items();
    t_oversized_banner_is_clipped();
    t_items_per_page_erases_the_rest();
    t_rows_editor_does_not_write_a_scrolled_off_item();
    t_editor_bounds_the_digit_count();
    t_too_many_rows_is_a_status();
    t_narrow_footer_drops_hints();
    t_long_message_is_clipped();
    t_static_labels();
    t_fast_fill();
    t_a_changed_row_does_not_blink();
    t_fuzz_keys();

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
