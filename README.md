# atc_menu

UART üzerinden çalışan, tablo-driven, allocation-free C99 debug menü framework'ü.

## Özellikler

- Tek soyutlama: `row` — group / value / state / action
- `static const` tablo, sıfır dinamik bellek
- Flicker-free render (cursor-home + line-erase)
- Port vtable: UART / USB CDC / telnet / mock — transport değişirse sadece port değişir
- Built-in `:` komut modu + `r` refresh + printf-style status/log helper

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
baud'da) aç. Hotkey'ler:

| Tuş | Etki                          |
|-----|-------------------------------|
| `t` | MCU temp göster               |
| `v` | Battery voltage göster        |
| `L` | LED toggle                    |
| `1` | Self-test çalıştır            |
| `r` | Yenile                        |
| `:` | Komut modu (`set temp 55.0`, `set vbat 3.10`, `set load 1800`) |

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
