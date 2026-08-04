/**
 * @file main.c
 * @brief ATC Menu - demo styled as a real MCU control panel. The hardware
 *        (GPIO port, ADC channels, PWM timer, peripheral registers) is
 *        simulated so the demo runs on the host, but the menu tree and the
 *        accessors are exactly what an MSP430/C2000 application would write.
 *        Navigate with the number keys - no Enter - plus 0/b, r, ?, n/p, i.
 *
 * Usage:
 *   menu_demo               draw on the local terminal
 *   menu_demo COM3 115200   drive a terminal attached to a serial port
 *
 * @author Ahmet Talha ARDIC
 * @date   2026-07-31
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atc_menu/menu.h"
#include "port_serial.h"
#include "port_stdio.h"

/* ---- simulated hardware -------------------------------------------------- */

#define PIN(n) ((uint8_t)(1u << (n)))

/* Four 8-bit ports. A pin is addressed by one int32_t selector so that a single
 * getter/setter pair can drive all 32 rows - see GPIO() below. */
enum { PORT_COUNT = 4 };

static uint8_t p_out[PORT_COUNT];                              /* output latch   */
static uint8_t p_dir[PORT_COUNT] = { 0x0F, 0x3F, 0x0F, 0x0F }; /* 1 = output     */
static uint8_t p_pad[PORT_COUNT] = { 0x10, 0x40, 0x30, 0xA0 }; /* driven outside */

#define GPIO(port, bit) ((int32_t)(((port) << 3) | (bit)))
#define GPIO_PORT(sel)  ((int)((sel) >> 3))
#define GPIO_BIT(sel)   ((unsigned)((sel) & 7))

/* A real port reads back the latch on output pins and the pad on input pins,
 * so a single getter covers both directions. */
static uint8_t port_in(int p)
{
    return (uint8_t)((p_out[p] & p_dir[p]) | (p_pad[p] & (uint8_t)~p_dir[p]));
}

enum { CH_TEMP = 0, CH_HUM, CH_VBAT, CH_COUNT };

/* temp in 0.1 C, humidity in raw ADC counts, Vbat in 0.01 V */
static uint16_t adc_raw[CH_COUNT] = { 253, 1638, 371 };

static uint32_t sys_ticks = 0xFFFFFFF0u;    /* about to wrap - shows u32 off */

static int32_t pwm_freq = 1000;             /* Hz          */
static int32_t duty_x10 = 500;              /* 50.0 %      */
static int32_t ta0ccr0  = 1000;             /* derived     */
static atc_menu_u8 pwm_en;

enum { REG_ADCCTL0 = 0, REG_UCA0BRW };

static uint32_t adcctl0 = 0x0010;
static uint32_t uca0brw = 0x0034;

static atc_menu_u8 sys_mode = 1;            /* IDLE / RUN / SLEEP */
enum { MODE_IDLE = 0, MODE_RUN, MODE_SLEEP };

static int running = 1;
static atc_menu_ctx_t ctx;

/* In serial mode the local console is free, so every screen update reports
 * how many bytes went out to draw it (full page vs. single row is visible). */
static size_t tx_total;

static int counting_serial_sink(void *user, const char *buf, size_t len)
{
    if (!atc_menu_port_serial_sink(user, buf, len))
        return 0;
    tx_total += len;
    return 1;
}

/* ---- accessors ----------------------------------------------------------- */

static int32_t gpio_get(int32_t arg)
{
    return (port_in(GPIO_PORT(arg)) >> GPIO_BIT(arg)) & 1;
}

static int gpio_set(int32_t arg, int32_t value)
{
    uint8_t mask = PIN(GPIO_BIT(arg));

    if (value)
        p_out[GPIO_PORT(arg)] |= mask;
    else
        p_out[GPIO_PORT(arg)] &= (uint8_t)~mask;
    return 1;
}

/* Refuses in the wrong system mode: writes nothing, explains why, returns 0. */
static int relay_set(int32_t arg, int32_t value)
{
    if (value && sys_mode != MODE_RUN) {
        atc_menu_message(&ctx, "Relay needs RUN mode", 1);
        return 0;
    }
    return gpio_set(arg, value);
}

static int32_t rd_adc(int32_t arg)
{
    return (int32_t)adc_raw[arg];
}

static int32_t rd_hum_pct(int32_t arg)
{
    (void)arg;
    return (int32_t)adc_raw[CH_HUM] * 100 / 4095;
}

/* Same idea one level up: the selector picks port and register, so all six
 * register rows share one accessor. */
enum { REG_OUT = 0, REG_IN, REG_DIR };

#define REG(port, kind) ((int32_t)(((port) << 2) | (kind)))

static int32_t rd_reg(int32_t arg)
{
    int p = (int)(arg >> 2);

    switch (arg & 3) {
    case REG_OUT: return (int32_t)p_out[p];
    case REG_IN:  return (int32_t)port_in(p);
    default:      return (int32_t)p_dir[p];
    }
}

/* Nothing to validate on the way out, so the getters are generated. The
 * uint32_t counter converts modularly; its row reads it back unsigned. */
ATC_MENU_DEFINE_GETTER(ticks,   sys_ticks)
ATC_MENU_DEFINE_GETTER(ta0ccr0, ta0ccr0)
ATC_MENU_DEFINE_GETTER(pwm,     pwm_en)

static int pwm_set(int32_t arg, int32_t value)
{
    (void)arg;
    if (value && sys_mode == MODE_SLEEP) {
        atc_menu_message(&ctx, "Cannot start PWM in SLEEP", 1);
        return 0;
    }
    pwm_en = (atc_menu_u8)value;
    return 1;
}

/* ---- settings the firmware owns ------------------------------------------ *
 * In RAM rather than in a register, but reached the same way as the pins
 * above. Validation and the store are one step: returning 0 wrote nothing. */

ATC_MENU_DEFINE_GETTER(freq, pwm_freq)

static int freq_set(int32_t arg, int32_t value)
{
    (void)arg;
    if (value < 100 || value > 20000) {
        atc_menu_message(&ctx, "Range 100..20000 Hz", 1);
        return 0;
    }
    pwm_freq = value;
    ta0ccr0 = 1000000 / value;   /* 1 MHz timer clock */
    return 1;
}

ATC_MENU_DEFINE_GETTER(duty, duty_x10)

static int duty_set(int32_t arg, int32_t value)
{
    (void)arg;
    if (value < 0 || value > 1000) {
        atc_menu_message(&ctx, "Range 0.0..100.0 %", 1);
        return 0;
    }
    duty_x10 = value;
    return 1;
}

ATC_MENU_DEFINE_GETTER(mode, sys_mode)

static int mode_set(int32_t arg, int32_t value)
{
    (void)arg;
    if (value == MODE_SLEEP && pwm_en) {
        atc_menu_message(&ctx, "Disable PWM before SLEEP", 1);
        return 0;
    }
    sys_mode = (atc_menu_u8)value;
    return 1;
}

/* Both HEX rows share one pair; the selector picks the register. */
static int32_t rd_periph(int32_t arg)
{
    return (int32_t)(arg == REG_ADCCTL0 ? adcctl0 : uca0brw);
}

static int wr_periph(int32_t arg, int32_t value)
{
    if (arg == REG_ADCCTL0)
        adcctl0 = (uint32_t)value;
    else
        uca0brw = (uint32_t)value;
    return 1;
}

/* ---- CUSTOM rows --------------------------------------------------------- *
 * A float and a string: the application writes the text and parses the entry. */

static float cal_gain = 1.025f;
static char dev_name[13] = "envmon-01";

/* Unsigned to text, zero padded to `pad` digits - a firmware has no printf. */
static unsigned u_text(char *out, uint32_t v, unsigned pad)
{
    char t[10];
    unsigned n = 0, i;

    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n < pad)
        t[n++] = '0';
    for (i = 0; i < n; i++)
        out[i] = t[n - 1 - i];
    return n;
}

static unsigned gain_show(int32_t arg, char *out, unsigned cap)
{
    uint32_t scaled = (uint32_t)(cal_gain * 10000.0f + 0.5f);
    unsigned n;

    (void)arg;
    if (cap < 7)
        return 0;
    n = u_text(out, scaled / 10000u, 0);
    out[n++] = '.';
    return n + u_text(out + n, scaled % 10000u, 4);
}

static int gain_edit(int32_t arg, const char *text)
{
    float v = (float)atof(text);   /* a firmware would parse this by hand */

    (void)arg;
    if (v < 0.5f || v > 2.0f) {
        atc_menu_message(&ctx, "Gain 0.5000..2.0000", 1);
        return 0;
    }
    cal_gain = v;
    return 1;
}

static unsigned name_show(int32_t arg, char *out, unsigned cap)
{
    unsigned n = 0;

    (void)arg;
    while (dev_name[n] && n < cap) {
        out[n] = dev_name[n];
        n++;
    }
    return n;
}

static int name_edit(int32_t arg, const char *text)
{
    (void)arg;
    if (!*text || strlen(text) >= sizeof dev_name) {
        atc_menu_message(&ctx, "Name 1..12 chars", 1);
        return 0;
    }
    strcpy(dev_name, text);
    return 1;
}

/* ---- commands ------------------------------------------------------------ */

enum { CMD_SAMPLE = 1, CMD_CALIBRATE, CMD_CLEAR_OUT, CMD_EXIT };

static void on_cmd(int32_t arg)
{
    switch (arg) {
    case CMD_SAMPLE:
        /* pretend we triggered an ADC burst and got fresh readings */
        adc_raw[CH_TEMP] = (uint16_t)(adc_raw[CH_TEMP] + 4);
        adc_raw[CH_HUM]  = (uint16_t)((adc_raw[CH_HUM] + 137u) % 4096u);
        atc_menu_message(&ctx, "Sampled", 0);
        atc_menu_refresh(&ctx);
        break;
    case CMD_CALIBRATE:
        adc_raw[CH_VBAT] = 371;
        atc_menu_message(&ctx, "Calibrated", 0);
        atc_menu_refresh(&ctx);
        break;
    case CMD_CLEAR_OUT:
        memset(p_out, 0, sizeof p_out);
        atc_menu_refresh(&ctx);
        break;
    case CMD_EXIT:
        running = 0;
        break;
    }
}

static int on_console(int32_t arg, const char *text)
{
    (void)arg;
    if (!strcmp(text, "led on")) {
        gpio_set(GPIO(0, 0), 1);
    } else if (!strcmp(text, "led off")) {
        gpio_set(GPIO(0, 0), 0);
    } else if (!strcmp(text, "sample")) {
        on_cmd(CMD_SAMPLE);
        return 1;
    } else {
        atc_menu_message(&ctx, "Unknown cmd (led on|led off|sample)", 1);
        return 0;
    }
    atc_menu_message(&ctx, "OK", 0);
    atc_menu_refresh(&ctx);
    return 1;
}

/* ---- menu tree ----------------------------------------------------------- */

ATC_MENU_CHOICES(sys_modes, "IDLE", "RUN", "SLEEP");

/* One page, four ports' worth of pins driven by a single gpio_get()/
 * gpio_set() pair. Spans several screens at the default 12 rows - press
 * n/p, or press i to change how many rows fit a page. */
ATC_MENU_PAGE_BEGIN(io_page, "I/O")
    ATC_MENU_TEXT     ("Four GPIO ports, each with outputs, inputs "
                       "and register readouts.")
    ATC_MENU_LABEL    ("P1 Outputs")
    ATC_MENU_CHECKBOX ("P1.0 LED",       gpio_get, gpio_set,  GPIO(0, 0))
    ATC_MENU_CHECKBOX ("P1.1 Fan",       gpio_get, gpio_set,  GPIO(0, 1))
    ATC_MENU_CHECKBOX ("P1.2 Heater",    gpio_get, gpio_set,  GPIO(0, 2))
    ATC_MENU_CHECKBOX ("P1.3 Relay",     gpio_get, relay_set, GPIO(0, 3))
    ATC_MENU_LABEL    ("P1 Inputs")
    ATC_MENU_CHECKBOX ("P1.4 Btn0",      gpio_get, NULL, GPIO(0, 4))
    ATC_MENU_CHECKBOX ("P1.5 Btn1",      gpio_get, NULL, GPIO(0, 5))
    ATC_MENU_CHECKBOX ("P1.6 Limit",     gpio_get, NULL, GPIO(0, 6))
    ATC_MENU_CHECKBOX ("P1.7 EStop",     gpio_get, NULL, GPIO(0, 7))
    ATC_MENU_SEPARATOR()
    ATC_MENU_READOUT  ("P1OUT",          rd_reg, REG(0, REG_OUT), ATC_MENU_HEX8)
    ATC_MENU_READOUT  ("P1IN",           rd_reg, REG(0, REG_IN),  ATC_MENU_HEX8)
    ATC_MENU_READOUT  ("P1DIR",          rd_reg, REG(0, REG_DIR), ATC_MENU_HEX8)

    ATC_MENU_TEXT     ("Port 2 covers process I/O: valves, pump, "
                       "alarm and buzzer outputs, plus door and "
                       "tamper inputs.")
    ATC_MENU_LABEL    ("P2 Outputs")
    ATC_MENU_CHECKBOX ("P2.0 Valve A",   gpio_get, gpio_set, GPIO(1, 0))
    ATC_MENU_CHECKBOX ("P2.1 Valve B",   gpio_get, gpio_set, GPIO(1, 1))
    ATC_MENU_CHECKBOX ("P2.2 Pump",      gpio_get, gpio_set, GPIO(1, 2))
    ATC_MENU_CHECKBOX ("P2.3 Alarm",     gpio_get, gpio_set, GPIO(1, 3))
    ATC_MENU_CHECKBOX ("P2.4 Buzzer",    gpio_get, gpio_set, GPIO(1, 4))
    ATC_MENU_CHECKBOX ("P2.5 Backlight", gpio_get, gpio_set, GPIO(1, 5))
    ATC_MENU_LABEL    ("P2 Inputs")
    ATC_MENU_CHECKBOX ("P2.6 Door",      gpio_get, NULL, GPIO(1, 6))
    ATC_MENU_CHECKBOX ("P2.7 Tamper",    gpio_get, NULL, GPIO(1, 7))
    ATC_MENU_SEPARATOR()
    ATC_MENU_READOUT  ("P2OUT",          rd_reg, REG(1, REG_OUT), ATC_MENU_HEX8)
    ATC_MENU_READOUT  ("P2IN",           rd_reg, REG(1, REG_IN),  ATC_MENU_HEX8)
    ATC_MENU_READOUT  ("P2DIR",          rd_reg, REG(1, REG_DIR), ATC_MENU_HEX8)

    ATC_MENU_TEXT     ("Port 3 drives the motor stage: enable, "
                       "direction, brake and clutch outputs.")
    ATC_MENU_LABEL    ("P3 Outputs")
    ATC_MENU_CHECKBOX ("P3.0 Motor En",  gpio_get, gpio_set, GPIO(2, 0))
    ATC_MENU_CHECKBOX ("P3.1 Motor Dir", gpio_get, gpio_set, GPIO(2, 1))
    ATC_MENU_CHECKBOX ("P3.2 Brake",     gpio_get, gpio_set, GPIO(2, 2))
    ATC_MENU_CHECKBOX ("P3.3 Clutch",    gpio_get, gpio_set, GPIO(2, 3))
    ATC_MENU_LABEL    ("P3 Inputs")
    ATC_MENU_CHECKBOX ("P3.4 Home",      gpio_get, NULL, GPIO(2, 4))
    ATC_MENU_CHECKBOX ("P3.5 Index",     gpio_get, NULL, GPIO(2, 5))
    ATC_MENU_CHECKBOX ("P3.6 Fault",     gpio_get, NULL, GPIO(2, 6))
    ATC_MENU_CHECKBOX ("P3.7 Ready",     gpio_get, NULL, GPIO(2, 7))
    ATC_MENU_SEPARATOR()
    ATC_MENU_READOUT  ("P3OUT",          rd_reg, REG(2, REG_OUT), ATC_MENU_HEX8)
    ATC_MENU_READOUT  ("P3IN",           rd_reg, REG(2, REG_IN),  ATC_MENU_HEX8)
    ATC_MENU_READOUT  ("P3DIR",          rd_reg, REG(2, REG_DIR), ATC_MENU_HEX8)

    ATC_MENU_TEXT     ("Port 4 is the analog front-end: channel "
                       "and gain select outputs.")
    ATC_MENU_LABEL    ("P4 Outputs")
    ATC_MENU_CHECKBOX ("P4.0 Ch1 Sel",   gpio_get, gpio_set, GPIO(3, 0))
    ATC_MENU_CHECKBOX ("P4.1 Ch2 Sel",   gpio_get, gpio_set, GPIO(3, 1))
    ATC_MENU_CHECKBOX ("P4.2 Gain A",    gpio_get, gpio_set, GPIO(3, 2))
    ATC_MENU_CHECKBOX ("P4.3 Gain B",    gpio_get, gpio_set, GPIO(3, 3))
    ATC_MENU_LABEL    ("P4 Inputs")
    ATC_MENU_CHECKBOX ("P4.4 Overrange", gpio_get, NULL, GPIO(3, 4))
    ATC_MENU_CHECKBOX ("P4.5 Zero Det",  gpio_get, NULL, GPIO(3, 5))
    ATC_MENU_CHECKBOX ("P4.6 Ref OK",    gpio_get, NULL, GPIO(3, 6))
    ATC_MENU_CHECKBOX ("P4.7 Cal Done",  gpio_get, NULL, GPIO(3, 7))
    ATC_MENU_SEPARATOR()
    ATC_MENU_READOUT  ("P4OUT",          rd_reg, REG(3, REG_OUT), ATC_MENU_HEX8)
    ATC_MENU_READOUT  ("P4IN",           rd_reg, REG(3, REG_IN),  ATC_MENU_HEX8)
    ATC_MENU_READOUT  ("P4DIR",          rd_reg, REG(3, REG_DIR), ATC_MENU_HEX8)

    ATC_MENU_SEPARATOR()
    ATC_MENU_ACTION   ("Clear outputs",  on_cmd, CMD_CLEAR_OUT)
ATC_MENU_PAGE_END(io_page)

ATC_MENU_PAGE_BEGIN(sensors_page, "Sensors")
    ATC_MENU_TEXT     ("Live ADC readings; Sample Now and Calibrate refresh them.")
    ATC_MENU_READOUT  ("Temp (C)",     rd_adc,     CH_TEMP, ATC_MENU_FIX1)
    ATC_MENU_READOUT  ("Vbat (V)",     rd_adc,     CH_VBAT, ATC_MENU_FIX2)
    ATC_MENU_READOUT  ("Humidity (%)", rd_hum_pct, 0,       ATC_MENU_DEC)
    ATC_MENU_READOUT  ("Hum (raw)",    rd_adc,     CH_HUM,  ATC_MENU_DEC)
    ATC_MENU_SEPARATOR()
    ATC_MENU_ACTION   ("Sample Now",   on_cmd, CMD_SAMPLE)
    ATC_MENU_ACTION   ("Calibrate",    on_cmd, CMD_CALIBRATE)
ATC_MENU_PAGE_END(sensors_page)

ATC_MENU_PAGE_BEGIN(pwm_page, "PWM")
    ATC_MENU_TEXT    ("PWM timer settings; changing Freq recomputes TA0CCR0.")
    ATC_MENU_CHECKBOX("Enable",    pwm_get, pwm_set, 0)
    ATC_MENU_NUMBER  ("Freq (Hz)", freq_get, freq_set, 0)
    ATC_MENU_FIXED   ("Duty (%)",  duty_get, duty_set, 0, 1)
    ATC_MENU_READOUT ("TA0CCR0",   ta0ccr0_get, 0, ATC_MENU_DEC)
ATC_MENU_PAGE_END(pwm_page)

ATC_MENU_PAGE_BEGIN(reg_page, "Registers")
    ATC_MENU_TEXT   ("Raw peripheral view; read-only except the two HEX fields.")
    ATC_MENU_HEX    ("ADCCTL0",      rd_periph, wr_periph, REG_ADCCTL0, 16)
    ATC_MENU_HEX    ("UCA0BRW",      rd_periph, wr_periph, REG_UCA0BRW, 16)
    ATC_MENU_READOUT("Uptime",       ticks_get, 0, ATC_MENU_DEC | ATC_MENU_UNSIGNED)
    ATC_MENU_READOUT("Uptime (hex)", ticks_get, 0, ATC_MENU_HEX32)
ATC_MENU_PAGE_END(reg_page)

ATC_MENU_PAGE_BEGIN(main_page, "Main")
    ATC_MENU_LABEL    ("System")
    ATC_MENU_TEXT     ("Demo menu for an MCU control panel - text "
                       "wraps across rows without breaking the zebra "
                       "stripe.")
    ATC_MENU_CHOICE   ("Sys Mode",  mode_get, mode_set, 0, sys_modes)
    ATC_MENU_CUSTOM   ("Device",    name_show, name_edit, 0)   /* a string */
    ATC_MENU_CUSTOM   ("Cal Gain",  gain_show, gain_edit, 0)   /* a float  */
    ATC_MENU_PROMPT   ("Console",   on_console, 0) /* led on|led off|sample */
    ATC_MENU_SEPARATOR()
    ATC_MENU_SUBMENU  ("I/O",       io_page)
    ATC_MENU_SUBMENU  ("Sensors",   sensors_page)
    ATC_MENU_SUBMENU  ("PWM",       pwm_page)
    ATC_MENU_SUBMENU  ("Registers", reg_page)
    ATC_MENU_SEPARATOR()
    ATC_MENU_ACTION   ("Exit",      on_cmd, CMD_EXIT)
ATC_MENU_PAGE_END(main_page)

int main(int argc, char **argv)
{
    static const atc_menu_info_t info = { "MSP430 EnvMon", "v0.2", "ahmettardic" };
    int serial = argc >= 3;
    atc_menu_sink_fn sink;
    int key;
    unsigned tick = 0;

    if (argc != 1 && argc != 3) {
        fprintf(stderr, "usage: %s [port baud]   e.g. %s COM3 115200\n",
                argv[0], argv[0]);
        return 1;
    }
    if (serial) {
        unsigned long baud = strtoul(argv[2], NULL, 10);
        if (!baud || !atc_menu_port_serial_open(argv[1], baud)) {
            fprintf(stderr, "cannot open %s @ %s\n", argv[1], argv[2]);
            return 1;
        }
        sink = counting_serial_sink;
        printf("serving menu on %s @ %s, press the Exit item to quit\n",
               argv[1], argv[2]);
    } else {
        atc_menu_port_stdio_init();
        sink = atc_menu_port_stdio_sink;
    }

    atc_menu_init(&ctx, &info, &main_page, sink, NULL);

    while (running) {
        size_t tx_before = tx_total;

        while ((key = serial ? atc_menu_port_serial_getkey()
                             : atc_menu_port_stdio_getkey()) >= 0)
            atc_menu_key(&ctx, (char)key);
        if (atc_menu_update(&ctx) == ATC_MENU_IDLE) {
            /* Idle: hand the CPU back, but come straight back when a key
             * lands, so no keypress ever waits out a sleep. The bound keeps
             * the simulated hardware below drifting at the same rate. On an
             * MCU this is where LPM would be entered instead. */
            if (serial)
                atc_menu_port_stdio_sleep_ms(10);
            else
                atc_menu_port_stdio_wait_ms(10);
        }
        if (serial && tx_total != tx_before)
            printf("TX %4u B  (total %u B)\n",
                   (unsigned)(tx_total - tx_before), (unsigned)tx_total);

        /* The hardware drifts in the background; the menu never repaints on
         * its own, so press 'r' (or Sample Now) to see the fresh values. */
        sys_ticks++;
        if (++tick % 100 == 0) {
            adc_raw[CH_TEMP] = (uint16_t)(adc_raw[CH_TEMP] + ((tick % 300) ? 1 : -4));
            adc_raw[CH_HUM]  = (uint16_t)((adc_raw[CH_HUM] + 37u) % 4096u);
        }
        if (tick % 500 == 0)
            p_pad[0] ^= PIN(4);         /* Btn0 being pressed and released */
    }

    sink(NULL, "\x1b[0m\x1b[2J\x1b[H", 10);
    if (serial)
        atc_menu_port_serial_close();
    else
        atc_menu_port_stdio_deinit();
    return 0;
}
