/* SPDX-License-Identifier: MIT */
/**
 * @file main.c
 * @brief The reference control-panel menu, declared as immediate-mode code
 *
 * The hardware is simulated so this runs on a host, but the tree and the
 * handling are what an MSP430/C2000 application would write — and so is the
 * output path: the menu goes out of a serial port to a terminal that knows
 * nothing about this process, which is the only arrangement the library is
 * for. The console this is started from stays free and reports the byte cost
 * of every update, and so does the last row of every page.
 *
 *   menu_demo /dev/ttyUSB0 115200
 *
 * With no hardware to hand, socat gives you both ends of a virtual line:
 *
 *   socat -d -d pty,raw,echo=0,link=/tmp/ttyA pty,raw,echo=0,link=/tmp/ttyB &
 *   menu_demo /tmp/ttyA 115200 &
 *   screen /tmp/ttyB 115200
 */
#include "atc_menu/menu.h"
#include "port_serial.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---- simulated hardware ------------------------------------------------- */

enum { PORT_COUNT = 4 };

static uint8_t p_out[PORT_COUNT];
static uint8_t p_dir[PORT_COUNT] = { 0x0Fu, 0x3Fu, 0x0Fu, 0x0Fu };
static uint8_t p_pad[PORT_COUNT] = { 0x10u, 0x40u, 0x30u, 0xA0u };

/* A port reads back the latch on output pins and the pad on input pins. */
static uint8_t port_in(unsigned p)
{
    return (uint8_t)((p_out[p] & p_dir[p]) | (p_pad[p] & (uint8_t)~p_dir[p]));
}

enum { CH_TEMP = 0, CH_HUM, CH_VBAT, CH_COUNT };

static uint16_t adc_raw[CH_COUNT] = { 253u, 1638u, 371u };
static uint32_t sys_ticks = 0xFFFFFFF0u;

/* Trims correct a reading in either direction, so these are the signed rows. */
static int32_t temp_trim_x10 = -25; /* -2.5 C, one decimal */
static int16_t hum_zero;

static uint16_t pwm_freq = 1000u;
static int32_t  duty_x10 = 500;
static uint16_t ta0ccr0 = 1000u;
static bool     pwm_en;

static uint16_t adcctl0 = 0x0010u;
static uint16_t uca0brw = 0x0034u;

static char     dev_name[13] = "envmon-01";
static int32_t  cal_gain = 10250; /* 1.0250, four decimals */
static unsigned sys_mode = 1u;
enum { MODE_IDLE = 0, MODE_RUN, MODE_SLEEP };
static const char *const SYS_MODES[3] = { "IDLE", "RUN", "SLEEP" };

static int running = 1;

static const char *const P1_OUT[4] = { "P1.0 LED", "P1.1 Fan", "P1.2 Heater",
                                       "P1.3 Relay" };
static const char *const P1_IN[4] = { "P1.4 Btn0", "P1.5 Btn1", "P1.6 Limit",
                                      "P1.7 EStop" };
static const char *const P2_OUT[6] = { "P2.0 Valve A", "P2.1 Valve B",
                                       "P2.2 Pump",    "P2.3 Alarm",
                                       "P2.4 Buzzer",  "P2.5 Backlight" };
static const char *const P2_IN[2] = { "P2.6 Door", "P2.7 Tamper" };
static const char *const P3_OUT[4] = { "P3.0 Motor En", "P3.1 Motor Dir",
                                       "P3.2 Brake", "P3.3 Clutch" };
static const char *const P3_IN[4] = { "P3.4 Home", "P3.5 Index", "P3.6 Fault",
                                      "P3.7 Ready" };
static const char *const P4_OUT[4] = { "P4.0 Ch1 Sel", "P4.1 Ch2 Sel",
                                       "P4.2 Gain A", "P4.3 Gain B" };
static const char *const P4_IN[4] = { "P4.4 Overrange", "P4.5 Zero Det",
                                      "P4.6 Ref OK", "P4.7 Cal Done" };

/* ---- what the menu costs on the wire ------------------------------------ */

/* A row that counted its own repaint would feed itself and never reach 0, so
   what it spends on itself — its bytes, and the cursor restore frame_end pairs
   with them — comes back out of the figure. */
static size_t   tx_total;
static size_t   tx_self;
static uint32_t tx_last;
static unsigned tx_hold;

enum { TX_HOLD_PASSES = 50u,    /* ~1 s: a live figure is gone before it reads */
       TX_CURSOR_RESTORE = 2u };

static void tx_footer(atc_menu_ctx_t *c)
{
    size_t before = tx_total;

    atc_menu_separator(c);
    atc_menu_uint32_ro(c, "TX last (B)", tx_last);
    if (tx_total != before)
        tx_self += tx_total - before + TX_CURSOR_RESTORE;
}

/* ---- pages -------------------------------------------------------------- */

static void pin_group(atc_menu_ctx_t *c, unsigned port, const char *out_head,
                      const char *const *outs, unsigned n_out,
                      const char *in_head, const char *const *ins,
                      unsigned n_in, unsigned in_first)
{
    unsigned i;

    atc_menu_label(c, out_head);
    for (i = 0u; i < n_out; ++i) {
        bool on = ((p_out[port] >> i) & 1u) != 0u;
        /* The relay needs RUN mode, so it is offered only there. */
        if (port == 0u && i == 3u)
            atc_menu_item_enable(c, sys_mode == MODE_RUN);
        if (atc_menu_bool(c, outs[i], &on))
            p_out[port] = (uint8_t)(on ? (p_out[port] | (1u << i))
                                       : (p_out[port] & ~(1u << i)));
    }

    atc_menu_label(c, in_head);
    for (i = 0u; i < n_in; ++i)
        atc_menu_bool_ro(c, ins[i], ((port_in(port) >> (in_first + i)) & 1u) != 0u);
}

static void page_io(atc_menu_ctx_t *c)
{
    atc_menu_label(c, "Four GPIO ports: outputs, inputs and registers.");

    pin_group(c, 0u, "P1 Outputs", P1_OUT, 4u, "P1 Inputs", P1_IN, 4u, 4u);
    atc_menu_separator(c);
    atc_menu_hex8_ro(c, "P1OUT", p_out[0]);
    atc_menu_hex8_ro(c, "P1IN", port_in(0u));
    atc_menu_hex8_ro(c, "P1DIR", p_dir[0]);

    atc_menu_label(c, "Port 2 drives process I/O: valves, pump, alarm.");
    pin_group(c, 1u, "P2 Outputs", P2_OUT, 6u, "P2 Inputs", P2_IN, 2u, 6u);
    atc_menu_separator(c);
    atc_menu_hex8_ro(c, "P2OUT", p_out[1]);
    atc_menu_hex8_ro(c, "P2IN", port_in(1u));
    atc_menu_hex8_ro(c, "P2DIR", p_dir[1]);

    atc_menu_label(c, "Port 3 drives the motor stage.");
    pin_group(c, 2u, "P3 Outputs", P3_OUT, 4u, "P3 Inputs", P3_IN, 4u, 4u);
    atc_menu_separator(c);
    atc_menu_hex8_ro(c, "P3OUT", p_out[2]);
    atc_menu_hex8_ro(c, "P3IN", port_in(2u));
    atc_menu_hex8_ro(c, "P3DIR", p_dir[2]);

    atc_menu_label(c, "Port 4 is the analog front-end.");
    pin_group(c, 3u, "P4 Outputs", P4_OUT, 4u, "P4 Inputs", P4_IN, 4u, 4u);
    atc_menu_separator(c);
    atc_menu_hex8_ro(c, "P4OUT", p_out[3]);
    atc_menu_hex8_ro(c, "P4IN", port_in(3u));
    atc_menu_hex8_ro(c, "P4DIR", p_dir[3]);

    atc_menu_separator(c);
    if (atc_menu_action(c, "Clear outputs"))
        memset(p_out, 0, sizeof p_out);

    tx_footer(c);
}

static void sample(atc_menu_ctx_t *c)
{
    adc_raw[CH_TEMP] = (uint16_t)(adc_raw[CH_TEMP] + 4u);
    adc_raw[CH_HUM] = (uint16_t)((adc_raw[CH_HUM] + 137u) % 4096u);
    atc_menu_message(c, "Sampled");
}

static void page_sensors(atc_menu_ctx_t *c)
{
    int32_t temp_x10 = (int32_t)adc_raw[CH_TEMP] + temp_trim_x10;
    int32_t hum_cnt  = (int32_t)adc_raw[CH_HUM] + hum_zero;
    /* Past the band the row says so in its colour, not in a message line. */
    bool    hot      = temp_x10 > 300;

    if (hum_cnt < 0)
        hum_cnt = 0; /* a trim can pull the count under the rail */

    atc_menu_label(c, "Live ADC readings; Sample Now refreshes them.");
    if (hot)
        atc_menu_item_style(c, ATC_MENU_BOLD | ATC_MENU_FG_RED);
    atc_menu_fix_ro(c, "Temp (C)", temp_x10, 1u);
    atc_menu_item_style(c, hot ? (ATC_MENU_BOLD | ATC_MENU_FG_RED)
                               : ATC_MENU_FG_GREEN);
    atc_menu_text_ro(c, "Thermal", hot ? "OVER BAND" : "OK");
    atc_menu_fix_ro(c, "Vbat (V)", adc_raw[CH_VBAT], 2u);
    atc_menu_uint16_ro(c, "Humidity (%)", (uint16_t)(hum_cnt * 100 / 4095));
    atc_menu_uint16_ro(c, "Hum (raw)", (uint16_t)hum_cnt);

    /* Both trims take a leading '-', and they act on the rows above: dialling
       Temp Trim down walks the reading back under the band. */
    atc_menu_label(c, "Calibration trim");
    {   /* the local keeps a refused value out of temp_trim_x10 entirely */
        int32_t trim = temp_trim_x10;

        if (atc_menu_fix(c, "Temp Trim (C)", &trim, 1u)) {
            if (trim < -500 || trim > 500)
                atc_menu_reject(c, "Range -50.0..+50.0 C");
            else
                temp_trim_x10 = trim;
        }
    }
    {
        int16_t zero = hum_zero;

        if (atc_menu_int16(c, "Hum Zero (counts)", &zero)) {
            if (zero < -500 || zero > 500)
                atc_menu_reject(c, "Range -500..+500 counts");
            else
                hum_zero = zero;
        }
    }

    atc_menu_separator(c);
    if (atc_menu_action(c, "Sample Now"))
        sample(c);
    if (atc_menu_action(c, "Calibrate")) {
        adc_raw[CH_VBAT] = 371u;
        temp_trim_x10 = 0;
        hum_zero = 0;
        atc_menu_message(c, "Calibrated");
    }

    tx_footer(c);
}

static void page_pwm(atc_menu_ctx_t *c)
{
    uint16_t hz = pwm_freq;

    atc_menu_label(c, "PWM timer; changing Freq recomputes TA0CCR0.");

    /* PWM cannot start in SLEEP, so the row is offered only when it can act. */
    atc_menu_item_enable(c, sys_mode != MODE_SLEEP);
    atc_menu_bool(c, "Enable", &pwm_en);

    /* Rejecting rather than messaging leaves the editor open over what was
       typed, so a wrong figure is corrected instead of retyped. */
    if (atc_menu_uint16(c, "Freq (Hz)", &hz)) {
        if (hz < 100u || hz > 20000u) {
            atc_menu_reject(c, "Range 100..20000 Hz");
        } else {
            pwm_freq = hz;
            ta0ccr0 = (uint16_t)(1000000u / hz); /* 1 MHz timer clock */
        }
    }
    {   /* the local keeps a refused value out of duty_x10 entirely */
        int32_t duty = duty_x10;

        if (atc_menu_fix(c, "Duty (%)", &duty, 1u)) {
            if (duty < 0 || duty > 1000)
                atc_menu_reject(c, "Range 0.0..100.0 %");
            else
                duty_x10 = duty;
        }
    }
    atc_menu_uint16_ro(c, "TA0CCR0", ta0ccr0);

    tx_footer(c);
}

static void page_registers(atc_menu_ctx_t *c)
{
    atc_menu_label(c, "Raw peripheral view; the two HEX fields are writable.");
    atc_menu_hex16(c, "ADCCTL0", &adcctl0);
    atc_menu_hex16(c, "UCA0BRW", &uca0brw);
    atc_menu_uint32_ro(c, "Uptime", sys_ticks);
    atc_menu_hex32_ro(c, "Uptime (hex)", sys_ticks);

    tx_footer(c);
}

static void page_main(atc_menu_ctx_t *c)
{
    atc_menu_label(c, "System");
    atc_menu_label(c, "Demo menu for an MCU control panel.");
    atc_menu_choice(c, "Sys Mode", &sys_mode, SYS_MODES, 3u);
    atc_menu_text(c, "Device", dev_name, sizeof dev_name);
    {   /* the local keeps a refused value out of cal_gain entirely */
        int32_t gain = cal_gain;

        if (atc_menu_fix(c, "Cal Gain", &gain, 4u)) {
            if (gain < 5000 || gain > 20000)
                atc_menu_reject(c, "Gain 0.5000..2.0000");
            else
                cal_gain = gain;
        }
    }
    atc_menu_separator(c);

    if (atc_menu_submenu(c, "I/O")) {
        page_io(c);
        atc_menu_submenu_end(c);
    }
    if (atc_menu_submenu(c, "Sensors")) {
        page_sensors(c);
        atc_menu_submenu_end(c);
    }
    if (atc_menu_submenu(c, "PWM")) {
        page_pwm(c);
        atc_menu_submenu_end(c);
    }
    if (atc_menu_submenu(c, "Registers")) {
        page_registers(c);
        atc_menu_submenu_end(c);
    }
    atc_menu_separator(c);
    if (atc_menu_action(c, "Exit"))
        running = 0;

    tx_footer(c);
}

/* ---- host plumbing ------------------------------------------------------ */

static void draw(atc_menu_ctx_t *c)
{
    atc_menu_frame_begin(c);
    page_main(c);
    atc_menu_frame_end(c);
}

/* The menu goes out of the serial port, so the console it was started from is
   free to report what each update cost: a full page against a single changed
   row is visible there. */
static int counting_serial_sink(const char *buf, size_t len, void *user)
{
    if (!atc_menu_port_serial_sink(buf, len, user))
        return 0;
    tx_total += len;
    return 1;
}

/* The screen this build paints on. Nothing asks the terminal how big it is and
   nothing needs to: an application declares the screen it was written for, and
   a terminal that is not that size is resized. */
ATC_MENU_SCREEN(vt100, 80, 24);

int main(int argc, char **argv)
{
    static const atc_menu_info_t info = { "MSP430 EnvMon", "v0.2", "ahmettardic" };
    static atc_menu_ctx_t c;
    unsigned long         baud;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <device> <baud>   e.g. %s /dev/ttyUSB0 115200\n",
                argv[0], argv[0]);
        return 1;
    }

    baud = strtoul(argv[2], NULL, 10);
    if (baud == 0ul || !atc_menu_port_serial_open(argv[1], baud)) {
        fprintf(stderr, "cannot open %s @ %s\n", argv[1], argv[2]);
        return 1;
    }
    printf("menu on %s @ %s, %ux%u; pick Exit there to quit\n",
           argv[1], argv[2], vt100.cols, vt100.rows);
    fflush(stdout);

    if (atc_menu_init(&c, &info, &vt100, counting_serial_sink, NULL)
        != ATC_MENU_OK) {
        fprintf(stderr, "init failed\n");
        atc_menu_port_serial_close();
        return 1;
    }
    atc_menu_term_begin(&c);

    while (running) {
        size_t tx_before = tx_total;
        size_t cost;
        int    ch;

        tx_self = 0u;

        /* One frame a pass: an idle frame sends nothing but still builds and
           hashes every row. A key gets a second, since the level it opens only
           lands at frame_end. */
        ch = atc_menu_port_serial_getkey();
        if (ch >= 0) {
            atc_menu_key(&c, ch);
            sys_ticks++;
        }
        draw(&c);
        if (ch >= 0)
            draw(&c);

        cost = (tx_total - tx_before) - tx_self;
        if (cost != 0u) {
            tx_last = (uint32_t)cost;
            tx_hold = TX_HOLD_PASSES;
        } else if (tx_hold != 0u && --tx_hold == 0u) {
            tx_last = 0u;
        }

        if (tx_total != tx_before) {
            printf("TX %4u B  (total %u B)\n",
                   (unsigned)(tx_total - tx_before), (unsigned)tx_total);
            fflush(stdout);
        }

        if (ch < 0)
            usleep(20000u); /* the read returns at once when idle */
    }

    atc_menu_term_end(&c);
    atc_menu_port_serial_close();
    return 0;
}
