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

**Does:** number-driven navigation, submenus, paging, editing of numbers
(decimal, hex, fixed point) and text, a message line, and incremental ANSI
painting. **Does not:** drive a display or a UART, store settings, wrap text, or
read a clock — there is no notion of time in it, not even a timeout.

## Quick start

```c
#include "atc_menu/menu.h"

static int uart_sink(const char *buf, size_t len, void *user)
{
    (void)user;
    return uart_write_all(buf, len) ? 1 : 0;   /* all of it, or none */
}

ATC_MENU_SCREEN(screen, 80, 24);               /* the two buffers, sized once */

static atc_menu_ctx_t        ctx;
static const atc_menu_info_t info = { "EnvMon", "v1.0", "me" };
static uint16_t              pwm_hz = 1000u;
static atc_menu_u8           p1out;

int main(void)
{
    atc_menu_init(&ctx, &info, &screen, uart_sink, NULL);
    atc_menu_term_begin(&ctx);

    for (;;) {
        int b = uart_getc_nonblocking();       /* < 0 when nothing arrived */
        if (b >= 0)
            atc_menu_key(&ctx, b);

        atc_menu_frame_begin(&ctx);
        if (atc_menu_submenu(&ctx, "I/O")) {
            unsigned i;
            uint16_t hz = pwm_hz;              /* a local keeps a refusal out */

            atc_menu_label(&ctx, "Outputs");
            for (i = 0u; i < 4u; ++i) {
                bool on = ((p1out >> i) & 1u) != 0u;
                if (atc_menu_bool(&ctx, pin_names[i], &on))
                    p1out ^= (atc_menu_u8)(1u << i);
            }
            atc_menu_hex8_ro(&ctx, "P1OUT", p1out);
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

Draw when a key arrives or a value moves. An idle frame sends nothing and lays
out nothing, but it is not free: it still signs every visible row.

## Widgets

The type is in the function name; read-only variants end in `_ro` and take a
**value**, not a pointer, so an expression works directly. No widget takes a
format argument.

| Editable | Read-only | Shown as |
|---|---|---|
| `atc_menu_uint8` / `uint16` / `uint32` | `…_ro` | `42` `1500` `4000000000` |
| `atc_menu_int16` / `int32` | `…_ro` | `-40` `-120000` |
| `atc_menu_hex8` / `hex16` / `hex32` | `…_ro` | `0x2A` `0x05DC` `0x0001D4C0` |
| `atc_menu_fix` | `atc_menu_fix_ro` | `23.450` (scaled integer, 0-4 decimals) |
| `atc_menu_bool` | `atc_menu_bool_ro` | `[X]` / `[ ]` |
| `atc_menu_text` | `atc_menu_text_ro` | `envmon-01` |

Plus `atc_menu_label`, `atc_menu_separator`, `atc_menu_choice`,
`atc_menu_action`, `atc_menu_submenu` / `_end`, `atc_menu_item_enable` and
`atc_menu_item_style` (bold/italic/underline/reverse OR one `ATC_MENU_FG_*`,
applied to the one item after it).

An editable widget returns `true` on the frame it wrote through the pointer, and
refuses anything outside the row's own range, so a narrowing cast can never
truncate behind your back. Further validation is yours, as above.

**A refused value keeps its editor.** `atc_menu_reject` puts the prompt back
over what was typed, so `100O` is a backspace away from `1000` rather than a
retype. The library's own range check behaves the same way.

Picking an editable row opens `Freq (Hz) [1000]> ` with the entry empty: the
bracket is the old value, `Esc` always means "leave it as it was", and
`atc_menu_text` keeps the keystrokes out of the caller's string until Enter.
`atc_menu_choice` previews rather than writes — printable keys step the
candidate, only Enter commits — so no single keystroke changes a live setting.
`atc_menu_bool` is the exception: it has no editor, the key that picks it writes
it, and `atc_menu_reject` has nothing to take back. Guard one with
`atc_menu_item_enable`, or make it a choice.

## Keys

| Key | Effect |
|---|---|
| `1`-`9` | item number |
| `0` | a digit while a prefix is pending, otherwise go up one level |
| `Enter` | finish an ambiguous prefix, or commit a value |
| `Backspace` | clear the prefix, delete a digit, or step a choice back |
| `.` | decimal point while editing a fixed-point value |
| `Esc` | cancel the prefix or the edit |
| `r` | full repaint |
| `n` / `p` | next / previous page |
| `i` | set how many items a page shows |

**A number acts as soon as it is unambiguous.** With 11 items, `2` fires at
once, `1` waits, and `1` `1` fires item 11 — no timer anywhere. **Numbers are
handed out per page** and restart at 1 on the next, so one keystroke is always
enough; labels and rules take a row and no number. The footer lists only the
keys that would do something.

## Geometry

**The two buffers are the geometry.** `buf` holds one row of `cols` columns
(and behind it the editor's scratch), `row_sig` holds one signature per row, so
its length is how tall the menu is. `ATC_MENU_SCREEN` writes each number once,
which is what keeps them from disagreeing — a `row_sig` shorter than `rows` is
the one mistake the library cannot catch.

`row_sig` is the diff. A row is signed by **what it is drawn from** — label,
value, number, style, stripe — rather than by the bytes it would come out as,
so an unchanged row is recognised without being laid out and an unchanged
number is never even formatted. The seven chrome rows go the same way under one
signature, except while an editor is open. That is why an idle frame sends
nothing and one changed row costs 128 bytes instead of a page.

`rows` is the terminal, not the menu: 6 to 8 rows go to the chrome (a banner
line with nothing in it is not painted and not charged) and the items get the
rest, paged. The closing rule, footer, message and prompt follow the last item
rather than sitting at the bottom, so a short level is a short menu and anything
the menu stops reaching is blanked.

## Contracts

- **The sink is all-or-nothing.** Every one of `len` bytes and return 1, or none
  and return 0. A refused row stays dirty and is rebuilt next frame, so a
  half-parsed escape sequence heals itself. Its TX buffer must hold one whole
  row (`ATC_MENU_ROW_BYTES(cols)`, 150 bytes at 80 columns), or block.
- **Declare the same items every frame.** Numbers are positions on the page. For
  a condition that changes at run time use `atc_menu_item_enable(c, false)`:
  the item keeps its number, is drawn dim, and answers "not available now".
- **Strings are never copied** and need only stay valid for the call. Two
  exceptions: a submenu label must live until `atc_menu_frame_end`, and
  `atc_menu_message` text while it is displayed.
- **Nothing may be called from an ISR.** Read the byte in your loop and hand it
  to `atc_menu_key`. No locking; one context, one caller.
- **The menu owns the terminal.** `atc_menu_term_begin` clears it, hides the
  cursor and paints by absolute position; `atc_menu_term_end` gives it back.

## Cost

Caller-owned except the context. Measured with 32-bit pointers, 80×24:

| RAM | Bytes | | Flash, `gcc -Os -m32`, `.text` | Bytes |
|---|---|---|---|---|
| `atc_menu_ctx_t` | 96 | | whole library | 13 027 |
| `buf` | 198 | | a 12-row menu, `--gc-sections` | 10 706 |
| `sig[24]` | 48 | | `.bss` + `.data` | 0 |
| **total** | **342** | | peak stack, deepest widget path | 512 |

Each widget is its own function, so a menu that never shows a hex value does not
carry the hex formatter.

## Portability

C99, freestanding: `<limits.h>`, `<stdbool.h>`, `<stddef.h>`, `<stdint.h>` and
`<string.h>` are the whole include list. No allocation, no recursion, no
floating point, no vendor header, no file-static mutable state — two contexts on
two UARTs never see each other.

Byte-shaped values are `atc_menu_u8` (`unsigned char`), never `uint8_t`: a C2000
has `CHAR_BIT == 16` and no `uint8_t` at all. A C2000 sink must mask each byte
with `& 0xFF` on the way out.

Everything the layout rests on is VT100 — `ESC[r;cH`, `ESC[K`, `ESC[2J`,
`ESC 7` / `ESC 8`. Colour, the stripe and the hidden cursor are not, and are the
part that degrades: a terminal without them shows a plainer menu of the same
shape.

## Build

```sh
cmake --preset default && cmake --build --preset default
ctest --preset default                                    # ASan + UBSan
cmake --preset msp430 && cmake --build --preset msp430    # TI cl430
cmake --preset c2000  && cmake --build --preset c2000     # TI cl2000
```

A cross build produces the library alone. The demo drives a serial terminal,
which is the only thing the library is for; with no hardware, `socat` gives you
both ends of a virtual line:

```sh
socat -d -d pty,raw,echo=0,link=/tmp/ttyA pty,raw,echo=0,link=/tmp/ttyB &
./build/default/examples/basic/menu_demo /tmp/ttyA 115200 &
screen /tmp/ttyB 115200
```

`docs/building-artifacts.md` covers packaging and the two ABI knobs;
`docs/atc-menu-flow.drawio` is the flow, file by file.

## Limits

| What | Limit |
|---|---|
| Values | `uint32_t` / `int32_t`; fixed point up to 4 decimals |
| Text | ASCII — columns are counted in bytes |
| Nesting | `ATC_MENU_MAX_DEPTH` (6); deeper is refused with `ERR_STATE` |
| Rows per level | 254; past it `atc_menu_frame_end` returns `ERR_STATE` |
| Page size | 1 to the item area: `rows` less 6, 7 or 8 rows of chrome |
| Resize | never queried; call `atc_menu_init` again with new buffers |
| Stale row | Fletcher-16 catches every single-byte change, but not a pair that cancels — `+k` and `−k` d bytes apart with `k·d ≡ 0 (mod 255)`; `r` repaints |

## Licence

MIT — see `LICENSE`.
