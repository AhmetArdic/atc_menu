# atc_menu

UART üzerinden çalışan, tablo-driven, allocation-free C11 debug menü framework'ü.

## Özellikler

- Tek soyutlama: `row` — group / value / state / action / submenu / bar / choice / input
- `static const` tablo, sıfır dinamik bellek, transport-agnostic (port vtable)
- Flicker-free render: cursor-home + line-erase, full clear yok
- UTF-8 box drawing + ANSI renk (terminal Nerd Font ya da modern Unicode coverage gerektirir)
- Built-in tuşlar: `r` refresh · `b` back · `?` path · `:` komut modu
- Native sub-menu nav stack, breadcrumb path, per-screen statik notlar

## Dizin yapısı

```
include/atc_menu/atc_menu.h     public API (tek başlık)
src/menu.c                      public API + handle_key + cmd/INPUT/CHOICE editor'leri
src/render.c                    tüm rendering: row primitive'leri, chrome, per-type
src/nav.c                       sub-menü nav stack + breadcrumb
src/{ansi,layout,symbols}.h     ANSI / kolon genişlikleri / UTF-8 glyph'ler
src/{internal,nav,render}.h     internal header'lar
ports/mock/                     test için TX + cmd capture portu
tests/                          CTest unit testleri
examples/demo.c                 Serial demo (Windows + POSIX)
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

`VALUE_W` / `UNIT_W` büyütülünce INPUT'ta yazılabilen karakter sayısı da
otomatik artar (`INPUT_BUF = INPUT_EDIT_W - 2`). Diğer sabitler (`KEY_W`,
`STATUS_W`, `GAP_W` vb.) nadiren değiştiği için `layout.h`'i doğrudan düzenle.

## Hızlı başlangıç

```c
#include "atc_menu/atc_menu.h"

static void rd_temp(char *b, size_t n, atc_status_t *st) {
    snprintf(b, n, "%.1f", read_temp_c());
    *st = ATC_ST_OK;
}
static void rd_led(char *b, size_t n, atc_status_t *st) {
    (void)b; (void)n; *st = led_is_on() ? ATC_ST_ON : ATC_ST_OFF;
}
static void act_toggle_led(void) { led_toggle(); }
static void act_self_test(void)  { run_self_test(); }

static const atc_menu_item_t home_items[] = {
    { .type = ATC_ROW_GROUP,  .label = "Sensors" },
    { .type = ATC_ROW_VALUE,  .key = 't', .label = "Temp", .unit = "C",
                              .read = rd_temp },
    { .type = ATC_ROW_STATE,  .key = 'L', .label = "LED",
                              .read = rd_led, .action = act_toggle_led },
    { .type = ATC_ROW_ACTION, .key = '1', .label = "Self test",
                              .action = act_self_test },
};
static const atc_menu_table_t home_table = {
    .items = home_items, .count = sizeof home_items / sizeof home_items[0],
};

static const atc_menu_info_t info = {
    .project = "MyApp", .version = "0.1.0", .author = "me",
};

atc_menu_init(&home_table, &my_port, &info);
atc_menu_render();
for (;;) atc_menu_handle_key(uart_getc());
```

Yeni transport eklemek = `atc_menu_port_t` içine `write` (ve isteğe bağlı
`cmd`) implemente etmek. Yeni satır eklemek = tabloya entry + read/action
callback.

## Row tipleri

| Tip               | Görsel                              | Notlar                                      |
|-------------------|-------------------------------------|---------------------------------------------|
| `ATC_ROW_GROUP`   | `   Sensors`                        | Bölüm başlığı; key opsiyonel (toplu refresh)|
| `ATC_ROW_VALUE`   | `t  Temp   45.3 C   OK`             | Read-only skaler                            |
| `ATC_ROW_STATE`   | `L  LED    ON`                      | İki-durumlu; `action` toggle eder           |
| `ATC_ROW_ACTION`  | `1  Self test`                      | Hotkey'e bağlı komut                        |
| `ATC_ROW_SUBMENU` | `i  ❯ MPU9250 IMU`                  | Alt tabloya drill-down                      |
| `ATC_ROW_BAR`     | `B  Battery ▕████▎  ▏ 78 % OK`      | 8 hücreli yatay seviye (1/8 sub-cell)       |
| `ATC_ROW_CHOICE`  | `m  Mode    ❮ NORMAL ❯      OK`     | N-state cycle; opsiyonel commit callback    |
| `ATC_ROW_INPUT`   | `d  PWM Duty       50 %     OK`     | Inline editor; INT/FLOAT/HEX/STR            |

**SUBMENU**: `submenu` pointer'ı zorunlu, derinlik `NAV_DEPTH` ile sınırlı.
**BAR**: `read` yüzdeyi string olarak yazar (`"78"`); renk `atc_status_t`'ten.
**CHOICE**: `choices` + `choice_idx` zorunlu. `choice_commit == NULL` →
immediate cycle; non-NULL → Enter ile commit, Esc revert. String'ler
`CHOICE_STR_MAX = VALUE_W - 4` (default 6 karakter).
**INPUT**: `input_commit` zorunlu. `input_min`/`input_max` INT/HEX için
range; validation fail editör'ü açık tutar, hata status'a basılır.

### Per-screen notlar

`atc_menu_table_t.notes` ile dim renkli statik satırlar footer üstüne
basılır — sayfanın "ne yapar" özeti için:

```c
static const char *const power_notes[] = {
    "INA219 high-side current monitor, 0.1 ohm shunt.",
    "':set load <mA>' to push WARN/ERR thresholds.",
};
static const atc_menu_table_t power_table = {
    .items = power_menu, .count = ARR_LEN(power_menu),
    .notes = power_notes, .note_count = ARR_LEN(power_notes),
};
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

Home hotkey'leri: `t` MCU temp · `v` battery voltage · `e` BME280 env
refresh · `i` IMU · `p` Power · `w` Visual Widgets · `L` LED toggle ·
`1` self-test · `:` komut modu (`set temp 55.0`, `set vbat 3.10`,
`set load 1800`).

Demo dört sensör simülasyonu içerir: MCU temp + battery (skaler), INA219
(3 değer), BME280 (4 değer), MPU9250 9-DoF IMU (9 değer, sub-menülerle).
Sim değerleri ~50 ms'de rastgele yürüyüşle güncellenir. Sensör başına
struct + `READ_F` makrosu paterni için `examples/sensor_{helpers.h,sim.{c,h}}`.

## Test mimarisi

- Çekirdek hiç dinamik bellek kullanmaz, transport'a sahip değildir.
- `ports/mock/` her TX byte'ını ve her `:` komutunu yakalar; testler
  `mock_buffer()`, `mock_last_cmd()` üzerinden assert eder.
- `tests/testing.h` minimal `EXPECT*` makrolarını ve `ATC_INIT_ITEMS`
  kısayolunu içerir; her test kendi binary'si, CTest tek tek koşturur.
