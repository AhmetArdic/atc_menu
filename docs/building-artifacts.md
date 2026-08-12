# Building a distributable artifact

The deliverable is `lib/libatc_menu.a` plus the headers that describe it. This
page is about shipping that pair to someone who will not build it themselves.

## Build and package

```sh
cmake --preset default
cmake --build --preset default
(cd build/default && cpack)
```

`cpack` writes `atc_menu-<version>.tar.gz` holding:

```
include/atc_menu/menu.h          the public API
include/atc_menu/version.h       generated: what commit this is
include/atc_menu/build_config.h  generated: what it was built with
lib/libatc_menu.a
```

## Version

`MAJOR.MINOR.PATCH` come from the latest `vMAJOR.MINOR.PATCH` git tag.
`ATC_MENU_GIT_DESCRIBE` adds commits-since-tag and dirty state, so a shipped
archive traces back to a commit even when it was cut off a tag:

```sh
git tag -a v0.2.0 -m "..."      # then reconfigure; the version follows
```

No tag and no git falls back to `ATC_MENU_VERSION_FALLBACK` in `CMakeLists.txt`,
and the describe string says `-nogit` so nobody mistakes it for a release.

## The one knob, and why it is the only one

```sh
cmake --preset default -DATC_MENU_MAX_DEPTH=8
```

| Knob | Default | Sizes |
|---|---|---|
| `ATC_MENU_MAX_DEPTH` | 6 | `crumb`, `path`, `top`, `item` in `atc_menu_ctx_t` |

It sits inside `atc_menu_ctx_t`, which is why it travels with the artifact: an
application that overrides it and links an archive built with another value has
two different opinions about the size of that struct, and the caller's `static
atc_menu_ctx_t` is then the wrong size. Nothing else can do that. The row and
column counts are the caller's buffer sizes, decided at run time, so a
40-column build and a 132-column build are the same binary — there is no width
variant to package. The editor's scratch is the tail of that same buffer,
so how much text a row can take is a run-time size too, and no build differs
from another over it.

The generated header makes the disagreement a compile error:

```c
#include "atc_menu/menu.h"
#include "atc_menu/build_config.h"

ATC_MENU_ASSERT_BUILD_MATCH();   /* file scope, once per file */
```

## Cross builds

```sh
cmake --preset msp430    # TI cl430
cmake --preset c2000     # TI cl2000
```

Neither toolchain file hardcodes an install path. Point at the codegen tools
with `-DTI_CGT_ROOT=<path>`, the environment variable of the same name, or by
putting `cl430`/`cl2000` on `PATH`.

`TI_CGT_ABI_FLAGS` is the one setting with a consequence beyond the build: a
static library carries its codegen choices as build attributes, and the linker
rejects an archive whose attributes disagree with the application's. The
defaults mirror the CCS example projects; if your application builds with
something else, override the whole string:

```sh
cmake --preset msp430 -DTI_CGT_ABI_FLAGS="-vmspx --abi=eabi --data_model=large"
```

A cross build produces the library only. The demo and the test suite are host
programs — stdio, a PC serial port, the filesystem — so they are switched off
whenever `CMAKE_CROSSCOMPILING` is set.

### C2000

`CHAR_BIT` is 16 there and `uint8_t` does not exist, which is why the library
spells byte-shaped values `atc_menu_u8` (`unsigned char`). Two consequences for
a port:

- the sink must mask each byte with `& 0xFF` on the way out — only the low half
  of a `char` belongs on the wire;
- `sizeof` counts 16-bit words, so `ATC_MENU_BUF_BYTES(cols)` yields a
  buffer of that many `char`s, which is the right amount of storage but twice
  the bytes.

## Consuming it

Prebuilt:

```cmake
add_library(atc_menu STATIC IMPORTED)
set_target_properties(atc_menu PROPERTIES
    IMPORTED_LOCATION "${ATC_MENU_DIR}/lib/libatc_menu.a")
target_include_directories(atc_menu INTERFACE "${ATC_MENU_DIR}/include")
```

From source, as a subdirectory — the demo and the tests switch themselves off,
so nothing extra is built:

```cmake
add_subdirectory(third_party/atc_menu)
target_link_libraries(app PRIVATE atc_menu)
```
