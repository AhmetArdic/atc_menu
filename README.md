# atc_menu

UART üzerinden çalışan, tablo-driven, allocation-free C11 debug menü framework'ü.

## Özellikler

- Tek soyutlama: `row` — group / value / state / action / submenu / **bar / choice / input**
- `static const` tablo, sıfır dinamik bellek, transport-agnostic (port vtable)
- Flicker-free render: cursor-home + line-erase, full clear yok
- UTF-8 box drawing + ANSI renk (terminal/font Nerd Font ya da modern Unicode coverage gerektirir)
- Built-in tuşlar: `r` refresh · `b` back · `?` path · `:` komut modu
- Native sub-menu (`ATC_ROW_SUBMENU`): çerçeve nav stack'ini kendi yönetir
- Project metadata header'ı: project / version / author / build (`atc_menu_init`'e geçilir)
- Per-screen statik notlar: tablo `notes` field'ı (footer'a dim renderle çıkar)

## Dizin yapısı

```
include/atc_menu/atc_menu.h     public API (tek başlık)
src/core/                       çekirdek + internal API
src/render/                     layout / box / ANSI / glyph'ler
src/input/                      nav + komut modu
src/widgets/                    her row tipi için bir .c
ports/mock/                     test için TX + cmd capture portu
tests/                          CTest unit testleri
examples/demo.c                 Windows serial demo
```

## Build & test

Windows + MinGW (gcc + mingw32-make):

```
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

Linux / macOS'ta default generator yeterli — `-G` flag'ini atla.

CMake seçenekleri:

| Option                     | Default | Etki                                                         |
|----------------------------|---------|--------------------------------------------------------------|
| `ATC_MENU_BUILD_TESTS`     | `ON`    | `tests/` altındaki CTest hedefleri                           |
| `ATC_MENU_BUILD_EXAMPLES`  | `OFF`   | `examples/demo` (Windows-only)                               |
| `ATC_MENU_WIDGET_BAR`      | `ON`    | `ATC_ROW_BAR` widget'ını derle                               |
| `ATC_MENU_WIDGET_CHOICE`   | `ON`    | `ATC_ROW_CHOICE` widget'ını derle                            |
| `ATC_MENU_WIDGET_INPUT`    | `ON`    | `ATC_ROW_INPUT` widget'ını derle                             |
| `ATC_MENU_INPUT_FLOAT`     | `ON`    | INPUT'ta `ATC_INPUT_FLOAT` parser'ı (`strtod`'u link'e çeker)|

İhtiyaç duyulmayan widget'ları kapatmak kütüphane boyutunu küçültür —
hepsi `OFF` iken minimal build referans build'in ~%29'unu yer (gömülü
hedef için anlamlı). Kapalı widget'a karşılık gelen `ATC_ROW_*` satırı
bir tabloda kullanılırsa init validation `widget_ops()` `NULL` döndürür
ve satır boş render olur.

### Layout / buffer override'ları

Tüm sütun genişliği ve buffer boyutu sabitleri `src/render/layout.h`
içinde `#ifndef` guard'ı ile tanımlı; CMake `target_compile_definitions`
(ya da `-DMENU_X=Y`) ile override edilebilir.

| Define              | Default | Anlam                                       |
|---------------------|--------:|---------------------------------------------|
| `MENU_KEY_COL`      |     `1` | Hotkey sütunu genişliği                     |
| `MENU_LABEL_COL`    |    `24` | Label sütunu genişliği                      |
| `MENU_VALUE_COL`    |    `10` | Value sütunu genişliği (BAR/CHOICE buna sığar) |
| `MENU_UNIT_COL`     |     `5` | Unit sütunu genişliği                       |
| `MENU_STATUS_COL`   |     `1` | Status sembol sütunu                        |
| `MENU_FIELD_GAP_W`  |     `3` | Sütunlar arası boşluk                       |
| `MENU_BUF_SIZE`     |    `16` | `read()` çağrısına geçen yığın tampon       |
| `MENU_CMD_BUF`      |    `64` | Komut modu satır tamponu                    |
| `MENU_INPUT_BUF`    |    `16` | INPUT widget editör tamponu                 |
| `MENU_ROW_BUF`      |   `256` | `row_t` derleme tamponu (ANSI dahil)        |
| `ATC_MENU_STACK_DEPTH` |  `4` | Sub-menu nav stack maksimum derinliği       |

`MENU_INNER_W`, `MENU_GROUP_LABEL_W`, `MENU_SUBMENU_LABEL_W`,
`MENU_INPUT_EDIT_W`, `MENU_NOTE_W`, `MENU_VALUE_BUF`,
`MENU_INPUT_EDIT_BUF` yukarıdakilerden türetilir; ayrıca tanımlamaya
gerek yok.

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
`cmd`) implemente etmek. Yeni satır eklemek = tabloya bir entry + bir
read/action callback.

## Serial demo (Windows)

```
cmake -S . -B build -G "MinGW Makefiles" -DATC_MENU_BUILD_EXAMPLES=ON
cmake --build build --target demo
build/examples/demo.exe COM8        # veya başka bir COM portu
build/examples/demo.exe COM3 9600   # opsiyonel baud override (default 115200 8N1)
```

PuTTY / TeraTerm ile aynı portu aç. Demo home menüde Quick view + BME280
Env satırlarını inline gösterir; IMU / Power / Visual Widgets sayfaları
SUBMENU drill-down ile açılır. IMU sayfası kendi içinde Accel/Gyro/Mag
sub-page'lerine ayrılır (2 derinlik). Sub-menüde `b` parent'a döner, `?`
tam path'i (`Home > MPU9250 IMU > Accelerometer`) status satırına basar.

Home hotkey'leri:

| Tuş | Etki                           |
|-----|--------------------------------|
| `t` | MCU temp göster                |
| `v` | Battery voltage göster         |
| `e` | BME280 Env grubunu yenile      |
| `i` | IMU sayfasına geç              |
| `p` | Power sayfasına geç            |
| `w` | Visual Widgets sayfasına geç   |
| `L` | LED toggle                     |
| `1` | Self-test çalıştır             |
| `:` | Komut modu (`set temp 55.0`, `set vbat 3.10`, `set load 1800`) |

Demo dört sensör simülasyonu içerir: tek-değerli (MCU temp + battery),
3-değerli INA219 (V/I/P), 4-değerli BME280 (T/H/P/Alt), 9-değerli MPU9250
9-DoF IMU. Sim değerleri her ~50 ms'de rastgele yürüyüşle güncellenir;
`r` ile yenileyince yeni değerler görünür. `set load <mA>` INA219 akımını
OK → WARN → ERR eşiklerinin üstüne itmek için.

## Sensör başına ergonomi

INA219 (3 satır) ve özellikle 9-DoF IMU (9 satır) callback'leri hızla
boilerplate'e döner. Demo bunu üç paternle çözüyor (`examples/sensor_helpers.h`,
`examples/sensor_sim.{h,c}`):

1. **Sensör başına struct + tick.** Her sensör (`ina219_t`, `bme280_t`,
   `mpu9250_t`) küçük bir struct'ta yaşar. `sensor_sim_tick()` (gerçekte
   bir I²C sürücüsü) struct'ı günceller; menü sadece field'ı okur.
2. **`READ_F` makrosu.** Bir field'i okuyup formatlayan callback'i tek
   satırda üretir:
   ```c
   READ_F(ina_v, g_ina.bus_v, "%.2f",
          st_range(g_ina.bus_v, 3.0f, 4.2f))
   ```
   `st_range/max/min` yardımcıları sık kullanılan threshold mantığını
   sarmalar.
3. **Grup başına sensör.** Tabloda her chip için `ATC_ROW_GROUP`
   (`"INA219 Power"`, `"BME280 Env"`, `"MPU9250 IMU"`) — kalabalık menüde
   göz hızla doğru bloğa gider.

Bu yardımcılar `examples/` altında, kütüphane API'sinin parçası değil.

## Sub-menü ile satır sayısını yönetmek

Sensör sayısı arttıkça tüm satırları tek ekrana sığdırmak okunaksızlaşır.
Çözüm: `ATC_ROW_SUBMENU` row + alt tablo. Her satır kendi `submenu`
pointer'ını taşır; çerçeve nav stack'ini yönetir, `b` parent'a döner, `?`
tam path'i status satırında basar.

```c
static const atc_menu_item_t imu_menu[] = {
    { .type = ATC_ROW_GROUP, .label = "MPU9250 IMU" },
    { .type = ATC_ROW_VALUE, .label = "Accel X", .unit = "g", .read = rd_ax },
    /* ... */
};
static const atc_menu_table_t imu_table = {
    .items = imu_menu, .count = sizeof imu_menu / sizeof imu_menu[0],
};

static const atc_menu_item_t home_menu[] = {
    { .type = ATC_ROW_VALUE,   .key = 't', .label = "MCU Temp", .unit = "C",
      .read = rd_temp },
    { .type = ATC_ROW_SUBMENU, .key = 'i', .label = "MPU9250 IMU",
      .submenu = &imu_table },
};
```

SUBMENU satırı bir prompt marker (`SYM_SUBMENU`) ile çıkar; footer'a
`[b] back  [?] path` eklenir. Submenu içinde başka SUBMENU olabilir —
demo `home → MPU9250 IMU → {Accel|Gyro|Mag}` 2 derinlik gezinti gösterir.
Stack derinliği varsayılan 4; `-DATC_MENU_STACK_DEPTH=8` ile artırılabilir.

### Per-screen notlar

`atc_menu_table_t.notes` ile dim renkli statik metin satırlarını footer
üstünde basabilirsin — sayfanın "ne yapar" özeti için ideal:

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

### Rezerve sistem tuşları

Çerçeve dört tuşu kendi kullanımı için ayırır — kullanıcı satırlarına
birini atamak init validation'da port'a uyarı yazar (`WARN: key 'X'
collides with built-in ...`):

| Tuş | İşlev                            |
|-----|----------------------------------|
| `r` | tam refresh                      |
| `b` | parent menüye dön                |
| `?` | tam path'i status'a yaz          |
| `:` | komut modu (port.cmd ayarlı ise) |

Tuşlar `src/core/internal.h` içinde `ATC_KEY_REFRESH`, `ATC_KEY_BACK`,
`ATC_KEY_PATH`, `ATC_KEY_CMD` define'larıyla tanımlı; tek noktadan
değiştirilebilir.

## Görsel widget'lar

Üç ek satır tipi gömülü-debug menüsünün eksik kalan kategorilerini doldurur.
Hepsi mevcut sütun yerleşimini bozmaz; kalıcılık (flash/NVS) kullanıcının
sorumluluğunda.

### `ATC_ROW_BAR` — yatay seviye çubuğu

Value sütununa 8 hücrelik yatay bir çubuk yerleşir; `read` sayısal
yüzdeyi (`"78"`) yazar, çerçeve dolu/boş hücreleri (sub-cell precision
ile, 1/8 adımlarla) çizer ve yüzdeyi unit sütununda basar. Renk
`read`'in döndürdüğü `atc_status_t`'ten gelir — eşik mantığı kullanıcı
kodunda kalır.

```c
static void rd_battery(char *b, size_t n, atc_status_t *st) {
    int pct = battery_pct();
    snprintf(b, n, "%d", pct);
    *st = (pct < 20) ? ATC_ST_ERR : (pct < 40) ? ATC_ST_WARN : ATC_ST_OK;
}

{ .type = ATC_ROW_BAR, .key = 'b', .label = "Battery", .read = rd_battery },
```

### `ATC_ROW_CHOICE` — N-state mod döngüsü

İki kullanım modu var:

- **Immediate cycle** (`choice_commit == NULL`): tuşa her basışta
  `*choice_idx` modulo ilerler, satır yerinde tazelenir. Backend hemen
  haberdar olur.
- **Browse-then-commit** (`choice_commit != NULL`): tuşa basmak edit
  mode'u açar; sonraki tuşlar pending seçimi gezdirir. `Enter` commit'ler
  ve callback'i tetikler, `Esc` revert eder. Backend yalnızca onay anında
  yazılır — pahalı veya yan-etkili setter'lar için doğru pattern.

```c
static const char *power_modes[] = { "ECO", "NORMAL", "TURBO" };
static uint8_t     power_idx     = 1;
static void        commit_power_mode(void) { apply_power_mode(power_idx); }

{ .type = ATC_ROW_CHOICE, .key = 'm', .label = "Power Mode",
  .choices = power_modes, .choice_count = 3, .choice_idx = &power_idx,
  .choice_commit = commit_power_mode },         /* opsiyonel */
```

Seçim string'leri ≤6 karakter olmalı (init uyarır).

### `ATC_ROW_INPUT` — runtime parametre girişi

Tuşa basınca inline editor açılır; çerçeve sonraki tüm tuşları yutar:
`Enter` validate + commit, `Esc` iptal, `Backspace` siler, geçerli
karakter buffer'a eklenir. Validation başarısız ise editor açık kalır ve
hata status satırına basılır. Tip seçenekleri: `ATC_INPUT_INT`,
`ATC_INPUT_FLOAT`, `ATC_INPUT_HEX`, `ATC_INPUT_STR`.

```c
static int32_t pwm_duty = 50;

static void rd_pwm(char *b, size_t n, atc_status_t *st) {
    snprintf(b, n, "%ld", (long)pwm_duty); *st = ATC_ST_OK;
}
static bool commit_pwm(const char *s) {
    pwm_duty = atol(s); apply_pwm(pwm_duty); return true;
}

{ .type = ATC_ROW_INPUT, .key = 'd', .label = "PWM Duty", .unit = "%",
  .read = rd_pwm, .input_type = ATC_INPUT_INT,
  .input_min = 0, .input_max = 100, .input_commit = commit_pwm },
```

Demo `examples/demo.c` `Visual Widgets` alt-menüsünde her üç widget'ın
çalışan örneğini içerir (`w` tuşu).

## Test mimarisi

- Çekirdek hiç dinamik bellek kullanmaz, transport'a sahip değildir.
- `ports/mock/` her TX byte'ını ve her `:` komutunu yakalar; testler
  `mock_buffer()`, `mock_last_cmd()` üzerinden assert eder.
- `tests/testing.h` minimal `EXPECT*` makrolarını ve `ATC_INIT_ITEMS`
  kısayolunu içerir; her test kendi binary'si, CTest tek tek koşturur.
