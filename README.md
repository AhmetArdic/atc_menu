# atc_menu

Immediate-mode terminal menu for microcontrollers. The menu tree is not a data
structure: the application declares it as code every frame, and the library
keeps only the navigation state. Items are picked by typing their number — no
cursor, no arrow keys, no escape sequences on the way in.

```
MSP430 EnvMon                                          v0.2
ahmettardic
===========================================================
Main / I/O (1/4)
       P1 Outputs
   1   P1.0 LED                                      [ ]
   2   P1.1 Fan                                      [X]
       P1 Inputs
   5   P1.4 Btn0                                     [X]
   ------------------------------------------------------
   9   P1OUT                                        0x0A
-----------------------------------------------------------
 0 Back  r Refresh  n/p Page  i Items
range is 100-20000
Select> 1_
```

## Scope

**Does:** number-driven navigation, submenus, paging at a page size the user
sets, editing of numbers (decimal, hex, fixed point) and text, a message line,
and incremental ANSI painting driven by a per-row signature.

**Does not:** drive a display or a UART, store settings, format logs, or wrap
text across rows. No dynamic allocation, no global mutable state, no recursion,
no floating point, and **no notion of time** — not even a timeout.

## Geometry

**The two buffers are the geometry.** `buf` holds one row of `cols` columns
(and behind it the editor's scratch), and `row_sig` holds one signature per row,
so its length is how tall the menu is. `atc_menu_screen_t` ties the four
together and `ATC_MENU_SCREEN` declares them, which is what keeps the numbers
from disagreeing:

```c
ATC_MENU_SCREEN(vt100, 80, 24);      /* buf, row_sig and the descriptor */
atc_menu_init(&ctx, &info, &vt100, uart_sink, NULL);
```

Each number is written once. That matters most for `rows`: a bare `uint16_t *`
carries no length, so a `row_sig` shorter than `rows` is the one mistake the
library cannot catch — it would overrun every frame. Fill the struct in by hand
if the buffers are not static; the fields are the same four.

`row_sig` is the diff: a 16-bit signature per row — Fletcher-16, with no
multiply in it, since a target without a hardware multiplier would pay for one
on every byte of every row. It is compared before anything goes out, which is
why an idle frame sends nothing and one changed row costs 128 bytes instead of a
page — 48 bytes of RAM for a 24-row menu, and the reason for every byte figure
below. Free on the wire is not free in the CPU: the frame still builds and
signs every visible row, so a menu that redraws in a tight loop is paying for
frames nobody asked for. Draw when a key arrives or a value moves.

The menu paints rows 1..`rows` from the top-left and never touches anything
below; a taller terminal simply leaves room, which is where an application puts
its own output.

**A row is the screen; an item is what you declared.** A level may declare far
more items than the screen has rows and the menu pages them. `rows` is physical:
absolute cursor addressing is what lets a changed row cost 128 bytes instead of
a whole page, and it cannot address a line the window does not have.

Chrome takes the rest, and the menu works out how much on its own: a banner line
with nothing in it is not painted and not charged, so a full `atc_menu_info_t`
costs 8 rows, a name only 7, and `info == NULL` 6. Nothing has to be reserved
for it — `rows` is the terminal, the items get what is left, and the 9-row floor
`atc_menu_init` enforces is the smallest window with one item row in it.

The closing rule, footer, message and prompt **follow the last item** rather
than sitting at the bottom of the window, so a level with four rows is four
rows tall and `i` genuinely shrinks the menu. `rows` is therefore the most the
menu may use, not what it always occupies; anything it stops reaching is
blanked, so a shorter page leaves no debris.

## Quick start

```c
#include "atc_menu/menu.h"

static int uart_sink(const char *buf, size_t len, void *user)
{
    (void)user;
    return uart_write_all(buf, len) ? 1 : 0;   /* all of it, or none */
}

/* The two buffers are the geometry; resizing the menu is these two numbers. */
ATC_MENU_SCREEN(screen, 80, 24);

static atc_menu_ctx_t   ctx;
static const atc_menu_info_t info = { "EnvMon", "v1.0", "me" };

static uint16_t pwm_hz = 1000u;
static atc_menu_u8 p1out;

int main(void)
{
    atc_menu_init(&ctx, &info, &screen, uart_sink, NULL);
    atc_menu_term_begin(&ctx);

    for (;;) {
        int b = uart_getc_nonblocking();      /* < 0 when nothing arrived */
        if (b >= 0)
            atc_menu_key(&ctx, b);

        atc_menu_frame_begin(&ctx);
        if (atc_menu_submenu(&ctx, "I/O")) {
            unsigned i;
            atc_menu_label(&ctx, "Outputs");
            for (i = 0u; i < 4u; ++i) {
                bool on = ((p1out >> i) & 1u) != 0u;
                if (atc_menu_bool(&ctx, pin_names[i], &on))
                    p1out ^= (atc_menu_u8)(1u << i);
            }
            atc_menu_hex8_ro(&ctx, "P1OUT", p1out);
            atc_menu_submenu_end(&ctx);
        }
        if (atc_menu_submenu(&ctx, "Config")) {
            uint16_t hz = pwm_hz;
            if (atc_menu_uint16(&ctx, "PWM (Hz)", &hz)) {
                if (hz < 100u || hz > 20000u)
                    atc_menu_reject(&ctx, "range is 100-20000");
                else
                    pwm_hz = hz;
            }
            atc_menu_submenu_end(&ctx);
        }
        atc_menu_frame_end(&ctx);
    }
}
```

## Widgets

The type is in the function name and read-only variants end in `_ro`. No widget
takes a format argument: the name carries width, signedness and base.

| Editable | Read-only | Shown as |
|---|---|---|
| `atc_menu_uint8` | `atc_menu_uint8_ro` | `42` |
| `atc_menu_uint16` | `atc_menu_uint16_ro` | `1500` |
| `atc_menu_uint32` | `atc_menu_uint32_ro` | `4000000000` |
| `atc_menu_int16` | `atc_menu_int16_ro` | `-40` |
| `atc_menu_int32` | `atc_menu_int32_ro` | `-120000` |
| `atc_menu_hex8` | `atc_menu_hex8_ro` | `0x2A` |
| `atc_menu_hex16` | `atc_menu_hex16_ro` | `0x05DC` |
| `atc_menu_hex32` | `atc_menu_hex32_ro` | `0x0001D4C0` |
| `atc_menu_fix` | `atc_menu_fix_ro` | `23.450` |
| `atc_menu_bool` | `atc_menu_bool_ro` | `[X]` / `[ ]` |
| `atc_menu_text` | `atc_menu_text_ro` | `envmon-01` |

Plus `atc_menu_label`, `atc_menu_separator`, `atc_menu_choice`,
`atc_menu_action`, `atc_menu_submenu` / `_end`, `atc_menu_item_enable` and
`atc_menu_item_style`.

`atc_menu_item_style` styles the one item after it and is spent there, so it can
neither leak into the row below nor survive the frame:

```c
atc_menu_item_style(c, ATC_MENU_BOLD | ATC_MENU_FG_RED);
atc_menu_text_ro(c, "State", "FAULT");
```

`ATC_MENU_BOLD`, `_ITALIC`, `_UNDERLINE` and `_REVERSE` OR together with one of
`ATC_MENU_FG_BLACK` … `_FG_WHITE`. The colour replaces the green the value
column would otherwise wear; the number column keeps its own, since it is what
the user types rather than what the application is saying. A dim item ignores
the style outright — "not available now" outranks decoration.

An editable widget returns `true` on the frame it wrote through the pointer, and
refuses anything outside the row's range — `atc_menu_uint8` takes 0..255 — so a
narrowing cast can never truncate a value behind your back. Further validation
is yours, and a local keeps a rejected value out of your state:

```c
uint16_t hz = pwm_hz;
if (atc_menu_uint16(c, "PWM (Hz)", &hz)) {
    if (hz < 100u) atc_menu_reject(c, "too low");
    else           pwm_hz = hz;
}
```

**A refused value keeps its editor.** `atc_menu_reject` puts the prompt back
over what was typed, so `100O` is a backspace away from `1000` rather than a
retype, and the message says why. The library's own range check does the same
thing, so both kinds of refusal behave alike. Without it a refused value simply
closes the editor and the typing is gone.

`atc_menu_bool` is outside this: it has no editor, so the keystroke that picks
the row is also the one that writes and `atc_menu_reject` has nothing to take
back — it does nothing there, quietly. A bool that needs a guard belongs behind
`atc_menu_item_enable`, or should be an `atc_menu_choice`, which shows a
candidate first for exactly this reason.

Picking an editable widget opens a prompt that names the row and shows what it
holds — `Freq (Hz) [1000]> ` — with the **entry itself empty**: the bracket is
the old value, the caret is the new one. Nothing has to be deleted first, `Esc`
always means "leave it as it was", and the row stays readable behind the
prompt. `atc_menu_text` keeps the keystrokes out of the caller's string until
Enter, so an abandoned edit never touches it.

`atc_menu_choice` previews instead of writing: selecting it opens
`Sys Mode [SLEEP]> `, every printable key steps to the next option and
Backspace to the previous, and only Enter commits. The row keeps showing the
committed choice throughout, so current and candidate are both on screen and no
single keystroke can change a live setting.

Read-only widgets take a **value**, not a pointer, so an expression works
directly: `atc_menu_bool_ro(c, "Btn0", (P1IN >> 4) & 1u)`.

## Keys

| Key | Effect |
|---|---|
| `1`-`9` | item number (see below) |
| `0` | a digit while a prefix is pending, otherwise go up one level |
| `Enter` | finish an ambiguous prefix, or commit a value |
| `Backspace` | clear the prefix, delete the last digit, or step a choice back |
| `.` | decimal point while editing a fixed-point value |
| `Esc` | cancel the prefix or the edit |
| `r` | full repaint |
| `n` / `p` | next / previous page — either drops a pending prefix |
| `i` | set how many items a page shows |

**The legend lists only the keys that would do something.** At the root there is
nowhere to go back to and `0 Back` is not offered; on a level that fits one page
neither is `n/p Page`. On a narrow screen a hint that no longer fits is dropped
whole, most-essential last, rather than being cut in half.

**A number acts as soon as it is unambiguous.** With N numbers on the page, one
that cannot be extended (`acc * 10 > N`) fires at once; `Enter` finishes one
that could still grow. With 11 items, `2` fires immediately, `1` waits, and
`1` `1` fires item 11. No timer is involved, which is why the library reads no
clock at all.

**Numbers are handed out per page** and start again at 1 on the next one, so a
page never holds more numbers than it has rows — which is what keeps one
keystroke enough. `atc_menu_label` and `atc_menu_separator` take a row and no
number; every other row is numbered, read-only ones included, and picking one of
those says so on the message line rather than swallowing the key.

`i` opens `Items [12] (1-16)> _` and the number typed there becomes the page
size — the same thing `atc_menu_set_items_per_page` does. Everything the frame
declared counts, so a label spends one the same way a widget does. An empty `Enter` or `Esc` leaves it alone;
anything else is clamped rather than refused.

## Contracts

**The sink is all-or-nothing.** It accepts every one of `len` bytes and returns
1, or takes none and returns 0. A refused line stays dirty and is rebuilt from
its own `ESC` next frame, so a half-parsed escape sequence heals itself. The
port's TX buffer must hold one whole row (`ATC_MENU_ROW_BYTES(cols)`, 150
bytes at 80 columns), or the write must block.

**Declare the same items every frame.** Numbers are positions on the page, so
dropping a declaration renumbers everything below it. For a condition that
changes at run time use `atc_menu_item_enable(c, false)`: the item keeps its
number, is drawn dim, and picking it answers "not available now".

**Strings are never copied.** Labels, values and choices need only stay valid
for the duration of the call, so a stack buffer is fine. Two exceptions: a
submenu label must live until `atc_menu_frame_end` (it is the title), and
`atc_menu_message` text must live while it is displayed.

**Nothing may be called from an ISR.** Read the byte in your loop and hand it to
`atc_menu_key`. There is no locking; one context serves one caller.

**The menu owns the terminal.** `atc_menu_term_begin` clears the screen, hides
the cursor and paints by absolute position; `atc_menu_term_end` gives it back.
Everything the *layout* rests on is VT100 — `ESC[r;cH`, `ESC[K`, `ESC[2J`,
`ESC 7` / `ESC 8` — so the menu never lands in the wrong column whatever it is
attached to. Colour and attributes are not, and they are the part that degrades:
a terminal without them shows a plainer menu of the same shape. It renders on
`screen`, `minicom`, `picocom`, PuTTY and a bare UART terminal. A terminal attached after
the menu started sees a blank screen until something changes — `r` repaints,
which is what it is for.

## Portability

C99, freestanding: `<limits.h>`, `<stdbool.h>`, `<stddef.h>`, `<stdint.h>` and
`<string.h>` are the whole include list. No allocation, no recursion, no
floating point, no vendor header, and no file-static mutable state — two
contexts on two UARTs never see each other.

Byte-shaped values are `atc_menu_u8` (`unsigned char`), never `uint8_t`: a C2000
has `CHAR_BIT == 16` and no `uint8_t` at all. On every 8-bit-byte target the two
are the same type, so passing a `uint8_t` variable keeps working. A C2000 sink
must mask each byte with `& 0xFF` on the way out — only the low half of a `char`
belongs on the wire.

## Memory

Everything is caller-owned except the context. Measured with 32-bit pointers on
an 80×24 terminal:

| Item | Bytes |
|---|---|
| `atc_menu_ctx_t` | 96 |
| `buf[ATC_MENU_BUF_BYTES(80)]` | 198 |
| `sig[24]` | 48 |
| **Total RAM** | **342** |
| `.bss` and `.data` in the library | **0** |
| Peak stack, deepest widget path | 528 |

The stack figure is `-fstack-usage` summed along the deepest chain there is —
`atc_menu_fix` 64, `num_edit_i` 80, `num_item` 128, `item_slot` 96, `row_item`
160 — and the sink's own frame sits on top of it.

The buffer is one row (`ATC_MENU_ROW_BYTES(cols)`, 150 at 80 columns)
followed by the scratch an open editor needs — the prompt title and the
keystrokes typed under it, neither of which can be rebuilt from the
application's strings once the widget call has returned. Nothing configures it:
the tail is whatever the buffer has spare, so a bigger array is a longer edit
line, and the context holds none of it.

Flash, `gcc -Os -m32` because that is what can be measured on a host — a Thumb
or MSP430 build is smaller. `.text` only, so no unwind tables:

| | Bytes |
|---|---|
| Whole library, nothing discarded | 11 655 |
| A 12-row menu of six widget kinds, linked with `--gc-sections` | 9 309 |

The gap is the point: each widget is its own function, so a menu that never
shows a hex value does not carry the hex formatter — and the menu measured here
already uses six kinds, so a plainer one drops further.

## Build

```sh
cmake --preset default && cmake --build --preset default
ctest --preset default                            # ASan + UBSan
```

Presets: `default`, `debug`, `asan`, `m32`, and the two cross builds.
`docs/building-artifacts.md` covers packaging and the two ABI knobs.

```sh
cmake --preset msp430 && cmake --build --preset msp430    # TI cl430
cmake --preset c2000  && cmake --build --preset c2000     # TI cl2000
(cd build/default && cpack)                               # atc_menu-<ver>.tar.gz
```

A cross build produces the library alone: the demo and the tests are host
programs, so they switch off whenever `CMAKE_CROSSCOMPILING` is set or the
project is a subdirectory of another.

The demo drives a serial terminal, which is the only thing the library is for —
there is no draw-on-this-console mode, because that is not the arrangement the
menu has to work in:

```sh
./build/default/examples/basic/menu_demo /dev/ttyUSB0 115200
```

Its geometry is its two buffers, declared for a VT100 at 80×24. Nothing asks
the terminal how big it is and nothing has to: an application declares the
screen it was built for, and a terminal that is not that size gets resized.

The console the demo was started from stays free, so it reports what each
update cost and a full page against a single changed row is visible there. An
idle frame sends nothing at all. With no hardware, `socat` gives you both ends
of a virtual line:

```sh
socat -d -d pty,raw,echo=0,link=/tmp/ttyA pty,raw,echo=0,link=/tmp/ttyB &
./build/default/examples/basic/menu_demo /tmp/ttyA 115200 &
screen /tmp/ttyB 115200
```

## Layout

```
include/atc_menu/menu.h   the entire public surface
src/menu_internal.h       what the six pieces below say to each other
src/menu_buf.c            bytes and numbers into a line buffer
src/menu_draw.c           which row a thing lands on, and what it looks like
src/menu_edit.c           the editor's state and its slice of the buffer
src/menu_input.c          a received byte, turned into navigation or a keystroke
src/menu_widget.c         what the application declares, frame by frame
src/menu_core.c           init, terminal, frame, teardown
port/host/                serial port for the host demo
examples/basic/           the control-panel demo
tests/                    fake sink and the host tests
cmake/toolchains/         TI cl430 and cl2000 cross builds
docs/                     packaging a prebuilt artifact
```

## Limits

| What | Limit |
|---|---|
| Values | `uint32_t` / `int32_t`; fixed point up to 4 decimals |
| Text | ASCII — columns are counted in bytes |
| Terminal, layout | VT100: `ESC[r;cH`, `ESC[2J`, `ESC[K`, `ESC[?6l`, `ESC 7` / `ESC 8` |
| Terminal, looks | Past VT100, all of it optional: `ESC[48;5;236m` stripe (xterm-256color), `ESC[90m` footer hint (aixterm), SGR `30`-`37` / `39` / `22` (ECMA-48), `ESC[?25l` cursor hiding (VT220) |
| Style bits | SGR `1`, `4`, `7` are VT100; `ATC_MENU_ITALIC` is SGR `3`, which the Linux console drops |
| Dropped attribute | Costs appearance only, with one exception: a disabled item is drawn with SGR `2`, so a terminal without faint shows it like any other row |
| Nesting | `ATC_MENU_MAX_DEPTH` (6); deeper is refused with `ERR_STATE` |
| Rows per level | 254; past it `atc_menu_frame_end` returns `ERR_STATE` |
| Page size | 1 to the item area: `rows` less 6, 7 or 8 rows of chrome |
| Resize | never queried; call `atc_menu_init` again with new buffers |
| Stale row | Fletcher-16 catches every single-byte change outright, but not a pair that cancels — `+k` and `−k` d bytes apart with `k·d ≡ 0 (mod 255)`; `r` repaints |

## Licence

MIT — see `LICENSE`.
