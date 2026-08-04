# ATC Menu

Lightweight C99 library that draws colored, line-based menus over a serial
connection. Built for small microcontrollers, but works with any device that
can send and receive bytes.

```
MSP430 EnvMon                       v0.2
ATC
========================================
Main / I/O (1/5)
       P1 Outputs
   1   P1.0 LED                    [ ]
   2   P1.1 Fan                    [ ]
   3   P1.2 Heater                 [ ]
   4   P1.3 Relay                  [ ]
       P1 Inputs
   5   P1.4 Btn0                   [X]
   6   P1.5 Btn1                   [ ]
   7   P1.6 Limit                  [ ]
   8   P1.7 EStop                  [ ]
   -----------------------------------
   9   P1OUT                      0x00
----------------------------------------
 0 Back  r Refresh  ? Help  i Items/page  n/p Page

Select> _
```

## Features

- No dynamic memory allocation, no printf, no recursion
- The menu tree is fixed at compile time and lives in read-only memory
- Values are read straight from your hardware, so no row can go stale
- Only the parts of the screen that changed get redrawn
- Select items by typing a number - no need to press Enter
- Long menus page automatically; text wraps without breaking the zebra stripe

## Quick start

```c
#include "atc_menu/menu.h"

static int32_t pwm_freq = 1000;

static int32_t gpio_get(int32_t arg)                { return (P1IN >> arg) & 1; }
static int     gpio_set(int32_t arg, int32_t value) { if (value) P1OUT |=  (1u << arg);
                                                      else       P1OUT &= ~(1u << arg);
                                                      return 1; }

ATC_MENU_DEFINE_GETTER(freq, pwm_freq)              /* freq_get */
static int     freq_set(int32_t arg, int32_t value) { (void)arg;
                                                      if (value < 100 || value > 20000)
                                                          return 0;   /* refused */
                                                      pwm_freq = value; return 1; }

ATC_MENU_PAGE_BEGIN(main_page, "Main")
    ATC_MENU_CHECKBOX("LED (P1.0)",    gpio_get, gpio_set, 0)
    ATC_MENU_CHECKBOX("BTN0 (P1.4)",   gpio_get, NULL,     4)
    ATC_MENU_NUMBER  ("PWM Freq (Hz)", freq_get, freq_set, 0)
ATC_MENU_PAGE_END(main_page)

int main(void)
{
    static atc_menu_ctx_t ctx;
    static const atc_menu_info_t info = { "My App", "v1.0", "Me" };

    atc_menu_init(&ctx, &info, &main_page, uart_sink, NULL);
    for (;;) {
        /* feed received bytes with atc_menu_key(&ctx, byte) */
        atc_menu_update(&ctx);
    }
}
```

## Public API

| Function | Purpose |
|---|---|
| `atc_menu_init` | set up the context, queue the first draw |
| `atc_menu_key` | feed one received byte (navigation, editing, callbacks) |
| `atc_menu_update` | draw whatever the last keys made pending |
| `atc_menu_refresh` | force a full redraw |
| `atc_menu_message` | show a feedback message on the message line |

The menu tree itself is built with the macros below, not through API calls.

## Navigating a menu

Type a number to select that item; `0`/`b` back, `r` redraw, `?` help,
`i` rows per page, `n`/`p` next and previous page.

## Menu items

| Macro | Purpose |
|---|---|
| `ATC_MENU_LABEL(label)` | section heading, not selectable |
| `ATC_MENU_SEPARATOR()` | horizontal line |
| `ATC_MENU_TEXT(text)` | text, word-wrapped over several rows |
| `ATC_MENU_READOUT(label, rd, arg, fmt)` | read-only value |
| `ATC_MENU_CHECKBOX(label, rd, wr, arg)` | on/off toggle |
| `ATC_MENU_NUMBER(label, rd, wr, arg)` | integer input |
| `ATC_MENU_FIXED(label, rd, wr, arg, decimals)` | decimal input |
| `ATC_MENU_HEX(label, rd, wr, arg, bits)` | hex input |
| `ATC_MENU_CHOICE(label, rd, wr, arg, choices)` | list of options |
| `ATC_MENU_ACTION(label, fn, arg)` | runs a function |
| `ATC_MENU_PROMPT(label, cb, arg)` | free text input |
| `ATC_MENU_CUSTOM(label, show, edit, arg)` | anything else - you render and parse it |
| `ATC_MENU_SUBMENU(label, page)` | opens another page |

## Binding values

Every value tool reaches its value through one pair of accessors:

```c
int32_t rd(int32_t arg);                /* what the row shows          */
int     wr(int32_t arg, int32_t value); /* what the user chose; 0 = no */
```

`arg` is yours - a bit index, an ADC channel, a register id - so one pair can
drive a whole port, as `gpio_get`/`gpio_set` do above.

`wr` validates and stores in one step, so there is no separate change callback.
Returning 0 refuses the entry: nothing is written, the editor stays open, and
`atc_menu_message()` can say why. A NULL `wr` makes the row read-only. The editor
guards only what 32 bits can hold - the range is `wr`'s to enforce.

`int32_t` is the container, not the type behind it: a `uint16_t` register, a
`uint8_t` flag, a `bool`, an enum all fit, with the cast in the accessor.
`uint32_t` fits too - the conversion is modular, so the bits survive; the display
is told with `ATC_MENU_DEC | ATC_MENU_UNSIGNED`.

For a value with nothing to validate, the library writes the pair. These go at
file scope, outside the page:

```c
ATC_MENU_DEFINE_ACCESSORS(freq, pwm_freq)   /* freq_get + freq_set */
ATC_MENU_DEFINE_GETTER(ticks, sys_ticks)    /* ticks_get           */
```

### Values the tools cannot express

A float, a string, an IP address, a 64-bit counter, a derived status - the
application prints it and parses whatever was typed back:

```c
static unsigned gain_show(int32_t arg, char *out, unsigned cap);
static int      gain_edit(int32_t arg, const char *text);

ATC_MENU_CUSTOM("Cal Gain", gain_show, gain_edit, 0)
```

`show` writes at most `cap` bytes and returns how many (no terminator); NULL
displays `...`, which is what `ATC_MENU_PROMPT` is. `edit` returns 0 to refuse;
NULL makes the row read-only.

## Configuration

Compile-time options in `atc_menu/menu_config.h`, overridable with
`-DATC_MENU_CFG_X=...`:

| Option | Default | Purpose |
|---|---|---|
| `ATC_MENU_CFG_LABEL` | 38 | label width |
| `ATC_MENU_CFG_VAL` | 12 | value width |
| `ATC_MENU_CFG_INPUT_BUF` | 24 | edit-line input buffer |
| `ATC_MENU_CFG_MAX_DEPTH` | 8 | nested submenu depth |
| `ATC_MENU_CFG_EOL` | `"\r\n"` | end-of-line sequence |

`LABEL` below 4 or `VAL` below 12 is a compile error - a formatted number has to
fit. Row width and line buffer follow from the two width knobs:

```
"   1   Voltage (mV)            1234.56   "
 \_____/\________________/ \__________/\_/     row = LABEL + VAL + 10
    7         LABEL       1     VAL     2
```

The split is a minimum, not a fence - a value shorter than `VAL` hands its
spare columns to the label.

## Limits

| What | Limit |
|---|---|
| Values | `int32_t`; anything else goes through `ATC_MENU_CUSTOM` |
| Text | ASCII only - rows are aligned by byte count |
| Terminal | VT100/ANSI escapes required |
| Page | 254 items (more is a compile error), 248 rows shown at once |
| Nesting | `ATC_MENU_CFG_MAX_DEPTH` (8); a deeper `SUBMENU` is ignored |
| Edit line | `ATC_MENU_CFG_INPUT_BUF - 1` (23) chars; further keys are dropped |
| Overlong label or value | truncated to its column |
| RAM | one `atc_menu_ctx_t` (352 bytes on a 64-bit host, less on an MCU) |

`ATC_MENU_SUBMENU` needs its page defined earlier in the same file, so the tree
is acyclic by construction - no forward references, no loops.

`atc_menu_key()` and `atc_menu_update()` must run in the same context, i.e. the
main loop. Do not call either from an ISR, or item callbacks would run at
interrupt level. There is no locking, so one context serves one caller.

The sink is all-or-nothing: accept all `len` bytes and return 1, or take none and
return 0 - a rejected line is rebuilt and retried. `atc_menu_message()` does not
copy the string, so it must stay valid while shown (use a literal).

## Building and testing

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

## Project layout

```
include/atc_menu/   public headers
src/                core, renderer, input, formatting
port/host/          port used for testing on a PC
examples/basic/     demo application
test/               test suite
```
