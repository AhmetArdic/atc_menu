/**
 * @file test_main.c
 * @brief ATC Menu - host test suite. Runs the library exactly like an
 *        embedded application would: bytes in via atc_menu_key(), drawing via
 *        atc_menu_update(), output captured by a fake sink and compared
 *        against golden files or structural expectations. The fixture mimics
 *        a small MCU control panel (sensors, GPIO, PWM, registers). Run with
 *        --update to regenerate goldens.
 * @author Ahmet Talha ARDIC
 * @date   2026-07-31
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atc_menu/menu.h"
#include "menu_internal.h"

static int failures;
#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++; \
        } \
    } while (0)

/* ---- fake sink ---------------------------------------------------------- */

static char cap[65536];
static size_t cap_len;
static size_t call_sizes[256];
static unsigned sink_calls;
static int reject_alternate;  /* reject every odd-numbered call */

static int test_sink(void *user, const char *buf, size_t len)
{
    (void)user;
    sink_calls++;
    if (reject_alternate && (sink_calls & 1u))
        return 0;
    if (cap_len + len <= sizeof(cap)) {
        memcpy(cap + cap_len, buf, len);
        if (sink_calls - 1 < 256)
            call_sizes[sink_calls - 1] = len;
        cap_len += len;
    }
    return 1;
}

static void cap_reset(void)
{
    cap_len = 0;
    sink_calls = 0;
}

static int cap_contains(const char *s)
{
    size_t sl = strlen(s), i;

    if (sl > cap_len)
        return 0;
    for (i = 0; i + sl <= cap_len; i++)
        if (!memcmp(cap + i, s, sl))
            return 1;
    return 0;
}

static unsigned cap_count(const char *s)
{
    size_t sl = strlen(s), i;
    unsigned n = 0;

    for (i = 0; sl && i + sl <= cap_len; i++)
        if (!memcmp(cap + i, s, sl))
            n++;
    return n;
}

/* ---- golden files ------------------------------------------------------- */

static int update_golden;

static void golden(const char *name)
{
    char path[128];
    FILE *f;

    sprintf(path, "golden/%s.bin", name);
    if (update_golden) {
        f = fopen(path, "wb");
        if (!f) {
            printf("FAIL cannot write %s\n", path);
            failures++;
            return;
        }
        fwrite(cap, 1, cap_len, f);
        fclose(f);
        printf("updated %s (%u bytes)\n", path, (unsigned)cap_len);
        return;
    }
    f = fopen(path, "rb");
    if (!f) {
        printf("FAIL missing %s (run with --update)\n", path);
        failures++;
        return;
    }
    {
        static char exp[65536];
        size_t n = fread(exp, 1, sizeof(exp), f);
        fclose(f);
        if (n != cap_len || memcmp(exp, cap, n) != 0) {
            printf("FAIL golden mismatch %s (expected %u, got %u bytes)\n",
                   path, (unsigned)n, (unsigned)cap_len);
            failures++;
        }
    }
}

/* ---- fixture: simulated MCU state --------------------------------------- */

static int32_t pwm_freq;        /* Hz */
static atc_menu_u8 led;         /* P1.0 output */
static int32_t sys_mode;        /* IDLE / RUN / SLEEP */
static char dev_name[32];
static int32_t duty_x10;        /* PWM duty, 0.1 % units */
static uint16_t adcctl;         /* 16-bit peripheral register */
static uint32_t wide_val;       /* full 32-bit unsigned setting */
static int16_t narrow_val;      /* 16-bit signed setting */
static int32_t tag_idx;
static int32_t il_mode;
static int32_t temp_x10;        /* 0.1 degC units */
static uint16_t hum_raw;        /* ADC counts, 0..4095 */
static int32_t setpoint;
static atc_menu_u8 relay;
static int saved;
static char last_cmd[32];

/* accessor-page state */
static uint8_t port_sim;        /* simulated 8-bit GPIO port */
static uint16_t adc_sim[2];
static uint32_t wide_sim;
static int32_t free_val;
static unsigned get_calls;      /* counts accessor invocations */
static int32_t last_arg;
static int arg_calls;

static atc_menu_ctx_t ctx;  /* tentative; defined again below the pages */

/* ---- fixture accessors --------------------------------------------------- */

static int32_t rd_temp(int32_t arg)
{
    (void)arg;
    return temp_x10;
}

static int32_t rd_hum(int32_t arg)  /* raw ADC counts scaled to percent */
{
    (void)arg;
    return (int32_t)hum_raw * 100 / 4095;
}

static int32_t led_get(int32_t arg)
{
    (void)arg;
    return led;
}

static int led_set(int32_t arg, int32_t value)
{
    (void)arg;
    led = (atc_menu_u8)value;
    return 1;
}

static int32_t relay_get(int32_t arg)
{
    (void)arg;
    return relay;
}

/* Interlock: refuses to write and explains why. The row is redrawn from
 * relay_get(), so the display falls back to the state that actually holds. */
static int relay_set(int32_t arg, int32_t value)
{
    (void)arg;
    (void)value;
    atc_menu_message(&ctx, "Relay needs RUN mode", 1);
    return 0;
}

/* The fixture's plain settings, through the library's own codegen macro. */
ATC_MENU_DEFINE_ACCESSORS(freq, pwm_freq)
ATC_MENU_DEFINE_ACCESSORS(mode, sys_mode)
ATC_MENU_DEFINE_ACCESSORS(duty, duty_x10)
ATC_MENU_DEFINE_ACCESSORS(tag,  tag_idx)
ATC_MENU_DEFINE_ACCESSORS(fval, free_val)
ATC_MENU_DEFINE_ACCESSORS(reg,  adcctl)      /* uint16_t register  */
ATC_MENU_DEFINE_ACCESSORS(u32v, wide_val)    /* uint32_t, past 2^31 */
ATC_MENU_DEFINE_ACCESSORS(i16v, narrow_val)  /* int16_t             */

static void on_cmd(int32_t arg)
{
    saved = (int)arg;
    atc_menu_message(&ctx, "Config saved", 0);
}

static int on_name(int32_t arg, const char *text)
{
    (void)arg;
    if (strlen(text) >= sizeof dev_name) {
        atc_menu_message(&ctx, "Too long", 1);
        return 0;
    }
    strcpy(dev_name, text);
    return 1;
}

static int on_console(int32_t arg, const char *text)
{
    (void)arg;
    if (!strcmp(text, "bad"))
        return 0;
    strcpy(last_cmd, text);
    return 1;
}

static int32_t setpoint_get(int32_t arg)
{
    (void)arg;
    return setpoint;
}

static int setpoint_set(int32_t arg, int32_t value)  /* interlock: never writes */
{
    (void)arg;
    (void)value;
    atc_menu_message(&ctx, "Setpoint locked", 1);
    return 0;
}

static int choice_cb_calls;
static int choice_cb_reject;   /* 0 = accept, nonzero = refuse */

static int32_t il_mode_get(int32_t arg)
{
    (void)arg;
    return il_mode;
}

static int il_mode_set(int32_t arg, int32_t value)
{
    (void)arg;
    choice_cb_calls++;
    if (choice_cb_reject)
        return 0;   /* no message of its own: the generic one shows */
    il_mode = value;
    return 1;
}

/* ---- accessor-page accessors --------------------------------------------- */

static int32_t port_get(int32_t arg)
{
    get_calls++;
    return (port_sim >> arg) & 1;
}

static int port_set(int32_t arg, int32_t value)
{
    if (value)
        port_sim |= (uint8_t)(1u << arg);
    else
        port_sim &= (uint8_t)~(1u << arg);
    return 1;
}

static int locked_set(int32_t arg, int32_t value)  /* never writes */
{
    (void)arg;
    (void)value;
    atc_menu_message(&ctx, "Locked", 1);
    return 0;
}

static int32_t adc_get(int32_t arg)
{
    return (int32_t)adc_sim[arg];
}

static int32_t wide_get(int32_t arg)
{
    (void)arg;
    return (int32_t)wide_sim;
}

static int32_t port_reg(int32_t arg)
{
    (void)arg;
    return (int32_t)port_sim;
}

static void on_arg(int32_t arg)
{
    last_arg = arg;
    arg_calls++;
}

/* ---- pages --------------------------------------------------------------- */

ATC_MENU_CHOICES(sys_modes, "IDLE", "RUN", "SLEEP");
ATC_MENU_CHOICES(tag_items, "t0", "t1");
ATC_MENU_CHOICES(il_modes, "AUTO", "MANUAL");

ATC_MENU_PAGE_BEGIN(reg_page, "Registers")
    ATC_MENU_HEX     ("ADCCTL0", reg_get, reg_set, 0, 16)
    ATC_MENU_SEPARATOR()
    ATC_MENU_READOUT ("TA0R",    rd_temp, 0, ATC_MENU_DEC)
ATC_MENU_PAGE_END(reg_page)

ATC_MENU_PAGE_BEGIN(panel_page, "Control Panel")
    ATC_MENU_LABEL   ("System")
    ATC_MENU_NUMBER  ("PWM Freq (Hz)", freq_get, freq_set, 0)     /* 1 */
    ATC_MENU_CHECKBOX("LED (P1.0)",    led_get,  led_set,  0)     /* 2 */
    ATC_MENU_CHOICE  ("Sys Mode",      mode_get, mode_set, 0, sys_modes) /* 3 */
    ATC_MENU_PROMPT  ("Dev Name",      on_name, 0)                                 /* 4 */
    ATC_MENU_FIXED   ("Duty (%)",      duty_get, duty_set, 0, 1)  /* 5 */
    ATC_MENU_PROMPT  ("Console",       on_console, 0)                              /* 6 */
    ATC_MENU_SUBMENU ("Registers",     reg_page)                                /* 7 */
    ATC_MENU_CHOICE  ("Tag",           tag_get,  tag_set,  0, tag_items) /* 8 */
    ATC_MENU_READOUT ("Temp (0.1C)",   rd_temp,  0, ATC_MENU_DEC) /* 9 */
    ATC_MENU_READOUT ("Humidity",      rd_hum,   0, ATC_MENU_DEC) /* 10 */
    ATC_MENU_ACTION  ("Save Cfg",      on_cmd, 7)                               /* 11 */
ATC_MENU_PAGE_END(panel_page)

ATC_MENU_PAGE_BEGIN(interlock_page, "Interlocks")
    ATC_MENU_NUMBER  ("Setpoint", setpoint_get, setpoint_set, 0)  /* 1 */
    ATC_MENU_CHECKBOX("Relay",    relay_get,    relay_set,    0)  /* 2 */
    ATC_MENU_CHOICE  ("IL Mode",  il_mode_get,  il_mode_set,  0, il_modes) /* 3 */
ATC_MENU_PAGE_END(interlock_page)

ATC_MENU_PAGE_BEGIN(adc_page, "ADC Channels")
    ATC_MENU_READOUT("CH01", rd_temp, 0, ATC_MENU_DEC)
    ATC_MENU_READOUT("CH02", rd_temp, 0, ATC_MENU_DEC)
    ATC_MENU_READOUT("CH03", rd_temp, 0, ATC_MENU_DEC)
    ATC_MENU_READOUT("CH04", rd_temp, 0, ATC_MENU_DEC)
    ATC_MENU_READOUT("CH05", rd_temp, 0, ATC_MENU_DEC)
    ATC_MENU_READOUT("CH06", rd_temp, 0, ATC_MENU_DEC)
    ATC_MENU_READOUT("CH07", rd_temp, 0, ATC_MENU_DEC)
    ATC_MENU_READOUT("CH08", rd_temp, 0, ATC_MENU_DEC)
    ATC_MENU_READOUT("CH09", rd_temp, 0, ATC_MENU_DEC)
    ATC_MENU_READOUT("CH10", rd_temp, 0, ATC_MENU_DEC)
    ATC_MENU_READOUT("CH11", rd_temp, 0, ATC_MENU_DEC)
    ATC_MENU_READOUT("CH12", rd_temp, 0, ATC_MENU_DEC)
    ATC_MENU_READOUT("CH13", rd_temp, 0, ATC_MENU_DEC)
    ATC_MENU_READOUT("CH14", rd_temp, 0, ATC_MENU_DEC)
    ATC_MENU_READOUT("CH15", rd_temp, 0, ATC_MENU_DEC)
ATC_MENU_PAGE_END(adc_page)


/* One getter pair drives four pins; the item's arg is the bit index. */
ATC_MENU_PAGE_BEGIN(acc_page, "Accessors")
    ATC_MENU_CHECKBOX("OUT0",  port_get, port_set,   0)   /* 1 read/write   */
    ATC_MENU_CHECKBOX("OUT1",  port_get, port_set,   1)   /* 2 read/write   */
    ATC_MENU_CHECKBOX("IN2",   port_get, NULL,       2)   /* 3 read-only    */
    ATC_MENU_CHECKBOX("LOCK3", port_get, locked_set, 3)   /* 4 refuses      */
    ATC_MENU_CHECKBOX("NONE",  NULL,     NULL,       0)   /* 5 null-safe    */
    ATC_MENU_SEPARATOR()
    ATC_MENU_READOUT ("Dec",   adc_get,  0, ATC_MENU_DEC)   /* 6  253        */
    ATC_MENU_READOUT ("Udec",  wide_get, 0, ATC_MENU_DEC | ATC_MENU_UNSIGNED)   /* 7  4294967280 */
    ATC_MENU_READOUT ("Hex8",  port_reg, 0, ATC_MENU_HEX8)  /* 8  0x05       */
    ATC_MENU_READOUT ("Hex16", wide_get, 0, ATC_MENU_HEX16) /* 9  0xFFF0     */
    ATC_MENU_READOUT ("Fix1",  adc_get,  1, ATC_MENU_FIX1)  /* 10 330.0      */
    ATC_MENU_READOUT ("Fix2",  adc_get,  1, ATC_MENU_FIX2)  /* 11 33.00      */
ATC_MENU_PAGE_END(acc_page)

/* All the editor itself guards: 32 bits, and a minus sign that has to land
 * inside int32_t. Narrower is the setter's business - rows 3 and 4. */
ATC_MENU_PAGE_BEGIN(range_page, "Ranges")
    ATC_MENU_NUMBER  ("Wide",  u32v_get, u32v_set, 0)     /* 1 full 32 bits   */
    ATC_MENU_READOUT ("WideU", u32v_get, 0, ATC_MENU_DEC | ATC_MENU_UNSIGNED)
    ATC_MENU_NUMBER  ("Narrow", reg_get, reg_set, 0)      /* 2 uint16_t var   */
    ATC_MENU_NUMBER  ("Signed", i16v_get, i16v_set, 0)    /* 3 int16_t var    */
    ATC_MENU_NUMBER  ("I32",   freq_get, freq_set, 0)     /* 4 signed 32-bit  */
    /* Scaled and unsigned: the decimal point must not turn the top of the
     * range negative. */
    ATC_MENU_FIXED   ("UFix",  u32v_get, u32v_set, 0, 3)  /* 5 */
    ATC_MENU_READOUT ("UFixU", u32v_get, 0, ATC_MENU_FIX3 | ATC_MENU_UNSIGNED)
ATC_MENU_PAGE_END(range_page)

/* ---- CUSTOM: values the built-in tools cannot express --------------------- */

static float gain = 1.5f;
static char label_text[9] = "alpha";
static int32_t last_show_arg;
static unsigned show_cap_seen;

/* A float, formatted without printf the way a firmware would. */
static unsigned gain_show(int32_t arg, char *out, unsigned cap)
{
    uint32_t scaled = (uint32_t)(gain * 100.0f + 0.5f);

    last_show_arg = arg;
    show_cap_seen = cap;
    if (cap < 4)
        return 0;
    out[0] = (char)('0' + scaled / 100u);
    out[1] = '.';
    out[2] = (char)('0' + (scaled / 10u) % 10u);
    out[3] = (char)('0' + scaled % 10u);
    return 4;
}

static int gain_edit(int32_t arg, const char *text)
{
    (void)arg;
    if (!strcmp(text, "2.25")) {
        gain = 2.25f;
        return 1;
    }
    atc_menu_message(&ctx, "Bad gain", 1);
    return 0;
}

/* A string, which no value tool can hold at all. */
static unsigned label_show(int32_t arg, char *out, unsigned cap)
{
    unsigned n = 0;

    (void)arg;
    while (label_text[n] && n < cap) {
        out[n] = label_text[n];
        n++;
    }
    return n;
}

static int label_edit(int32_t arg, const char *text)
{
    (void)arg;
    if (strlen(text) >= sizeof label_text)
        return 0;
    strcpy(label_text, text);
    return 1;
}

/* Writes past `cap`; the renderer has to hold it to the value column. */
static unsigned greedy_show(int32_t arg, char *out, unsigned cap)
{
    (void)arg;
    (void)cap;
    memset(out, 'Z', 40);
    return 40;
}

ATC_MENU_PAGE_BEGIN(custom_page, "Custom")
    ATC_MENU_CUSTOM("Gain",   gain_show,  gain_edit,  7)   /* 1 float        */
    ATC_MENU_CUSTOM("Label",  label_show, label_edit, 0)   /* 2 string       */
    ATC_MENU_CUSTOM("RdOnly", label_show, NULL,       0)   /* 3 no edit      */
    ATC_MENU_CUSTOM("Ask",    NULL,       label_edit, 0)   /* 4 prompt-shaped */
    ATC_MENU_CUSTOM("Greedy", greedy_show, NULL,      0)   /* 5 overruns cap */
ATC_MENU_PAGE_END(custom_page)

/* 12x "abcde " wraps to exactly 3 lines; see test_text_wrap(). */
static const char bulletin_text[] =
    "abcde abcde abcde abcde abcde abcde "
    "abcde abcde abcde abcde abcde abcde ";

ATC_MENU_PAGE_BEGIN(text_page, "Notes")
    ATC_MENU_NUMBER("Alpha", freq_get,     freq_set,     0)  /* 1 */
    ATC_MENU_TEXT  ("Sensor readings update every second automatically.")
    ATC_MENU_NUMBER("Beta",  fval_get,     fval_set,     0)  /* 2 */
    ATC_MENU_TEXT  ("Configuration changes need a manual save before "
                    "they take effect.")
    ATC_MENU_NUMBER("Gamma", setpoint_get, setpoint_set, 0)  /* 3 */
ATC_MENU_PAGE_END(text_page)

ATC_MENU_PAGE_BEGIN(text_paging_page, "Bulletin")
    ATC_MENU_LABEL ("Intro")
    ATC_MENU_TEXT  (bulletin_text)
    ATC_MENU_NUMBER("Value", freq_get, freq_set, 0)          /* 1 */
ATC_MENU_PAGE_END(text_paging_page)

ATC_MENU_PAGE_BEGIN(cmd_page, "Commands")
    ATC_MENU_ACTION ("Alpha",  on_arg, 11)                                    /* 1 */
    ATC_MENU_ACTION ("Beta",   on_arg, 22)                                    /* 2 */
    ATC_MENU_ACTION ("NoFn",   NULL,   33)                                    /* 3 */
    ATC_MENU_READOUT("Hex32",  wide_get, 0, ATC_MENU_HEX32)     /* 4 0xFFFFFFF0 */
    ATC_MENU_READOUT("Fix3",   adc_get,  1, ATC_MENU_FIX3)      /* 5 3.300      */
    ATC_MENU_READOUT("NoRead", NULL,     0, ATC_MENU_DEC)       /* 6 0          */
    ATC_MENU_NUMBER ("Free",   fval_get, fval_set, 0)           /* 7 */
ATC_MENU_PAGE_END(cmd_page)

static const atc_menu_info_t info = { "MSP430 EnvMon", "v1.4.2", "ATC / A. Ardic" };
static atc_menu_ctx_t ctx;

static void reset_vars(void)
{
    pwm_freq = 1000;
    led = 1;
    sys_mode = 2;  /* SLEEP */
    strcpy(dev_name, "node-01");
    duty_x10 = 500;
    adcctl = 0x1A2B;
    tag_idx = 0;   /* -> "t0" */
    il_mode = 0;   /* -> "AUTO" */
    choice_cb_calls = 0;
    choice_cb_reject = 0;
    temp_x10 = 253;
    hum_raw = 1638;  /* -> 40 % */
    setpoint = 42;
    relay = 0;
    saved = 0;
    last_cmd[0] = '\0';

    port_sim = 0x05;      /* bit0 and bit2 high */
    adc_sim[0] = 253;
    adc_sim[1] = 3300;
    wide_sim = 0xFFFFFFF0u;
    wide_val = 0;
    narrow_val = 0;
    free_val = 0;
    get_calls = 0;
    last_arg = 0;
    arg_calls = 0;
}

static void pump(void)
{
    int i;

    for (i = 0; i < 300; i++)
        if (atc_menu_update(&ctx) == ATC_MENU_IDLE)
            break;
}

static void feed(const char *s)
{
    while (*s)
        atc_menu_key(&ctx, *s++);
    pump();
}

static void start(const atc_menu_page_t *root, const atc_menu_info_t *a)
{
    reset_vars();
    cap_reset();
    reject_alternate = 0;
    atc_menu_init(&ctx, a, root, test_sink, NULL);
    pump();
}

/* ---- tests -------------------------------------------------------------- */

#define FMTCHK(expr, exp) \
    do { \
        char t[24]; \
        unsigned n = (expr); \
        CHECK(n == strlen(exp) && !memcmp(t, exp, n)); \
    } while (0)

static void test_fmt(void)
{
    FMTCHK(atc_menu_fmt_u32(t, 0), "0");
    FMTCHK(atc_menu_fmt_u32(t, 4294967295u), "4294967295");
    FMTCHK(atc_menu_fmt_i32(t, -123), "-123");
    FMTCHK(atc_menu_fmt_i32(t, 2147483647), "2147483647");
    FMTCHK(atc_menu_fmt_hex(t, 0x1A2B, 4), "1A2B");
    FMTCHK(atc_menu_fmt_hex(t, 0xF, 2), "0F");
    FMTCHK(atc_menu_fmt_fix(t, 3300, 2, 1), "33.00");
    FMTCHK(atc_menu_fmt_fix(t, 3350, 2, 1), "33.50");
    FMTCHK(atc_menu_fmt_fix(t, -50, 2, 1), "-0.50");
    FMTCHK(atc_menu_fmt_fix(t, 5, 1, 1), "0.5");
    FMTCHK(atc_menu_fmt_fix(t, 7, 0, 1), "7");
    /* Unsigned: the same bits that read as -16 signed are a 7-digit whole part. */
    FMTCHK(atc_menu_fmt_fix(t, -16, 3, 0), "4294967.280");
    FMTCHK(atc_menu_fmt_fix(t, -16, 0, 0), "4294967280");
}

static void test_full_page(void)
{
    start(&panel_page, &info);
    CHECK(cap_count("\x1b[2J") == 1);
    CHECK(cap_contains("MSP430 EnvMon"));
    CHECK(cap_contains("v1.4.2"));
    CHECK(cap_contains("ATC / A. Ardic"));
    CHECK(cap_contains("Control Panel"));
    CHECK(cap_contains("\x1b[48;5;236m"));  /* zebra */
    CHECK(cap_contains("Select> "));
    CHECK(cap_contains("1000"));      /* PWM freq */
    CHECK(cap_contains("[X]"));       /* LED on, read through led_get */
    CHECK(cap_contains("SLEEP"));     /* sys mode */
    CHECK(cap_contains("50.0"));      /* duty */
    CHECK(cap_contains("..."));       /* prompt */
    CHECK(cap_contains("t0"));        /* tag choice */
    CHECK(cap_contains("253"));       /* temp readout */
    CHECK(cap_contains("40"));        /* humidity, scaled by the accessor */
    CHECK(!cap_contains("40%"));      /* PERCENT is gone */
    printf("full page: %u bytes\n", (unsigned)cap_len);
    golden("full_page");
}

static void test_full_page_noapp(void)
{
    start(&panel_page, NULL);
    CHECK(!cap_contains("MSP430 EnvMon"));
    CHECK(cap_contains("Control Panel"));
    golden("full_page_noapp");
}

static void test_sink_reject(void)
{
    static char ref[65536];
    size_t ref_len;

    start(&panel_page, &info);
    ref_len = cap_len;
    memcpy(ref, cap, cap_len);

    reset_vars();
    cap_reset();
    reject_alternate = 1;
    atc_menu_init(&ctx, &info, &panel_page, test_sink, NULL);
    CHECK(atc_menu_update(&ctx) == ATC_MENU_BUSY);  /* first line rejected */
    pump();
    reject_alternate = 0;
    CHECK(cap_len == ref_len && !memcmp(cap, ref, ref_len));
}

/* Accessor-bound rows are rebuilt from scratch on a retry, so a rejected line
 * must still come out byte-identical. */
static void test_sink_reject_accessors(void)
{
    static char ref[65536];
    size_t ref_len;

    start(&acc_page, &info);
    ref_len = cap_len;
    memcpy(ref, cap, cap_len);

    reset_vars();
    cap_reset();
    reject_alternate = 1;
    atc_menu_init(&ctx, &info, &acc_page, test_sink, NULL);
    pump();
    reject_alternate = 0;
    CHECK(cap_len == ref_len && !memcmp(cap, ref, ref_len));
}

static void test_idle_no_output(void)
{
    start(&panel_page, &info);
    cap_reset();
    pump();
    CHECK(cap_len == 0);
}

static void test_row_update(void)
{
    start(&panel_page, &info);
    cap_reset();
    feed("2");  /* one keypress, no Enter: toggles the LED through led_set */
    CHECK(led == 0);
    CHECK(!cap_contains("\x1b[2J"));
    CHECK(cap_contains("[ ]"));
    CHECK(cap_count("\x1b[K") == 1);  /* exactly one row repainted */
    printf("row update: %u bytes\n", (unsigned)call_sizes[0]);
    CHECK(call_sizes[0] < ATC_MENU_LINE_BUF);
}

static void test_number(void)
{
    /* 11 selectable items, so a lone 1 could still grow into 10 or 11: this is
     * the one case where Enter is still needed. */
    start(&panel_page, &info);
    cap_reset();
    feed("1\r");
    CHECK(cap_contains("PWM Freq (Hz)"));
    CHECK(!cap_contains("step"));       /* no range hint any more */
    feed("1900\r");
    CHECK(pwm_freq == 1900);
    CHECK(cap_contains("1900"));

    feed("1\r" "150\r");                /* no grid check: accepted */
    CHECK(pwm_freq == 150);

    feed("1\r" "12a\r");
    CHECK(cap_contains("Invalid value"));
    feed("\x03");
    CHECK(pwm_freq == 150);
}

static void test_number_no_range(void)
{
    start(&cmd_page, NULL);
    feed("7" "-2000000\r");              /* NULL callback accepts anything */
    CHECK(free_val == -2000000);
    CHECK(!cap_contains("Out of range"));
}

static void test_fixed(void)
{
    start(&panel_page, &info);
    feed("5" "33.5\r");
    CHECK(duty_x10 == 335);
    feed("5" "3.55\r");  /* one decimal digit allowed */
    CHECK(cap_contains("Invalid value"));
    feed("\x03");
    CHECK(duty_x10 == 335);
}

static void test_hex(void)
{
    start(&panel_page, &info);
    cap_reset();
    feed("7");
    CHECK(cap_contains("Control Panel / Registers"));
    CHECK(cap_contains("0x1A2B"));
    feed("1");  /* only 2 selectable items here: instant */
    CHECK(cap_contains("(hex, 16 bit)"));   /* HEX keeps its hint */
    feed("0xBEEF\r");
    CHECK(adcctl == 0xBEEF);
    feed("1" "12345\r");  /* 0x12345 needs more than 16 bits */
    CHECK(cap_contains("Out of range"));
    feed("\x03");
    feed("0");  /* back */
    CHECK(cap_count("\x1b[2J") >= 2);
}

static void test_prompt(void)
{
    start(&panel_page, &info);
    feed("6" "pwm on\r");
    CHECK(!strcmp(last_cmd, "pwm on"));
    feed("6" "bad\r");
    CHECK(cap_contains("Rejected"));
    feed("\x03");
    CHECK(strcmp(last_cmd, "bad") != 0);

    /* text into an application buffer: the PROMPT callback owns the copy */
    feed("4" "lab-node\r");
    CHECK(!strcmp(dev_name, "lab-node"));
}

static void test_choice_checkbox_action(void)
{
    start(&panel_page, &info);
    cap_reset();
    feed("3\r");  /* SLEEP -> IDLE (cycles past the end), Enter commits */
    CHECK(sys_mode == 0);
    CHECK(cap_contains("IDLE"));
    feed("8\r");  /* second choice list */
    CHECK(tag_idx == 1);
    CHECK(cap_contains("t1"));
    feed("11");  /* two digits, still no Enter */
    CHECK(saved == 7);                    /* the item's arg reached the handler */
    CHECK(cap_contains("Config saved"));
    feed("9");  /* readout select = row refresh */
    CHECK(temp_x10 == 253);
}

static void test_choice_preview(void)
{
    start(&panel_page, &info);
    cap_reset();
    feed("3");  /* SLEEP -> preview IDLE, not committed */
    CHECK(sys_mode == 2);
    CHECK(ctx.mode == ATC_MENU_MODE_CHOICE);
    CHECK(cap_contains("IDLE"));
    feed("z");  /* preview IDLE -> RUN */
    CHECK(sys_mode == 2);
    CHECK(cap_contains("RUN"));

    feed("\x1b");
    CHECK(sys_mode == 2);
    CHECK(ctx.mode == ATC_MENU_MODE_SELECT);
    feed("2");  /* normal selection still works */
    CHECK(led == 0);

    feed("3");
    CHECK(ctx.mode == ATC_MENU_MODE_CHOICE);
    feed("\x03");
    CHECK(sys_mode == 2);
    CHECK(ctx.mode == ATC_MENU_MODE_SELECT);

    start(&interlock_page, NULL);
    CHECK(choice_cb_calls == 0);
    feed("3");  /* AUTO -> preview MANUAL */
    CHECK(il_mode == 0);
    CHECK(choice_cb_calls == 0);
    feed("\r");
    CHECK(il_mode == 1);
    CHECK(choice_cb_calls == 1);

    /* A refused commit keeps the preview open, so Enter alone can retry it. */
    choice_cb_reject = 1;
    feed("3\r");  /* MANUAL -> preview AUTO, the setter refuses */
    CHECK(il_mode == 1);
    CHECK(choice_cb_calls == 2);
    CHECK(cap_contains("Rejected"));
    CHECK(ctx.mode == ATC_MENU_MODE_CHOICE);
    feed("\r");  /* retry while still rejecting */
    CHECK(il_mode == 1);
    CHECK(choice_cb_calls == 3);
    choice_cb_reject = 0;
    feed("\r");
    CHECK(il_mode == 0);
    CHECK(choice_cb_calls == 4);
}

/* The escape hatch: whatever the application can print and parse can be bound. */
static void test_custom(void)
{
    start(&custom_page, NULL);

    /* A float round-trips through text alone. */
    CHECK(cap_contains("1.50"));
    CHECK(last_show_arg == 7);          /* arg reaches show() */
    CHECK(show_cap_seen == ATC_MENU_CFG_VAL);
    feed("1" "2.25\r");
    CHECK(gain > 2.24f && gain < 2.26f);
    CHECK(ctx.mode == ATC_MENU_MODE_SELECT);
    CHECK(cap_contains("2.25"));        /* redrawn from show() */

    /* A refusing edit keeps the editor open and says why, as everywhere. */
    feed("1" "9.99\r");
    CHECK(cap_contains("Bad gain"));
    CHECK(gain > 2.24f && gain < 2.26f);
    CHECK(ctx.mode == ATC_MENU_MODE_EDIT);
    feed("\x03");

    /* A string, which no value tool could hold. */
    feed("2" "beta\r");
    CHECK(!strcmp(label_text, "beta"));
    CHECK(cap_contains("beta"));
    feed("2" "waytoolongforthis\r");
    CHECK(!strcmp(label_text, "beta"));
    CHECK(cap_contains("Rejected"));
    feed("\x03");

    /* No edit() is read-only: selecting refreshes rather than opening. */
    feed("3");
    CHECK(ctx.mode == ATC_MENU_MODE_SELECT);

    /* No show() is a PROMPT: "..." in the column, editor opens anyway. */
    CHECK(cap_contains("..."));
    feed("4");
    CHECK(ctx.mode == ATC_MENU_MODE_EDIT);
    feed("gamma\r");
    CHECK(!strcmp(label_text, "gamma"));

    /* A show() that overruns is held to the value column. */
    cap_reset();
    feed("r");
    CHECK(cap_contains("ZZZZ"));
    CHECK(!cap_contains("ZZZZZZZZZZZZZ"));   /* ATC_MENU_CFG_VAL is 12 */
}

/* What the editor guards is structural - 32 bits - and nothing else. */
static void test_ranges(void)
{
    start(&range_page, NULL);

    /* 1 Wide, 2 WideU, 3 Narrow, 4 Signed, 5 I32, 6 UFix, 7 UFixU */

    /* The whole unsigned range goes in, and the generated accessor stores it
     * as the uint32_t it really is. */
    feed("1" "4294967295\r");
    CHECK(wide_val == 4294967295u);
    CHECK(ctx.mode == ATC_MENU_MODE_SELECT);
    feed("1" "4294967296\r");            /* one past 32 bits */
    CHECK(cap_contains("Invalid value"));
    CHECK(wide_val == 4294967295u);
    feed("\x03");

    /* Only the edited row is redrawn, so ask for a full page to see the
     * ATC_MENU_UNSIGNED readout of the same bits the signed row shows as -1. */
    cap_reset();
    feed("r");
    CHECK(cap_contains("4294967295"));
    CHECK(cap_contains("-1"));

    /* A minus sign has to land inside int32_t, which is the one bound left. */
    feed("5" "-2147483647\r");
    CHECK(pwm_freq == -2147483647);
    feed("5" "-2147483648\r");
    CHECK(cap_contains("Invalid value"));
    CHECK(pwm_freq == -2147483647);
    feed("\x03");

    /* Narrower variables are the accessor's business, not the menu's: the
     * generated setter converts, and what the getter reads back is what fits. */
    feed("3" "65535\r");
    CHECK(adcctl == 65535);
    feed("4" "-32767\r");
    CHECK(narrow_val == -32767);

    /* Scaled and unsigned at once: what goes in past INT32_MAX comes back out
     * the same way, rather than wrapping negative on the way to the screen. */
    feed("6" "4294967.295\r");
    CHECK(wide_val == 4294967295u);
    cap_reset();
    feed("r");
    CHECK(cap_contains("4294967.295"));
    feed("6" "4294967.296\r");
    CHECK(cap_contains("Invalid value"));
    CHECK(wide_val == 4294967295u);
    feed("\x03");
}

static void test_setter_refuses(void)
{
    start(&interlock_page, NULL);
    feed("2");  /* the setter refuses and explains; no generic fallback */
    CHECK(cap_contains("Relay needs RUN mode"));
    CHECK(relay == 0);
    CHECK(cap_contains("[ ]"));   /* row redrawn from the getter */

    /* An editable row keeps the editor open on the entry it would not take. */
    feed("1" "50\r");
    CHECK(cap_contains("Setpoint locked"));
    CHECK(setpoint == 42);
    CHECK(ctx.mode == ATC_MENU_MODE_EDIT);
    feed("\x03");
    CHECK(ctx.mode == ATC_MENU_MODE_SELECT);
}

static void test_paging(void)
{
    start(&adc_page, NULL);
    CHECK(cap_contains("CH12"));
    CHECK(!cap_contains("CH13"));
    CHECK(cap_contains("Page"));
    cap_reset();
    feed("n");
    CHECK(cap_contains("CH13"));
    CHECK(cap_contains("CH15"));
    cap_reset();
    feed("p");
    CHECK(cap_contains("CH01"));
}

/* Counts are folded in by ATC_MENU_PAGE_END, so they are plain ROM data. */
static void test_page_counts(void)
{
    CHECK(reg_page.count == 3);
    CHECK(panel_page.count == 12);
    CHECK(adc_page.count == 15);
    CHECK(cmd_page.count == 7);
}

static void test_text_wrap(void)
{
    atc_menu_item_t it;
    char buf31[32], buf32[33], many[301];
    unsigned n;

    it.kind = ATC_MENU_KIND_TEXT;
    it.flags = 0;
    it.detail = 0;

    it.label = "";
    CHECK(atc_menu_item_rows(&it) == 1);         /* empty text: one blank row */

    it.label = "Short note.";
    CHECK(atc_menu_item_rows(&it) == 1);

    memset(buf31, 'a', 31);
    buf31[31] = '\0';
    it.label = buf31;
    CHECK(atc_menu_item_rows(&it) == 1);         /* exactly one row's width */

    memset(buf32, 'a', 32);
    buf32[32] = '\0';
    it.label = buf32;
    CHECK(atc_menu_item_rows(&it) == 2);         /* one char over: hard-break */

    it.label = "Sensor readings update every second automatically.";
    CHECK(atc_menu_item_rows(&it) == 2);         /* breaks at a space, not mid-word */

    it.label = bulletin_text;
    CHECK(atc_menu_item_rows(&it) == 3);

    /* No artificial line cap: keeps wrapping for as long as the text does. */
    for (n = 0; n < 50; n++)
        memcpy(many + n * 6, "abcde ", 6);
    many[300] = '\0';
    it.label = many;
    CHECK(atc_menu_item_rows(&it) == 10);
}

static void test_text_item(void)
{
    start(&text_page, NULL);
    CHECK(text_page.count == 5);
    CHECK(cap_contains("Alpha"));
    CHECK(cap_contains("Sensor"));
    CHECK(cap_contains("automatically."));
    CHECK(cap_contains("Beta"));
    CHECK(cap_contains("Configuration"));
    CHECK(cap_contains("effect."));
    CHECK(cap_contains("Gamma"));

    /* Alpha/Beta/Gamma (item positions 0/2/4) are the only zebra rows. */
    CHECK(cap_count("\x1b[48;5;236m") == 3);

    /* TEXT carries no selection number, so 2 still lands on Beta. */
    cap_reset();
    feed("2");
    CHECK(cap_contains("Beta"));
    feed("42\r");
    CHECK(free_val == 42);

    golden("text_page");
}

/* A TEXT item is never split across pages - it moves whole to the next one. */
static void test_text_paging(void)
{
    start(&text_paging_page, NULL);

    cap_reset();
    atc_menu_set_items_per_page(&ctx, 2);
    pump();
    CHECK(cap_contains("Intro"));
    CHECK(!cap_contains("abcde"));
    CHECK(!cap_contains("Value"));

    cap_reset();
    feed("n");
    CHECK(cap_contains("abcde"));
    CHECK(!cap_contains("Intro"));
    CHECK(!cap_contains("Value"));

    cap_reset();
    feed("n");
    CHECK(cap_contains("Value"));
    CHECK(!cap_contains("abcde"));
    CHECK(!cap_contains("Intro"));

    cap_reset();
    feed("p");
    CHECK(cap_contains("abcde"));
    CHECK(!cap_contains("Value"));

    cap_reset();
    feed("p");
    CHECK(cap_contains("Intro"));
    CHECK(!cap_contains("abcde"));
}

static void test_items_per_page_runtime(void)
{
    start(&adc_page, NULL);  /* test_paging covers the untouched default */

    cap_reset();
    atc_menu_set_items_per_page(&ctx, 5);   /* a 4-row LCD-sized window */
    pump();
    CHECK(cap_contains("CH05"));
    CHECK(!cap_contains("CH06"));
    CHECK(cap_contains("Page"));

    cap_reset();
    feed("n");
    CHECK(cap_contains("CH06"));
    CHECK(cap_contains("CH10"));
    CHECK(!cap_contains("CH05"));
    CHECK(!cap_contains("CH11"));

    cap_reset();
    feed("p");
    CHECK(cap_contains("CH01"));
    CHECK(cap_contains("CH05"));
    CHECK(!cap_contains("CH06"));

    /* A window wider than the page shows everything and drops the paging hint. */
    cap_reset();
    atc_menu_set_items_per_page(&ctx, 40);
    pump();
    CHECK(cap_contains("CH01"));
    CHECK(cap_contains("CH15"));
    CHECK(!cap_contains("Page"));
}

/* Resizing rewinds paging, so 'p' never faces a partial window. */
static void test_items_per_page_resize_rewinds(void)
{
    start(&adc_page, NULL);
    feed("n");                          /* offset 12, window 12 */
    cap_reset();
    atc_menu_set_items_per_page(&ctx, 20);  /* offset 12 would now underflow on 'p' */
    pump();
    CHECK(cap_contains("CH01"));
    cap_reset();
    feed("p");                          /* already at the top: nothing to do */
    CHECK(!cap_contains("\x1b[2J"));
}

static void test_items_per_page_clamp(void)
{
    start(&adc_page, NULL);
    cap_reset();
    atc_menu_set_items_per_page(&ctx, 0);
    CHECK(ctx.items_per_page == 1);
    pump();
    CHECK(cap_contains("CH01"));
    CHECK(!cap_contains("CH02"));

    atc_menu_set_items_per_page(&ctx, 9999);
    CHECK(ctx.items_per_page == ATC_MENU_ITEMS_PER_PAGE_MAX);
}

/* The 'i' command is the user-facing way to reach the same state the tests
 * above drive directly through atc_menu_set_items_per_page(). */
static void test_items_per_page_cmd(void)
{
    start(&adc_page, NULL);  /* 15 selectable READOUT rows, no Enter needed */

    cap_reset();
    feed("i");
    CHECK(ctx.mode == ATC_MENU_MODE_ITEMS_PER_PAGE);
    CHECK(cap_contains("Items/page"));
    CHECK(cap_contains("12"));  /* untouched default, shown in the prompt */

    feed("5\r");
    CHECK(ctx.mode == ATC_MENU_MODE_SELECT);
    CHECK(ctx.items_per_page == 5);
    CHECK(cap_contains("CH05"));
    CHECK(!cap_contains("CH06"));

    /* Cancel leaves the value untouched. */
    cap_reset();
    feed("i");
    feed("9\x1b");
    CHECK(ctx.mode == ATC_MENU_MODE_SELECT);
    CHECK(ctx.items_per_page == 5);

    feed("i");
    feed("7\x03");
    CHECK(ctx.mode == ATC_MENU_MODE_SELECT);
    CHECK(ctx.items_per_page == 5);

    /* Out-of-range input is clamped, not rejected. */
    feed("i");
    feed("9999\r");
    CHECK(ctx.items_per_page == ATC_MENU_ITEMS_PER_PAGE_MAX);

    /* Non-numeric input is rejected and the prompt stays open to retry. */
    atc_menu_set_items_per_page(&ctx, 5);
    cap_reset();
    feed("i");
    feed("x\r");
    CHECK(cap_contains("Invalid value"));
    CHECK(ctx.mode == ATC_MENU_MODE_ITEMS_PER_PAGE);
    feed("\x03");

    /* Global like back/refresh/help: works from inside a submenu too. */
    start(&panel_page, NULL);
    feed("7");  /* Registers submenu */
    CHECK(ctx.depth == 1);
    feed("i");
    feed("3\r");
    CHECK(ctx.items_per_page == 3);
    CHECK(ctx.depth == 1);  /* still inside the submenu */

    /* Mid-selection, 'i' is just an ignored non-digit, not a command. */
    start(&panel_page, NULL);  /* 11 selectable items -> "1" alone is pending */
    feed("1");
    feed("i");
    CHECK(ctx.mode == ATC_MENU_MODE_SELECT);
    feed("1");  /* "11" = Save Cfg */
    CHECK(saved == 7);
}

static void test_csi_swallow(void)
{
    start(&panel_page, &info);
    /* arrow key split across updates must not leak into the input */
    feed("\x1b");
    feed("[");
    feed("A");
    feed("2");
    CHECK(led == 0);
}

static void test_esc_cancel(void)
{
    start(&panel_page, &info);
    feed("6" "half-typed");    /* enter console edit, type something */
    feed("\x1b");              /* ESC exits the edit */
    feed("2");                 /* selection works again */
    CHECK(led == 0);
    CHECK(last_cmd[0] == '\0');
    feed("1\r");               /* arrow key while editing also cancels */
    feed("\x1b[A");
    feed("2");
    CHECK(led == 1);
    CHECK(pwm_freq == 1000);
}

/* At ROW_W=40 the footer cannot fit every hint; the lowest-priority one
 * (Items/page) is dropped instead of spilling past the box below it. */
static void test_footer_priority(void)
{
    start(&panel_page, &info);  /* single page: nothing to drop */
    CHECK(cap_contains("Items/page"));
    CHECK(!cap_contains("n/p"));

    start(&adc_page, NULL);  /* paginated: Items/page loses to Page */
    CHECK(cap_contains("n/p"));
    CHECK(cap_contains("Page"));
    CHECK(!cap_contains("Items/page"));
}

static void test_misc(void)
{
    start(&panel_page, &info);
    cap_reset();
    feed("\r");  /* enter on an empty selection: prompt redraw only */
    CHECK(!cap_contains("\x1b[2J"));
    feed("?");
    CHECK(cap_contains("0/b back"));
    feed("\x03");
    CHECK(cap_contains("Select> "));
    feed("12");  /* past the 11 selectable items */
    CHECK(cap_contains("Invalid selection"));
}

/* ---- instant selection --------------------------------------------------- */

/* A page of 9 or fewer selectable items is completely Enter-free. */
static void test_instant_select(void)
{
    start(&cmd_page, NULL);  /* 7 selectable items */
    feed("1");
    CHECK(last_arg == 11 && arg_calls == 1);
    feed("2");
    CHECK(last_arg == 22 && arg_calls == 2);

    cap_reset();
    feed("7");  /* a NUMBER: one keypress opens the editor */
    CHECK(cap_contains("Free"));
    feed("42\r");  /* entered text still ends with Enter */
    CHECK(free_val == 42);
}

/* From 10 selectable items on, a leading digit is genuinely ambiguous, so it
 * waits for the next digit instead of guessing. */
static void test_pending_prefix(void)
{
    start(&panel_page, &info);  /* 11 selectable items */
    cap_reset();
    feed("1");                  /* could still become 10 or 11 */
    CHECK(saved == 0);
    CHECK(pwm_freq == 1000);
    CHECK(cap_contains("Select> \x1b[0m" "1"));  /* echoed, nothing else */
    feed("1");                  /* 11 = Save Cfg, acts on the second digit */
    CHECK(saved == 7);

    cap_reset();
    feed("1\r");                /* Enter commits the lone 1 */
    CHECK(cap_contains("PWM Freq (Hz)"));
    feed("\x03");

    feed("1");                  /* Backspace drops a pending prefix */
    feed("\x08" "2");
    CHECK(led == 0);            /* 2 toggled the LED; 12 was never formed */
}

/* Out-of-range numbers are unambiguous too, so they fail without Enter. */
static void test_instant_badsel(void)
{
    start(&cmd_page, NULL);
    cap_reset();
    feed("8");
    CHECK(cap_contains("Invalid selection"));
    CHECK(arg_calls == 0);
}

/* There is no RX queue left to overflow: every byte is handled inside
 * atc_menu_key(), so a burst arriving between two atc_menu_update() calls
 * cannot drop a keypress. */
static void test_burst_no_drop(void)
{
    unsigned i;

    start(&cmd_page, NULL);
    for (i = 0; i < 40; i++)  /* no update() in between */
        atc_menu_key(&ctx, (i & 1u) ? '2' : '1');
    pump();
    CHECK(arg_calls == 40);
    CHECK(last_arg == 22);
}

/* ---- accessor tests ------------------------------------------------------ */

static void test_readout_formats(void)
{
    start(&acc_page, NULL);
    CHECK(cap_contains("253"));         /* DEC   */
    CHECK(cap_contains("4294967280"));  /* UDEC, full 32-bit range */
    CHECK(cap_contains("0x05"));        /* HEX8  */
    CHECK(cap_contains("0xFFF0"));      /* HEX16 */
    CHECK(cap_contains("330.0"));       /* FIX1  */
    CHECK(cap_contains("33.00"));       /* FIX2  */
    golden("acc_page");

    start(&cmd_page, NULL);
    CHECK(cap_contains("0xFFFFFFF0"));  /* HEX32 */
    CHECK(cap_contains("3.300"));       /* FIX3  */
}

static void test_readout_arg(void)
{
    /* One getter drives four rows; the bit index comes from the item's arg. */
    start(&acc_page, NULL);
    CHECK(get_calls == 4);              /* exactly one call per bound row */
    CHECK(cap_count("[X]") == 2);       /* bits 0 and 2 of 0x05 */
    CHECK(cap_count("[ ]") == 3);       /* bits 1, 3 and the NULL getter */
}

static void test_checkbox_rw(void)
{
    start(&acc_page, NULL);
    feed("1\r");                      /* 11 selectable: 1 still needs Enter */
    CHECK(port_sim == 0x04);
    cap_reset();
    feed("2");                        /* toggle bit 1, no Enter */
    CHECK(port_sim == 0x06);
    CHECK(!cap_contains("\x1b[2J"));    /* single row, not a full page */
    CHECK(cap_count("\x1b[K") == 1);
    CHECK(cap_contains("[X]"));
}

static void test_checkbox_readonly(void)
{
    start(&acc_page, NULL);
    cap_reset();
    feed("3");                        /* NULL setter: refresh only */
    CHECK(port_sim == 0x05);            /* nothing written */
    CHECK(!cap_contains("\x1b[2J"));
    CHECK(cap_count("\x1b[K") == 1);    /* the row was still repainted */
}

static void test_checkbox_veto(void)
{
    start(&acc_page, NULL);
    feed("4");
    CHECK(port_sim == 0x05);            /* the setter declined to write */
    CHECK(cap_contains("Locked"));
    CHECK(cap_contains("[ ]"));         /* row shows the state that holds */
}

static void test_not_editable(void)
{
    /* activate()'s default branch enters edit mode, so READOUT and CHECKBOX
     * must have explicit cases; if they fall through, the next digit would be
     * swallowed as edit text instead of selecting an item. */
    start(&acc_page, NULL);
    feed("7");                        /* a READOUT */
    feed("1\r");                      /* must still act as a selection */
    CHECK(port_sim == 0x04);

    start(&acc_page, NULL);
    feed("3");                        /* a read-only CHECKBOX */
    feed("1\r");
    CHECK(port_sim == 0x04);
}

static void test_action_arg(void)
{
    start(&cmd_page, NULL);
    feed("1");
    CHECK(last_arg == 11);
    feed("2");
    CHECK(last_arg == 22);
    CHECK(arg_calls == 2);
    feed("3");                        /* NULL handler must not crash */
    CHECK(arg_calls == 2);
}

static void test_accessor_null(void)
{
    start(&cmd_page, NULL);
    CHECK(cap_contains("NoRead"));      /* NULL reader renders as 0 */
    start(&acc_page, NULL);
    feed("5");                        /* NULL getter and setter */
    CHECK(port_sim == 0x05);
    CHECK(cap_contains("[ ]"));
}

int main(int argc, char **argv)
{
    update_golden = (argc > 1 && !strcmp(argv[1], "--update"));

    test_fmt();
    test_full_page();
    test_full_page_noapp();
    test_sink_reject();
    test_sink_reject_accessors();
    test_idle_no_output();
    test_row_update();
    test_number();
    test_number_no_range();
    test_fixed();
    test_hex();
    test_prompt();
    test_choice_checkbox_action();
    test_choice_preview();
    test_ranges();
    test_custom();
    test_setter_refuses();
    test_paging();
    test_page_counts();
    test_text_wrap();
    test_text_item();
    test_text_paging();
    test_items_per_page_runtime();
    test_items_per_page_resize_rewinds();
    test_items_per_page_clamp();
    test_items_per_page_cmd();
    test_csi_swallow();
    test_esc_cancel();
    test_footer_priority();
    test_misc();
    test_instant_select();
    test_pending_prefix();
    test_instant_badsel();
    test_burst_no_drop();

    test_readout_formats();
    test_readout_arg();
    test_checkbox_rw();
    test_checkbox_readonly();
    test_checkbox_veto();
    test_not_editable();
    test_action_arg();
    test_accessor_null();

    if (failures) {
        printf("%d FAILURE(S)\n", failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
