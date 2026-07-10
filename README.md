# atc_menu

UART üzerinden çalışan, allocation-free C11 debug menü framework'ü. Menüler
derleme zamanında **`ATC_MENU` makrolarıyla** tanımlanır; tüm ağaç `.rodata`'ya
oturur, RAM tüketmez.

## Özellikler

- 8 satır tipi: group / value / state / action / submenu / bar / choice / input
- `static const` tablo — sıfır malloc, transport-agnostic (port vtable)
- Tek-thread / non-reentrant: byte'ı ISR'da kuyruğa al, main loop'ta işle
- Flicker-free render: cursor-home + line-erase, full clear yok
- UTF-8 box drawing + ANSI renk (terminal Nerd Font ya da modern Unicode coverage gerektirir)
- Built-in tuşlar: `r` refresh · `b` back · `?` path · `:` komut modu
- Sub-menu nav stack, breadcrumb path, per-screen statik notlar

## Dizin yapısı

```
include/atc_menu/atc_menu.h         public API (tipler + lifecycle)
include/atc_menu/atc_menu_macros.h  compile-time macro layer
src/menu.c                          lifecycle + modal state (cmd/input/choice)
src/render.c                        row primitives + chrome render
src/nav.c                           sub-menü nav stack
src/{ansi,layout,symbols}.h         ANSI / kolon genişlikleri / UTF-8 glyph'ler
ports/mock/                         test için TX + cmd capture portu
tests/                              CTest unit testleri
examples/demo.c                     Serial demo
```

## Build & test

```
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Windows + MinGW: `cmake -S . -B build -G "MinGW Makefiles"`.

CMake seçenekleri:

| Option                     | Default | Etki                                |
|----------------------------|---------|-------------------------------------|
| `ATC_MENU_BUILD_TESTS`     | `ON`    | CTest hedefleri                     |
| `ATC_MENU_BUILD_EXAMPLES`  | `OFF`   | `examples/demo` (Windows + POSIX)   |

Örneği derleme ve çalıştırma: [Serial demo](#serial-demo).

### Layout tunables

`src/layout.h` içinde dört parametre CMake `-D` ile override edilebilir:

| Define      | Default | Etki                                       |
|-------------|--------:|--------------------------------------------|
| `LABEL_W`   |    `24` | Label sütunu genişliği                     |
| `VALUE_W`   |    `10` | Value sütunu — `INPUT_BUF` bununla büyür   |
| `UNIT_W`    |     `5` | Unit sütunu — INPUT edit alanı da büyür    |
| `NAV_DEPTH` |     `4` | Maksimum sub-menü iç içe derinliği         |

```
cmake -S . -B build -DLABEL_W=32 -DVALUE_W=20 -DUNIT_W=8 -DNAV_DEPTH=8
```

## Menü tanımı

```c
#include "atc_menu/atc_menu_macros.h"

static void rd_temp(char *b, size_t n, atc_status_t *st) {
    snprintf(b, n, "%d", read_temp_c()); *st = ATC_ST_OK;
}

ATC_MENU(home,
    ATC_GROUP ("Sensors"),
    ATC_VALUE ('t', "Temp", "C", rd_temp),
    ATC_STATE ('L', "LED",       rd_led, act_toggle_led),
    ATC_ACTION('1', "Self test", act_self_test),
    ATC_NOTE  ("Press hotkeys to interact."),
);

atc_menu_init(&home, &my_port, &info);
```

Genişletilmiş etiket karşılığı:
- `ATC_GROUP/VALUE/STATE/ACTION/SUBMENU/BAR/CHOICE/INPUT/NOTE`

Submenu'ler `&other_menu` ile bağlanır; alt tablo, ona referans veren satırdan
**önce** tanımlanmalıdır (C derleme sırası).

## Row tipleri

| Tip                | Görsel                              | Notlar                                      |
|--------------------|-------------------------------------|---------------------------------------------|
| `ATC_ROW_GROUP`    | `   Sensors`                        | Bölüm başlığı; key opsiyonel (toplu refresh)|
| `ATC_ROW_VALUE`    | `t  Temp   45.3 C   OK`             | Read-only skaler                            |
| `ATC_ROW_STATE`    | `L  LED    ON`                      | İki-durumlu; `action` toggle eder           |
| `ATC_ROW_ACTION`   | `1  Self test`                      | Hotkey'e bağlı komut                        |
| `ATC_ROW_SUBMENU`  | `i  ❯ MPU9250 IMU`                  | Alt tabloya drill-down                      |
| `ATC_ROW_BAR`      | `B  Battery ▕████▎  ▏ 78 % OK`      | 8 hücreli yatay seviye (1/8 sub-cell)       |
| `ATC_ROW_CHOICE`   | `m  Mode    ❮ NORMAL ❯      OK`     | N-state cycle; opsiyonel commit callback    |
| `ATC_ROW_INPUT`    | `d  PWM Duty       50 %     OK`     | Inline editor; INT/HEX/STR                  |
| `ATC_ROW_NOTE`     | dim tam-genişlik metin              | Ekran sonunda; ilki üstüne noktalı ayraç    |

**SUBMENU**: `submenu` pointer'ı zorunlu, derinlik `NAV_DEPTH` ile sınırlı.
**BAR**: `read` yüzdeyi string olarak yazar (`"78"`); renk `atc_status_t`'ten.
**CHOICE**: `choices` + `choice_idx` zorunlu. `choice_commit == NULL` →
immediate cycle; non-NULL → Enter ile commit, Esc revert. String'ler
`CHOICE_STR_MAX = VALUE_W - 4` (default 6 karakter).
**INPUT**: `input_commit` zorunlu. Tipler `INT`/`HEX`/`STR` — float yok
(MSP430/C2000'de float libc'sini linke çekmemek için bilinçli). Ondalık
gerekiyorsa fixed-point kullan: `1.5 A` yerine `15` (×0.1) gir, `commit`
içinde ölçekle. `input_min`/`input_max` INT/HEX için range; validation
fail editör'ü açık tutar, hata status'a basılır.

### Per-screen notlar

`ATC_NOTE` satırlarıyla dim renkli statik metin, kutunun altına noktalı
ayraçla ayrılıp basılır — sayfanın "ne yapar" özeti için. NOTE satırları
tablonun sonunda olmalıdır (aksi halde init uyarı basar):

```c
ATC_MENU(power,
    /* ... rows ... */
    ATC_NOTE("INA219 high-side current monitor, 0.1 ohm shunt."),
    ATC_NOTE("':set load <mA>' to push WARN/ERR thresholds."),
);
```

## Rezerve sistem tuşları

Çerçeve dört tuşu kendi kullanımı için ayırır — kullanıcı satırlarına
birini atamak init validation'da port'a uyarı yazar (`WARN: key 'X'
collides with built-in ...`):

| Tuş | İşlev                            |
|-----|----------------------------------|
| `r` | tam refresh                      |
| `b` | parent menüye dön                |
| `?` | tam path'i status'a yaz          |
| `:` | komut modu (port.cmd ayarlı ise) |

`src/internal.h` içinde `KEY_REFRESH`, `KEY_BACK`, `KEY_PATH`, `KEY_CMD`
define'larıyla tanımlı.

## Serial demo

```
cmake -S . -B build -DATC_MENU_BUILD_EXAMPLES=ON
cmake --build build --target demo
build/examples/demo /dev/ttyUSB0          # Linux/macOS
build/examples/demo /dev/ttyUSB0 9600     # opsiyonel baud (default 115200 8N1)
build/examples/demo.exe COM8              # Windows
```

Donanımsız test için `socat` ile sanal seri çift:

```
socat -d -d pty,raw,echo=0 pty,raw,echo=0   # /dev/pts/<a> ve /dev/pts/<b>
build/examples/demo /dev/pts/<a>
screen /dev/pts/<b> 115200                  # picocom/minicom de olur
```

Home hotkey'leri: `t` MCU temp · `v` battery voltage · `i` IMU · `p`
Power · `w` Visual Widgets · `L` LED toggle · `1` self-test · `:` komut
modu (`set temp 55.0`, `set vbat 3.10`, `set load 1800`).

Tüm menü ağacı `ATC_MENU` makrolarıyla `.rodata`'da tanımlı.

## Test mimarisi

- Çekirdek hiç dinamik bellek kullanmaz, transport'a sahip değildir.
- `ports/mock/` her TX byte'ını ve her `:` komutunu yakalar; testler
  `mock_buffer()`, `mock_last_cmd()` üzerinden assert eder.
- `tests/testing.h` minimal `EXPECT*` makrolarını ve `ATC_INIT_ITEMS`
  kısayolunu içerir; her test kendi binary'si, CTest tek tek koşturur.
