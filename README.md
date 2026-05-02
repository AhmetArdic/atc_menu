# atc_menu

UART üzerinden çalışan, tablo-driven, allocation-free C99 debug menü framework'ü.

## Özellikler

- Tek soyutlama: `row` — group / value / state / action / submenu / **bar / choice / input**
- `static const` tablo, sıfır dinamik bellek
- Flicker-free render (cursor-home + line-erase)
- Port vtable: UART / USB CDC / telnet / mock — transport değişirse sadece port değişir
- Built-in `:` komut modu + `r` refresh + `b` back + `?` path + printf-style status/log helper
- Native sub-menu: `ATC_ROW_SUBMENU` → çerçeve nav stack'ini kendi yönetir; aktif konumu görmek için `?` tam path'i status'a basar
- Görsel widget seti: `ATC_ROW_BAR` (yatay seviye), `ATC_ROW_CHOICE` (mod döngüsü), `ATC_ROW_INPUT` (runtime parametre girişi)

## Dizin yapısı

```
include/atc_menu/atc_menu.h   public API
src/menu.c                    çekirdek
src/{ansi,layout,symbols}.h   internal
ports/mock/                   test için TX + cmd capture
tests/                        CTest unit tests
examples/demo.c               Windows serial demo
design.md                     detaylı tasarım
```

## Build & test

Windows'ta MinGW (gcc + mingw32-make) ile:

```
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

Linux / macOS'ta default generator (Unix Makefiles) yeterli — `-G` flag'i atla.

CMake seçenekleri:

| Option                     | Default | Etki                            |
|----------------------------|---------|---------------------------------|
| `ATC_MENU_BUILD_TESTS`     | `ON`    | `tests/` altındaki CTest hedefleri |
| `ATC_MENU_BUILD_EXAMPLES`  | `OFF`   | `examples/demo` (Windows)       |

Render saf ASCII'dir; ANSI escape sequence destekleyen herhangi bir terminal yeterli (PuTTY, TeraTerm, screen, …).

## Serial demo (Windows)

```
cmake -S . -B build -G "MinGW Makefiles" -DATC_MENU_BUILD_EXAMPLES=ON
cmake --build build --target demo
build/examples/demo.exe COM8        # or any other COM port
build/examples/demo.exe COM3 9600   # optional baud override
```

PuTTY / TeraTerm ile aynı portu varsayılan 115200 8N1 (veya verdiğin
baud'da) aç. Demo home menüde Quick view + BME280 Env satırlarını inline
gösterir; IMU ve Power bilgisi SUBMENU'ye drill-down ile açılır. IMU
sayfası kendi içinde Accel/Gyro/Mag sub-page'lerine ayrılır (2 derinlik).
Sub-menüde `b` parent'a döner, `?` tam path'i status satırında basar.

Home hotkey'leri:

| Tuş | Etki                          |
|-----|-------------------------------|
| `t` | MCU temp göster               |
| `v` | Battery voltage göster        |
| `e` | BME280 Env grubunu yenile     |
| `i` | IMU sayfasına geç             |
| `p` | Power sayfasına geç           |
| `w` | Visual Widgets sayfasına geç  |
| `L` | LED toggle                    |
| `1` | Self-test çalıştır            |
| `r` | Yenile                        |
| `:` | Komut modu (`set temp 55.0`, `set vbat 3.10`, `set load 1800`) |

Sub-menüde ek olarak `b` parent menüye döner, `?` tam path'i basar.

Demo dört farklı çıktı sayısında sensör simülasyonu içerir: tek-değerli (TMP102),
3-değerli INA219 (V/I/P), 4-değerli BME280 (T/H/P/Alt) ve 9-değerli MPU9250
9-DoF IMU. Sim değerleri her ~50 ms'de rastgele yürüyüşle güncellenir; `r`
ile yenileyince yeni değerler görünür. `set load <mA>` INA219 akımını OK →
WARN → ERR eşiklerinin üstüne itmek için.

Detay: `examples/demo.c` brief'i.

## Çoklu sensör için ergonomi

Her satır kendi `read` callback'ini ister; INA219 (3 satır) ve özellikle
9-DoF IMU (9 satır) için bu hızla boilerplate'e dönüşür. Demo bunu üç
basit paternle çözüyor — `examples/sensor_helpers.h` ve `examples/sensor_sim.{h,c}`:

1. **Sensör başına struct + tick.** Her sensör (`ina219_t`, `bme280_t`,
   `mpu9250_t`) küçük bir struct'ta yaşar. `sensor_sim_tick()` (gerçekte
   bir I²C sürücüsü) struct'ı günceller; menü sadece field'ı okur.
   Sürücü ile UI ayrı katmanda kalır.
2. **`READ_F` makrosu.** Bir field'i okuyup formatlayan callback'i
   tek satırda üretir:
   ```c
   READ_F(ina_v, g_ina.bus_v, "%.2f",
          st_range(g_ina.bus_v, 3.0f, 4.2f))
   ```
   Bu, dört satırlık callback'i bire indirir. `st_range/max/min`
   yardımcıları sık kullanılan threshold mantığını sarmalar.
3. **Grup başına sensör.** Tabloda her chip için `ATC_ROW_GROUP`
   (`"INA219 Power"`, `"BME280 Env"`, `"MPU9250 IMU"`) — kalabalık
   menüde göz hızla doğru bloğa gider.

Bu yardımcılar `examples/` altında, kütüphane API'sinin parçası değil:
projenin kendi yardımcılarını eklemekte serbestsin.

## Sub-menü ile satır sayısını yönetmek

Sensör sayısı arttıkça tüm satırları tek ekrana sığdırmak okunaksızlaşır.
Çözüm: `ATC_ROW_SUBMENU` row'u ile drill-down. Her satır kendi `submenu`
pointer'ını taşır; çerçeve nav stack'ini yönetir, `b` parent'a döner,
`?` tam path'i (`Home > MPU9250 IMU > Accelerometer`) status satırında
basar. Header değişmez — aktif tablonun `ATC_ROW_GROUP` başlığı zaten
"buradayım" sinyali.

```c
static const atc_menu_item_t imu_menu[] = {
    { .type = ATC_ROW_GROUP, .label = "MPU9250 IMU" },
    { .type = ATC_ROW_VALUE, .label = "Accel X", .unit = "g", .read = rd_ax },
    /* ... */
};

static const atc_menu_item_t home_menu[] = {
    { .type = ATC_ROW_VALUE,   .key = 't', .label = "MCU Temp", .unit = "C",
      .read = rd_temp },
    { .type = ATC_ROW_SUBMENU, .key = 'i', .label = "MPU9250 IMU",
      .submenu = imu_menu,
      .submenu_count = sizeof imu_menu / sizeof imu_menu[0] },
};
```

SUBMENU satırı `i  > MPU9250 IMU` şeklinde `> ` marker'ıyla çıkar;
footer'a `[b] back  [?] path` eklenir. Submenu içinde başka SUBMENU
olabilir — demo `home → MPU9250 IMU → {Accel|Gyro|Mag}` 2 derinlik
gezinti gösterir.

Stack derinliği varsayılan 4; `-DATC_MENU_STACK_DEPTH=8` ile artırılabilir.

### Rezerve sistem tuşları

Çerçeve dört tuşu kendi kullanımı için ayırır — kullanıcı satırlarına bu
tuşlardan birini atamak init validation'da uyarı çıkarır:

| Tuş | İşlev                       |
|-----|-----------------------------|
| `r` | tam refresh                 |
| `b` | parent menüye dön           |
| `?` | tam path'i status'a yaz     |
| `:` | komut modu (port.cmd ayarlı ise) |

Bu tuşlar `src/menu.c` içinde `ATC_KEY_REFRESH`, `ATC_KEY_BACK`, `ATC_KEY_PATH`,
`ATC_KEY_CMD` define'larıyla tanımlıdır; tek noktadan değiştirilebilir.

### Satır layout'u ve genişlikler

Tüm satırlar aynı sütun düzenini paylaşır; `src/layout.h` içinde derived
şekilde tanımlıdır — bir sütun değişirse `MENU_HDR_INNER` ve box otomatik
ayarlanır:

```
| K | LABEL                    | VALUE      | UNIT  | STAT |
  ^   ^MENU_LABEL_COL          ^MENU_VALUE  ^UNIT   ^STATUS
```

| Define              | Default | Anlamı                        |
|---------------------|---------|-------------------------------|
| `MENU_KEY_COL`      | 1       | Hotkey karakter sütunu        |
| `MENU_LABEL_COL`    | 24      | Sol-hizalı etiket             |
| `MENU_VALUE_COL`    | 10      | Sağ-hizalı değer / widget gövdesi |
| `MENU_UNIT_COL`     | 5       | Sol-hizalı birim              |
| `MENU_STATUS_COL`   | 4       | Status (max "WARN")           |
| `MENU_SEP_W`        | 3       | Field ayraç (` \| `)          |
| `MENU_PAD_W`        | 1       | Border içi pad boşluğu        |

## Görsel widget'lar

Üç ek satır tipi gömülü-debug menüsünün eksik kalan kategorilerini doldurur.
Hepsi saf ASCII; mevcut 50-char iç genişliği ve 24+10+5 sütun yerleşimini bozmaz.

### `ATC_ROW_BAR` — yatay seviye çubuğu

```
|  b  Battery               [########..] 78 %    OK  |
|  c  CPU Load              [######....] 62 %  WARN  |
|  m  Free RAM              [##........] 12 %   ERR  |
```

10 char value sütununa `[` + 8 hücre + `]` çubuk yerleşir; `read` ASCII yüzde
(`"78"`) yazar, çerçeve dolu/boş hücreleri çizer ve yüzdeyi unit sütununda
basar. Renk `read`'in döndürdüğü `atc_status_t`'ten gelir — eşik mantığı
kullanıcı kodunda kalır.

```c
static void rd_battery(char *b, size_t n, atc_status_t *st) {
    int pct = battery_pct();
    snprintf(b, n, "%d", pct);
    *st = (pct < 20) ? ATC_ST_ERR : (pct < 40) ? ATC_ST_WARN : ATC_ST_OK;
}

{ .type = ATC_ROW_BAR, .key = 'b', .label = "Battery", .read = rd_battery },
```

### `ATC_ROW_CHOICE` — N-state mod döngüsü

```
|  m  Power Mode             [ NORMAL ]            OK  |
|  f  Fan Curve              [  AUTO  ]            OK  |
```

Tuşa her basışta `*choice_idx` modulo ilerler; satır yerinde tazelenir.
Seçim string'leri ≤6 karakter olmalı (init uyarır).

```c
static const char *power_modes[] = { "ECO", "NORMAL", "TURBO" };
static uint8_t     power_idx = 1;

{ .type = ATC_ROW_CHOICE, .key = 'm', .label = "Power Mode",
  .choices = power_modes, .choice_count = 3, .choice_idx = &power_idx },
```

Kalıcılık (flash/NVS) kullanıcının sorumluluğunda — `choice_idx` pointer'ı
uygulamanın RAM'inde duruyor.

### `ATC_ROW_INPUT` — runtime parametre girişi

```
Normal:  |  d  PWM Duty                       50  %         OK  |
Editing: |  d  PWM Duty            =75_                          |
Footer:  [Enter] commit  [Esc] cancel  [BS] erase    range: 0..100
```

Tuşa basınca inline editor açılır; çerçeve sonraki tüm tuşları yutar:
`Enter` validate + commit, `Esc` iptal, `Backspace` siler, geçerli karakter
buffer'a eklenir. Validation başarısız ise editor açık kalır ve hata
status satırına basılır. Tip seçenekleri: `ATC_INPUT_INT`, `ATC_INPUT_FLOAT`,
`ATC_INPUT_HEX`, `ATC_INPUT_STR`.

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

## Hızlı başlangıç

```c
#include "atc_menu/atc_menu.h"

static void rd_temp(char *b, size_t n, atc_status_t *st) {
    snprintf(b, n, "%.1f", read_temp_c());
    *st = ATC_ST_OK;
}
static void act_toggle_led(void) { led_toggle(); }

static const atc_menu_item_t items[] = {
    { .type = ATC_ROW_GROUP,  .label = "Sensors", .unit = "" },
    { .type = ATC_ROW_VALUE,  .key = 't', .label = "Temp", .unit = "C",
                              .read = rd_temp },
    { .type = ATC_ROW_STATE,  .key = 'L', .label = "LED",  .unit = "",
                              .read = rd_led, .action = act_toggle_led },
    { .type = ATC_ROW_ACTION, .key = '1', .label = "Self test", .unit = "",
                              .action = act_self_test },
};

atc_menu_init(items, sizeof items / sizeof items[0], &my_port);
atc_menu_render();
for (;;) atc_menu_handle_key(uart_getc());
```

Yeni bir transport eklemek = `atc_menu_port_t` içine `write` (ve isteğe bağlı `cmd`) implemente etmek.
Yeni bir satır eklemek = tabloya bir satır + bir read/action callback.

## Test mimarisi

- Çekirdek hiç dinamik bellek kullanmaz, transport'a sahip değildir.
- `ports/mock/` her TX byte'ını ve her `:` komutunu yakalar; testler `mock_buffer()`, `mock_last_cmd()` üzerinden assert eder.
- `tests/testing.h` minimal `EXPECT*` makrolarını içerir; her test kendi binary'si, CTest tek tek koşturur.
