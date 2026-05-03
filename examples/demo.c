/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file demo.c
 * @brief End-to-end demo driving the menu over a Windows serial port.
 *
 * Build:  cmake -S . -B build -G "MinGW Makefiles" -DATC_MENU_BUILD_EXAMPLES=ON
 *         cmake --build build --target demo
 * Run:    build/examples/demo.exe COM8 [baud]
 *
 * Open the same port in an ANSI-capable serial terminal (PuTTY, TeraTerm,
 * screen) at 115200 8N1 by default. Hotkeys are visible on-screen.
 *
 * Command mode (':'):
 *   set temp <C>    override MCU temp
 *   set vbat <V>    override battery voltage
 *   set load <mA>   override INA219 current target (push to WARN/ERR)
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "atc_menu/atc_menu.h"
#include "sensor_helpers.h"
#include "sensor_sim.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARR_LEN(a) (sizeof(a) / sizeof((a)[0]))

static HANDLE g_serial = INVALID_HANDLE_VALUE;
static size_t g_tx_bytes;

static bool app_led_on;

static void rd_temp(char *b, size_t n, atc_status_t *st) {
    snprintf(b, n, "%.1f", g_mcu.mcu_temp_c);
    *st = (g_mcu.mcu_temp_c < 70.0f) ? ATC_ST_OK : ATC_ST_WARN;
}

static void rd_vbat(char *b, size_t n, atc_status_t *st) {
    snprintf(b, n, "%.2f", g_mcu.vbat_v);
    *st = (g_mcu.vbat_v > 3.3f) ? ATC_ST_OK : ATC_ST_ERR;
}

static void rd_led(char *b, size_t n, atc_status_t *st) {
    (void)b; (void)n;
    *st = app_led_on ? ATC_ST_ON : ATC_ST_OFF;
}

static void act_toggle_led(void) {
    app_led_on = !app_led_on;
    fprintf(stderr, "  [backend] LED -> %s\n", app_led_on ? "ON" : "OFF");
}

static void act_self_test(void) {
    fprintf(stderr, "  [backend] self test running...\n");
    atc_menu_status("self test running...");
    Sleep(300);
    atc_menu_status("self test ok");
    fprintf(stderr, "  [backend] self test ok\n");
}

READ_F(ina_v, g_ina.bus_v,      "%.2f", st_range(g_ina.bus_v, 3.0f, 4.2f))
READ_F(ina_i, g_ina.current_ma, "%.0f", st_max(g_ina.current_ma, 1500.f, 2500.f))
READ_F(ina_p, g_ina.power_mw,   "%.0f", ATC_ST_NONE)

READ_F(bme_t, g_bme.temp_c,       "%.1f", st_range(g_bme.temp_c, 0.f, 50.f))
READ_F(bme_h, g_bme.humidity,     "%.1f", st_range(g_bme.humidity, 20.f, 80.f))
READ_F(bme_p, g_bme.pressure_hpa, "%.1f", ATC_ST_NONE)
READ_F(bme_a, g_bme.altitude_m,   "%.2f", ATC_ST_NONE)

READ_F(mpu_ax, g_mpu.ax, "%+.2f", ATC_ST_NONE)
READ_F(mpu_ay, g_mpu.ay, "%+.2f", ATC_ST_NONE)
READ_F(mpu_az, g_mpu.az, "%+.2f", ATC_ST_NONE)
READ_F(mpu_gx, g_mpu.gx, "%+.1f", ATC_ST_NONE)
READ_F(mpu_gy, g_mpu.gy, "%+.1f", ATC_ST_NONE)
READ_F(mpu_gz, g_mpu.gz, "%+.1f", ATC_ST_NONE)
READ_F(mpu_mx, g_mpu.mx, "%+.1f", ATC_ST_NONE)
READ_F(mpu_my, g_mpu.my, "%+.1f", ATC_ST_NONE)
READ_F(mpu_mz, g_mpu.mz, "%+.1f", ATC_ST_NONE)

static uint8_t      app_pwr_mode = 1;
static uint8_t      app_fan_mode = 0;
static int32_t      app_pwm_duty = 50;
static int32_t      app_threshold_mv = 3300;
static unsigned int app_cpu_tick;

static const char *app_pwr_choices[] = { "ECO", "NORMAL", "TURBO" };
static const char *app_fan_choices[] = { "AUTO", "LOW", "MED", "HIGH" };

static void commit_pwr_mode(void) {
    fprintf(stderr, "  [backend] Power mode -> %s\n", app_pwr_choices[app_pwr_mode]);
}
static void commit_fan_mode(void) {
    fprintf(stderr, "  [backend] Fan curve -> %s\n",  app_fan_choices[app_fan_mode]);
}

static void rd_battery_pct(char *b, size_t n, atc_status_t *st) {
    float v = g_mcu.vbat_v;
    if (v < 3.0f) v = 3.0f;
    if (v > 4.2f) v = 4.2f;
    int pct = (int)((v - 3.0f) * 100.0f / 1.2f + 0.5f);
    snprintf(b, n, "%d", pct);
    *st = (pct < 20) ? ATC_ST_ERR : (pct < 40) ? ATC_ST_WARN : ATC_ST_OK;
}

static void rd_cpu_load(char *b, size_t n, atc_status_t *st) {
    unsigned int t = (app_cpu_tick++) % 200;
    int pct = (t < 100) ? (int)t : (int)(200 - t);
    snprintf(b, n, "%d", pct);
    *st = (pct >= 85) ? ATC_ST_ERR : (pct >= 60) ? ATC_ST_WARN : ATC_ST_OK;
}

static void rd_pwm_duty(char *b, size_t n, atc_status_t *st) {
    snprintf(b, n, "%ld", (long)app_pwm_duty);
    *st = ATC_ST_OK;
}

static void rd_threshold(char *b, size_t n, atc_status_t *st) {
    snprintf(b, n, "%ld", (long)app_threshold_mv);
    *st = ATC_ST_OK;
}

static bool commit_pwm_duty(const char *s) {
    app_pwm_duty = atol(s);
    fprintf(stderr, "  [backend] PWM duty -> %ld %%\n", (long)app_pwm_duty);
    return true;
}
static bool commit_threshold(const char *s) {
    app_threshold_mv = atol(s);
    fprintf(stderr, "  [backend] Threshold -> %ld mV\n", (long)app_threshold_mv);
    return true;
}

static const atc_menu_item_t widgets_menu[] = {
    { .type = ATC_ROW_GROUP,  .label = "Levels (BAR)" },
    { .type = ATC_ROW_BAR,    .key = 'B', .label = "Battery",
      .read = rd_battery_pct },
    { .type = ATC_ROW_BAR,    .key = 'c', .label = "CPU Load",
      .read = rd_cpu_load },

    { .type = ATC_ROW_GROUP,  .label = "Modes (CHOICE)" },
    { .type = ATC_ROW_CHOICE, .key = 'm', .label = "Power Mode",
      .choices = app_pwr_choices, .choice_count = 3, .choice_idx = &app_pwr_mode,
      .choice_commit = commit_pwr_mode },
    { .type = ATC_ROW_CHOICE, .key = 'f', .label = "Fan Curve",
      .choices = app_fan_choices, .choice_count = 4, .choice_idx = &app_fan_mode,
      .choice_commit = commit_fan_mode },

    { .type = ATC_ROW_GROUP,  .label = "Setpoints (INPUT)" },
    { .type = ATC_ROW_INPUT,  .key = 'd', .label = "PWM Duty", .unit = "%",
      .read = rd_pwm_duty, .input_type = ATC_INPUT_INT,
      .input_min = 0, .input_max = 100, .input_commit = commit_pwm_duty },
    { .type = ATC_ROW_INPUT,  .key = 'h', .label = "Threshold", .unit = "mV",
      .read = rd_threshold, .input_type = ATC_INPUT_INT,
      .input_min = 0, .input_max = 5000, .input_commit = commit_threshold },
};

static float app_load_target_ma = -1.0f;  /* <0 = use sim default */

static void app_cmd(const char *line) {
    if (strncmp(line, "set temp ", 9) == 0) {
        g_mcu.mcu_temp_c = (float)atof(line + 9);
        fprintf(stderr, "  [backend] MCU temp override -> %.1f C\n", g_mcu.mcu_temp_c);
    } else if (strncmp(line, "set vbat ", 9) == 0) {
        g_mcu.vbat_v = (float)atof(line + 9);
        fprintf(stderr, "  [backend] Vbat override -> %.2f V\n", g_mcu.vbat_v);
    } else if (strncmp(line, "set load ", 9) == 0) {
        app_load_target_ma = (float)atof(line + 9);
        g_ina.current_ma   = app_load_target_ma;
        fprintf(stderr, "  [backend] INA219 load -> %.0f mA\n", app_load_target_ma);
    } else if (line[0]) {
        atc_menu_status("unknown cmd");
    }
}

static const atc_menu_info_t demo_info = {
    .project = "ATC Menu Demo",
    .version = "1.0.0",
    .author  = "Ahmet Talha ARDIC",
    .build   = __DATE__,
};

static const atc_menu_item_t imu_accel_menu[] = {
    { .type = ATC_ROW_GROUP, .label = "MPU9250 Accelerometer" },
    { .type = ATC_ROW_VALUE, .label = "Accel X", .unit = "g", .read = rd_mpu_ax },
    { .type = ATC_ROW_VALUE, .label = "Accel Y", .unit = "g", .read = rd_mpu_ay },
    { .type = ATC_ROW_VALUE, .label = "Accel Z", .unit = "g", .read = rd_mpu_az },
};
static const atc_menu_table_t imu_accel_table = {
    .items = imu_accel_menu, .count = ARR_LEN(imu_accel_menu),
};

static const atc_menu_item_t imu_gyro_menu[] = {
    { .type = ATC_ROW_GROUP, .label = "MPU9250 Gyroscope" },
    { .type = ATC_ROW_VALUE, .label = "Gyro X", .unit = "dps", .read = rd_mpu_gx },
    { .type = ATC_ROW_VALUE, .label = "Gyro Y", .unit = "dps", .read = rd_mpu_gy },
    { .type = ATC_ROW_VALUE, .label = "Gyro Z", .unit = "dps", .read = rd_mpu_gz },
};
static const atc_menu_table_t imu_gyro_table = {
    .items = imu_gyro_menu, .count = ARR_LEN(imu_gyro_menu),
};

static const atc_menu_item_t imu_mag_menu[] = {
    { .type = ATC_ROW_GROUP, .label = "MPU9250 Magnetometer" },
    { .type = ATC_ROW_VALUE, .label = "Mag X", .unit = "uT", .read = rd_mpu_mx },
    { .type = ATC_ROW_VALUE, .label = "Mag Y", .unit = "uT", .read = rd_mpu_my },
    { .type = ATC_ROW_VALUE, .label = "Mag Z", .unit = "uT", .read = rd_mpu_mz },
};
static const atc_menu_table_t imu_mag_table = {
    .items = imu_mag_menu, .count = ARR_LEN(imu_mag_menu),
};

static const atc_menu_item_t imu_menu[] = {
    { .type = ATC_ROW_GROUP,   .label = "MPU9250 IMU 9-DoF" },
    { .type = ATC_ROW_SUBMENU, .key = 'a', .label = "Accelerometer",
      .submenu = &imu_accel_table },
    { .type = ATC_ROW_SUBMENU, .key = 'g', .label = "Gyroscope",
      .submenu = &imu_gyro_table },
    { .type = ATC_ROW_SUBMENU, .key = 'm', .label = "Magnetometer",
      .submenu = &imu_mag_table },
};
static const char *const imu_notes[] = {
    "9-DoF IMU, I2C @ 0x68, sample rate 100 Hz.",
    "Drill into a sub-axis page for individual readouts.",
};
static const atc_menu_table_t imu_table = {
    .items = imu_menu, .count = ARR_LEN(imu_menu),
    .notes = imu_notes, .note_count = ARR_LEN(imu_notes),
};

static const atc_menu_item_t power_menu[] = {
    { .type = ATC_ROW_GROUP, .label = "INA219 Power" },
    { .type = ATC_ROW_VALUE, .label = "Bus V",   .unit = "V",  .read = rd_ina_v },
    { .type = ATC_ROW_VALUE, .label = "Current", .unit = "mA", .read = rd_ina_i },
    { .type = ATC_ROW_VALUE, .label = "Power",   .unit = "mW", .read = rd_ina_p },
};
static const char *const power_notes[] = {
    "INA219 high-side current monitor, 0.1 ohm shunt.",
    "':set load <mA>' to push WARN/ERR thresholds.",
};
static const atc_menu_table_t power_table = {
    .items = power_menu, .count = ARR_LEN(power_menu),
    .notes = power_notes, .note_count = ARR_LEN(power_notes),
};

static const atc_menu_table_t widgets_table = {
    .items = widgets_menu, .count = ARR_LEN(widgets_menu),
};

static const atc_menu_item_t home_menu[] = {
    { .type = ATC_ROW_GROUP,   .label = "Quick view" },
    { .type = ATC_ROW_VALUE,   .key = 't', .label = "MCU Temp", .unit = "C",
      .read = rd_temp },
    { .type = ATC_ROW_VALUE,   .key = 'v', .label = "Battery",  .unit = "V",
      .read = rd_vbat },

    { .type = ATC_ROW_GROUP,   .key = 'e', .label = "BME280 Env" },
    { .type = ATC_ROW_VALUE,   .label = "Temperature", .unit = "C",   .read = rd_bme_t },
    { .type = ATC_ROW_VALUE,   .label = "Humidity",    .unit = "%",   .read = rd_bme_h },
    { .type = ATC_ROW_VALUE,   .label = "Pressure",    .unit = "hPa", .read = rd_bme_p },
    { .type = ATC_ROW_VALUE,   .label = "Altitude",    .unit = "m",   .read = rd_bme_a },

    { .type = ATC_ROW_GROUP,   .label = "Drill into" },
    { .type = ATC_ROW_SUBMENU, .key = 'i', .label = "MPU9250 IMU",
      .submenu = &imu_table },
    { .type = ATC_ROW_SUBMENU, .key = 'p', .label = "INA219 Power",
      .submenu = &power_table },
    { .type = ATC_ROW_SUBMENU, .key = 'w', .label = "Visual Widgets",
      .submenu = &widgets_table },

    { .type = ATC_ROW_GROUP,   .label = "Control" },
    { .type = ATC_ROW_STATE,   .key = 'L', .label = "LED",
      .read = rd_led, .action = act_toggle_led },
    { .type = ATC_ROW_ACTION,  .key = '1', .label = "Self test",
      .action = act_self_test },
};
static const char *const home_notes[] = {
    "Press a row's hotkey to interact with it.",
    "Type ':' for command mode (e.g., set temp 30).",
};
static const atc_menu_table_t home_table = {
    .items = home_menu, .count = ARR_LEN(home_menu),
    .notes = home_notes, .note_count = ARR_LEN(home_notes),
};

static void com_write(const char *buf, size_t len) {
    g_tx_bytes += len;
    DWORD written = 0;
    WriteFile(g_serial, buf, (DWORD)len, &written, NULL);
}

static const atc_menu_port_t serial_port = {
    .write = com_write,
    .cmd   = app_cmd,
};

static bool serial_open(const char *port, DWORD baud) {
    char path[32];
    snprintf(path, sizeof path, "\\\\.\\%s", port);

    g_serial = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                           0, NULL, OPEN_EXISTING, 0, NULL);
    if (g_serial == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "open %s failed: %lu\n", port, GetLastError());
        return false;
    }

    DCB dcb = { .DCBlength = sizeof dcb };
    if (!GetCommState(g_serial, &dcb)) {
        fprintf(stderr, "GetCommState failed: %lu\n", GetLastError());
        return false;
    }
    dcb.BaudRate = baud;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary  = TRUE;
    if (!SetCommState(g_serial, &dcb)) {
        fprintf(stderr, "SetCommState failed: %lu\n", GetLastError());
        return false;
    }

    COMMTIMEOUTS to = {
        .ReadIntervalTimeout         = MAXDWORD,
        .ReadTotalTimeoutConstant    = 50,
        .ReadTotalTimeoutMultiplier  = 0,
        .WriteTotalTimeoutConstant   = 50,
        .WriteTotalTimeoutMultiplier = 10,
    };
    SetCommTimeouts(g_serial, &to);
    return true;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <COMx> [baud]\n", argv[0]);
        return 1;
    }
    DWORD baud = (argc > 2) ? (DWORD)atoi(argv[2]) : 115200;
    if (!serial_open(argv[1], baud)) return 1;

    sensor_sim_init();

    atc_menu_init(&home_table, &serial_port, &demo_info);
    atc_menu_render();
    fprintf(stderr, "initial render: %zu B\n", g_tx_bytes);

    for (;;) {
        char  c     = 0;
        DWORD nread = 0;

        sensor_sim_tick();
        if (app_load_target_ma >= 0.0f) g_ina.current_ma = app_load_target_ma;

        if (ReadFile(g_serial, &c, 1, &nread, NULL) && nread == 1) {
            size_t before = g_tx_bytes;
            atc_menu_handle_key(c);
            size_t emitted = g_tx_bytes - before;
            if (emitted) fprintf(stderr, "key %c: %zu B\n", c, emitted);
        }
    }
}
