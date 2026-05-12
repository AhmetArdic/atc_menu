# atc_menu

UART üzerinden çalışan, allocation-free C11 debug menü framework'ü. Aynı
veri tipini iki şekilde yazabilirsin: derleme zamanında **`ATC_MENU` makrolarıyla**
(Flash'a oturur, RAM tüketmez) veya runtime'da **builder fonksiyonlarıyla**
(RAM'da, koşullu/dinamik içerik).

## Özellikler

- 9 satır tipi: group / value / state / action / submenu / bar / choice / input / **log_view**
- İki tanımlama yüzeyi, ortak `atc_menu_table_t`: static (macro, Flash) veya dynamic (builder, RAM)
- **Fullscreen event log**: ring buffer üstünde scrollback (`q`, `k`/`j`, `g`/`G`)
- `static const` ya da caller-buffer — sıfır malloc, transport-agnostic (port vtable)
- Flicker-free render: cursor-home + line-erase, full clear yok
- UTF-8 box drawing + ANSI renk (terminal Nerd Font ya da modern Unicode coverage gerektirir)
- Built-in tuşlar: `r` refresh · `b` back · `?` path · `:` komut modu
- Sub-menu nav stack, breadcrumb path, per-screen statik notlar

## Dizin yapısı

```
include/atc_menu/atc_menu.h         public API (tipler + builder API)
include/atc_menu/atc_menu_macros.h  compile-time macro layer
include/atc_menu/log_ring.h         line-ring buffer for log_view
src/menu.c                          lifecycle + modal state (cmd/input/choice/log_view)
src/render.c                        row primitives + chrome + log view render
src/nav.c                           sub-menü nav stack
src/builder.c                       runtime builder (atc_menu_begin/value/...)
src/log_ring.c                      ring buffer impl
src/{ansi,layout,symbols}.h         ANSI / kolon genişlikleri / UTF-8 glyph'ler
ports/mock/                         test için TX + cmd capture portu
tests/                              CTest unit testleri
examples/demo.c                     Serial demo (macros + builder + log)
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

## Menü tanımı: iki yol, aynı tip

Hem `ATC_MENU` makrosu hem `atc_menu_begin/...` builder'ı `atc_menu_table_t`
üretir. İkisi birbirine `&submenu` ile bağlanabilir.

**Karar rehberi:** menü değişmez → makro (Flash). Menü runtime'da koşullu
satır eklemeli/sıralamasını değiştirmeli → builder (RAM).

### Static — macro (Flash'a oturur)

```c
#include "atc_menu/atc_menu_macros.h"

static void rd_temp(char *b, size_t n, atc_status_t *st) {
    snprintf(b, n, "%.1f", read_temp_c()); *st = ATC_ST_OK;
}

ATC_MENU(home, ATC_NO_NOTES,
    ATC_GROUP ("Sensors"),
    ATC_VALUE ('t', "Temp", "C", rd_temp),
    ATC_STATE ('L', "LED",       rd_led, act_toggle_led),
    ATC_ACTION('1', "Self test", act_self_test),
);

atc_menu_init(&home, &my_port, &info);
```

Genişletilmiş etiket karşılığı:
- `ATC_GROUP/VALUE/STATE/ACTION/SUBMENU/BAR/CHOICE/INPUT/LOG_VIEW`
- `ATC_WITH_NOTES(arr)` veya `ATC_NO_NOTES` — `ATC_MENU`'nün notes argümanı

### Dynamic — builder (RAM, runtime build edilir)

```c
static atc_menu_item_t  home_items[16];
static atc_menu_table_t home;

static void build_home(void) {
    atc_menu_begin   (&home, home_items, 16);
    atc_menu_group   (&home, "Sensors");
    atc_menu_value   (&home, 't', "Temp", "C", rd_temp);
    if (g_has_imu) {                                       /* runtime conditional */
        atc_menu_submenu(&home, 'i', "IMU", &imu);         /* static submenu OK */
    }
    atc_menu_action  (&home, '1', "Self test", act_self_test);
    atc_menu_end     (&home);
}

build_home();
atc_menu_init(&home, &my_port, &info);
```

Aynı tablo'ya tekrar `atc_menu_begin(...)` çağırarak runtime'da menüyü
yeniden inşa edebilirsin — caller buffer paylaşılabilir.

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
| `ATC_ROW_INPUT`    | `d  PWM Duty       50 %     OK`     | Inline editor; INT/FLOAT/HEX/STR            |
| `ATC_ROW_LOG_VIEW` | `E  ❯ Event log`                    | Fullscreen log scrollback                   |

**SUBMENU**: `submenu` pointer'ı zorunlu, derinlik `NAV_DEPTH` ile sınırlı.
**BAR**: `read` yüzdeyi string olarak yazar (`"78"`); renk `atc_status_t`'ten.
**CHOICE**: `choices` + `choice_idx` zorunlu. `choice_commit == NULL` →
immediate cycle; non-NULL → Enter ile commit, Esc revert. String'ler
`CHOICE_STR_MAX = VALUE_W - 4` (default 6 karakter).
**INPUT**: `input_commit` zorunlu. `input_min`/`input_max` INT/HEX için
range; validation fail editör'ü açık tutar, hata status'a basılır.
**LOG_VIEW**: `log_ring` zorunlu. Satır basıldığında ekran fullscreen
moda geçer; `q`/`Esc` ile menüye döner, `k`/`j` scroll, `g`/`G` ile uç noktalar.

## Event log (fullscreen)

```c
#include "atc_menu/log_ring.h"

static char           log_buf[16][64];
static atc_log_ring_t event_log;

atc_menu_log_init   (&event_log, &log_buf[0][0], 16, 64);
atc_menu_log_printf (&event_log, "i2c err=%d", err);  /* her event'te çağır */

/* Menü tarafında — static: */
ATC_MENU(home, ATC_NO_NOTES,
    /* ... */
    ATC_LOG_VIEW('E', "Event log", &event_log),
);

/* veya dynamic: */
atc_menu_log_view(&home, 'E', "Event log", &event_log);
```

Ring caller-owned buffer kullanır, sıfır malloc. Yeni satırlar oldukça
eski satırların üstüne wrap eder; en yeni satır altta görünür.

### Per-screen notlar

`atc_menu_table_t.notes` ile dim renkli statik satırlar footer üstüne
basılır — sayfanın "ne yapar" özeti için:

```c
static const char *const power_notes[] = {
    "INA219 high-side current monitor, 0.1 ohm shunt.",
    "':set load <mA>' to push WARN/ERR thresholds.",
};

ATC_MENU(power, ATC_WITH_NOTES(power_notes),
    /* ... */
);
```

Dynamic taraf: `atc_menu_notes(&t, notes_arr, n)`.

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
define'larıyla tanımlı. Log view aktifken `q`/`Esc` view'i kapatır; `r`
yerine sadece view'in kendi tuşları (`k`/`j`/`g`/`G`) işler.

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
Power · `w` Visual Widgets · `L` LED toggle · `1` self-test · `E`
event log · `:` komut modu (`set temp 55.0`, `set vbat 3.10`,
`set load 1800`).

Demo iki yüzeyi de gösterir: 6 leaf menü `ATC_MENU` makrolarıyla
`.rodata`'da, `home` builder ile RAM'a inşa ediliyor. Eylemler
(`act_toggle_led`, `commit_pwr_mode`, `set temp ...` vb.)
`atc_menu_log_printf` çağrılarıyla event log'a düşer; `E` ile fullscreen
scrollback açılır.

## Test mimarisi

- Çekirdek hiç dinamik bellek kullanmaz, transport'a sahip değildir.
- `ports/mock/` her TX byte'ını ve her `:` komutunu yakalar; testler
  `mock_buffer()`, `mock_last_cmd()` üzerinden assert eder.
- `tests/testing.h` minimal `EXPECT*` makrolarını ve `ATC_INIT_ITEMS`
  kısayolunu içerir; her test kendi binary'si, CTest tek tek koşturur.
