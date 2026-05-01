/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file demo.c
 * @brief End-to-end demo that drives the menu over a Windows serial port.
 *
 * Demonstrates the native sub-menu pattern: a small home menu shows
 * top-line metrics and acts as a launcher; SUBMENU rows drill into
 * sensor-specific views. The framework keeps the navigation stack and
 * the built-in 'b' key pops back — no thunks or manual back rows here.
 *
 * Simulated backends for sensors of varying output count:
 *
 *   - TMP102-like temp sensor : 1 value
 *   - INA219 power monitor    : 3 values  (Vbus, I, P)
 *   - BME280 environmental    : 4 values  (T, H, P, Alt)
 *   - MPU9250 9-DoF IMU       : 9 values  (3x accel, gyro, mag)
 *
 * ## Build (MinGW)
 *     cmake -S . -B build -G "MinGW Makefiles" -DATC_MENU_BUILD_EXAMPLES=ON
 *     cmake --build build --target demo
 *
 * ## Run
 *     build/examples/demo.exe COM8 [baud]
 *
 * Open the same port in an ANSI-capable serial terminal (PuTTY,
 * TeraTerm, screen, ...) at the matching baud (default 115200) 8N1.
 *
 * ## Hotkeys (home)
 *   t   show MCU temp          L   toggle LED
 *   v   show battery voltage   1   run self-test
 *   e   refresh BME280 group   i   open IMU view
 *   p   open Power view        r   repaint        :   command mode
 *
 * ## Hotkeys (IMU launcher — sub-menu, depth 1)
 *   a   accelerometer page     g   gyroscope page
 *   m   magnetometer page      b   back to home
 *
 * ## Hotkeys (any sub-menu)
 *   b   back to parent         ?   show full path
 *   r   repaint                :   command mode
 *
 * ## Command mode
 *   set temp <C>          override MCU temp
 *   set vbat <V>          override battery voltage
 *   set load <mA>         override INA219 current target (push to WARN/ERR)
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

/* ------------------------------------------------------------------ */
/* App state                                                           */
/* ------------------------------------------------------------------ */

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

static void act_toggle_led(void) { app_led_on = !app_led_on; }

static void act_self_test(void) {
    atc_menu_status("self test running...");
    Sleep(300);
    atc_menu_status("self test ok");
}

/* ------------------------------------------------------------------ */
/* Multi-value sensor readers — boilerplate squashed by READ_F     */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/* Command handler                                                     */
/* ------------------------------------------------------------------ */

static float app_load_target_ma = -1.0f;  /* <0 = use sim default */

static void app_cmd(const char *line) {
    if (strncmp(line, "set temp ", 9) == 0) {
        g_mcu.mcu_temp_c = (float)atof(line + 9);
    } else if (strncmp(line, "set vbat ", 9) == 0) {
        g_mcu.vbat_v = (float)atof(line + 9);
    } else if (strncmp(line, "set load ", 9) == 0) {
        app_load_target_ma = (float)atof(line + 9);
        g_ina.current_ma   = app_load_target_ma;
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

static const atc_menu_item_t imu_gyro_menu[] = {
    { .type = ATC_ROW_GROUP, .label = "MPU9250 Gyroscope" },
    { .type = ATC_ROW_VALUE, .label = "Gyro X", .unit = "dps", .read = rd_mpu_gx },
    { .type = ATC_ROW_VALUE, .label = "Gyro Y", .unit = "dps", .read = rd_mpu_gy },
    { .type = ATC_ROW_VALUE, .label = "Gyro Z", .unit = "dps", .read = rd_mpu_gz },
};

static const atc_menu_item_t imu_mag_menu[] = {
    { .type = ATC_ROW_GROUP, .label = "MPU9250 Magnetometer" },
    { .type = ATC_ROW_VALUE, .label = "Mag X", .unit = "uT", .read = rd_mpu_mx },
    { .type = ATC_ROW_VALUE, .label = "Mag Y", .unit = "uT", .read = rd_mpu_my },
    { .type = ATC_ROW_VALUE, .label = "Mag Z", .unit = "uT", .read = rd_mpu_mz },
};

static const atc_menu_item_t imu_menu[] = {
    { .type = ATC_ROW_GROUP,   .label = "MPU9250 IMU 9-DoF" },
    { .type = ATC_ROW_SUBMENU, .key = 'a', .label = "Accelerometer",
      .submenu = imu_accel_menu, .submenu_count = ARR_LEN(imu_accel_menu) },
    { .type = ATC_ROW_SUBMENU, .key = 'g', .label = "Gyroscope",
      .submenu = imu_gyro_menu,  .submenu_count = ARR_LEN(imu_gyro_menu) },
    { .type = ATC_ROW_SUBMENU, .key = 'm', .label = "Magnetometer",
      .submenu = imu_mag_menu,   .submenu_count = ARR_LEN(imu_mag_menu) },
};

static const atc_menu_item_t power_menu[] = {
    { .type = ATC_ROW_GROUP, .label = "INA219 Power" },
    { .type = ATC_ROW_VALUE, .label = "Bus V",   .unit = "V",  .read = rd_ina_v },
    { .type = ATC_ROW_VALUE, .label = "Current", .unit = "mA", .read = rd_ina_i },
    { .type = ATC_ROW_VALUE, .label = "Power",   .unit = "mW", .read = rd_ina_p },
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
      .submenu = imu_menu,   .submenu_count = ARR_LEN(imu_menu) },
    { .type = ATC_ROW_SUBMENU, .key = 'p', .label = "INA219 Power",
      .submenu = power_menu, .submenu_count = ARR_LEN(power_menu) },

    { .type = ATC_ROW_GROUP,   .label = "Control" },
    { .type = ATC_ROW_STATE,   .key = 'L', .label = "LED",
      .read = rd_led, .action = act_toggle_led },
    { .type = ATC_ROW_ACTION,  .key = '1', .label = "Self test",
      .action = act_self_test },
};

/* ------------------------------------------------------------------ */
/* Serial port                                                         */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <COMx> [baud]\n", argv[0]);
        return 1;
    }
    DWORD baud = (argc > 2) ? (DWORD)atoi(argv[2]) : 115200;
    if (!serial_open(argv[1], baud)) return 1;

    sensor_sim_init();

    atc_menu_set_info(&demo_info);
    atc_menu_init(home_menu, ARR_LEN(home_menu), &serial_port);
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
