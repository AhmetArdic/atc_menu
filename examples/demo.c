/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file demo.c
 * @brief End-to-end demo driving the menu over a serial port (Windows + POSIX).
 *
 * Build (Windows):
 *   cmake -S . -B build -G "MinGW Makefiles" -DATC_MENU_BUILD_EXAMPLES=ON
 *   cmake --build build --target demo
 *   build/examples/demo.exe COM8 [baud]
 *
 * Build (Linux):
 *   cmake -S . -B build -DATC_MENU_BUILD_EXAMPLES=ON
 *   cmake --build build --target demo
 *   build/examples/demo /dev/ttyUSB0 [baud]
 *
 * To try it without hardware on Linux, create a virtual serial pair with
 * socat and connect a terminal to the other end:
 *   socat -d -d pty,raw,echo=0 pty,raw,echo=0
 *   build/examples/demo /dev/pts/<a>
 *   screen /dev/pts/<b> 115200
 *
 * Open the port in an ANSI-capable serial terminal (PuTTY, TeraTerm, screen,
 * picocom, minicom) at 115200 8N1 by default. Hotkeys are visible on-screen.
 *
 * Command mode (':'):
 *   set temp <C>    override MCU temp
 *   set vbat <V>    override battery voltage
 *   set load <mA>   override INA219 current target (push to WARN/ERR)
 *
 * Every menu is declared with the compile-time ATC_MENU() macro family,
 * so the whole tree lives in .rodata and costs zero RAM.
 */

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
   /* Expose POSIX (nanosleep) and BSD (cfmakeraw) extensions under strict C11. */
#  ifndef _DEFAULT_SOURCE
#    define _DEFAULT_SOURCE
#  endif
#  ifndef _POSIX_C_SOURCE
#    define _POSIX_C_SOURCE 200809L
#  endif
#  include <errno.h>
#  include <fcntl.h>
#  include <termios.h>
#  include <time.h>
#  include <unistd.h>
#endif

#include "atc_menu/menu.h"
#include "sensor_helpers.h"
#include "sensor_sim.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sleep_ms(unsigned ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
#endif
}

#define ARR_LEN(a) (sizeof(a) / sizeof((a)[0]))

#ifdef _WIN32
static HANDLE g_serial = INVALID_HANDLE_VALUE;
#else
static int    g_serial = -1;
#endif
static size_t g_tx_bytes;

/* -------------------------------------------------------------- read callbacks */

static bool app_led_on;

static void rd_temp(char *b, size_t n, atc_menu_status_t *st) {
    snprintf(b, n, "%.1f", g_mcu.mcu_temp_c);
    *st = (g_mcu.mcu_temp_c < 70.0f) ? ATC_MENU_ST_OK : ATC_MENU_ST_WARN;
}

static void rd_vbat(char *b, size_t n, atc_menu_status_t *st) {
    snprintf(b, n, "%.2f", g_mcu.vbat_v);
    *st = (g_mcu.vbat_v > 3.3f) ? ATC_MENU_ST_OK : ATC_MENU_ST_ERR;
}

static void rd_led(char *b, size_t n, atc_menu_status_t *st) {
    (void)b; (void)n;
    *st = app_led_on ? ATC_MENU_ST_ON : ATC_MENU_ST_OFF;
}

static void act_toggle_led(void) {
    app_led_on = !app_led_on;
    fprintf(stderr, "  [backend] LED -> %s\n", app_led_on ? "ON" : "OFF");
}

static void act_self_test(void) {
    fprintf(stderr, "  [backend] self test running...\n");
    atc_menu_status("self test running...");
    sleep_ms(300);
    atc_menu_status("self test ok");
    fprintf(stderr, "  [backend] self test ok\n");
}

READ_F(ina_v, g_ina.bus_v,      "%.2f", st_range(g_ina.bus_v, 3.0f, 4.2f))
READ_F(ina_i, g_ina.current_ma, "%.0f", st_max(g_ina.current_ma, 1500.f, 2500.f))
READ_F(ina_p, g_ina.power_mw,   "%.0f", ATC_MENU_ST_NONE)

READ_F(bme_t, g_bme.temp_c,       "%.1f", st_range(g_bme.temp_c, 0.f, 50.f))
READ_F(bme_h, g_bme.humidity,     "%.1f", st_range(g_bme.humidity, 20.f, 80.f))
READ_F(bme_p, g_bme.pressure_hpa, "%.1f", ATC_MENU_ST_NONE)
READ_F(bme_a, g_bme.altitude_m,   "%.2f", ATC_MENU_ST_NONE)

READ_F(mpu_ax, g_mpu.ax, "%+.2f", ATC_MENU_ST_NONE)
READ_F(mpu_ay, g_mpu.ay, "%+.2f", ATC_MENU_ST_NONE)
READ_F(mpu_az, g_mpu.az, "%+.2f", ATC_MENU_ST_NONE)
READ_F(mpu_gx, g_mpu.gx, "%+.1f", ATC_MENU_ST_NONE)
READ_F(mpu_gy, g_mpu.gy, "%+.1f", ATC_MENU_ST_NONE)
READ_F(mpu_gz, g_mpu.gz, "%+.1f", ATC_MENU_ST_NONE)
READ_F(mpu_mx, g_mpu.mx, "%+.1f", ATC_MENU_ST_NONE)
READ_F(mpu_my, g_mpu.my, "%+.1f", ATC_MENU_ST_NONE)
READ_F(mpu_mz, g_mpu.mz, "%+.1f", ATC_MENU_ST_NONE)

static uint_least8_t app_pwr_mode = 1;
static uint_least8_t app_fan_mode = 0;
static int32_t       app_pwm_duty = 50;
static int32_t       app_threshold_mv = 3300;
static unsigned int  app_cpu_tick;

static const char *app_pwr_choices[] = { "ECO", "NORMAL", "TURBO" };
static const char *app_fan_choices[] = { "AUTO", "LOW", "MED", "HIGH" };

static void commit_pwr_mode(void) {
    fprintf(stderr, "  [backend] Power mode -> %s\n", app_pwr_choices[app_pwr_mode]);
}
static void commit_fan_mode(void) {
    fprintf(stderr, "  [backend] Fan curve -> %s\n",  app_fan_choices[app_fan_mode]);
}

static void rd_battery_pct(char *b, size_t n, atc_menu_status_t *st) {
    float v = g_mcu.vbat_v;
    if (v < 3.0f) v = 3.0f;
    if (v > 4.2f) v = 4.2f;
    int pct = (int)((v - 3.0f) * 100.0f / 1.2f + 0.5f);
    snprintf(b, n, "%d", pct);
    *st = (pct < 20) ? ATC_MENU_ST_ERR : (pct < 40) ? ATC_MENU_ST_WARN : ATC_MENU_ST_OK;
}

static void rd_cpu_load(char *b, size_t n, atc_menu_status_t *st) {
    unsigned int t = (app_cpu_tick++) % 200;
    int pct = (t < 100) ? (int)t : (int)(200 - t);
    snprintf(b, n, "%d", pct);
    *st = (pct >= 85) ? ATC_MENU_ST_ERR : (pct >= 60) ? ATC_MENU_ST_WARN : ATC_MENU_ST_OK;
}

static void rd_pwm_duty(char *b, size_t n, atc_menu_status_t *st) {
    snprintf(b, n, "%ld", (long)app_pwm_duty);
    *st = ATC_MENU_ST_OK;
}

static void rd_threshold(char *b, size_t n, atc_menu_status_t *st) {
    snprintf(b, n, "%ld", (long)app_threshold_mv);
    *st = ATC_MENU_ST_OK;
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

/* -------------------------------------------------------------- leaf menus (Flash) */

ATC_MENU(imu_accel,
    ATC_GROUP(    "MPU9250 Accelerometer"),
    ATC_VALUE(0, "Accel X", "g", rd_mpu_ax),
    ATC_VALUE(0, "Accel Y", "g", rd_mpu_ay),
    ATC_VALUE(0, "Accel Z", "g", rd_mpu_az),
);

ATC_MENU(imu_gyro,
    ATC_GROUP(    "MPU9250 Gyroscope"),
    ATC_VALUE(0, "Gyro X", "dps", rd_mpu_gx),
    ATC_VALUE(0, "Gyro Y", "dps", rd_mpu_gy),
    ATC_VALUE(0, "Gyro Z", "dps", rd_mpu_gz),
);

ATC_MENU(imu_mag,
    ATC_GROUP(    "MPU9250 Magnetometer"),
    ATC_VALUE(0, "Mag X", "uT", rd_mpu_mx),
    ATC_VALUE(0, "Mag Y", "uT", rd_mpu_my),
    ATC_VALUE(0, "Mag Z", "uT", rd_mpu_mz),
);

ATC_MENU(imu,
    ATC_GROUP  (     "MPU9250 IMU 9-DoF"),
    ATC_SUBMENU('a', "Accelerometer", &imu_accel),
    ATC_SUBMENU('g', "Gyroscope",     &imu_gyro),
    ATC_SUBMENU('m', "Magnetometer",  &imu_mag),
    ATC_NOTE   ("9-DoF IMU, I2C @ 0x68, sample rate 100 Hz."),
    ATC_NOTE   ("Drill into a sub-axis page for individual readouts."),
);

ATC_MENU(power,
    ATC_GROUP(    "INA219 Power"),
    ATC_VALUE(0, "Bus V",   "V",  rd_ina_v),
    ATC_VALUE(0, "Current", "mA", rd_ina_i),
    ATC_VALUE(0, "Power",   "mW", rd_ina_p),
    ATC_NOTE ("INA219 high-side current monitor, 0.1 ohm shunt."),
    ATC_NOTE ("':set load <mA>' to push WARN/ERR thresholds."),
);

ATC_MENU(widgets,
    ATC_GROUP (     "Levels (BAR)"),
    ATC_BAR   ('B', "Battery",          rd_battery_pct),
    ATC_BAR   ('c', "CPU Load",         rd_cpu_load),

    ATC_GROUP (     "Modes (CHOICE)"),
    ATC_CHOICE('m', "Power Mode", app_pwr_choices, ARR_LEN(app_pwr_choices),
               &app_pwr_mode, commit_pwr_mode),
    ATC_CHOICE('f', "Fan Curve",  app_fan_choices, ARR_LEN(app_fan_choices),
               &app_fan_mode, commit_fan_mode),

    ATC_GROUP (     "Setpoints (INPUT)"),
    ATC_INPUT ('d', "PWM Duty",  "%",  rd_pwm_duty,
               ATC_MENU_INPUT_INT, 0, 100,  commit_pwm_duty),
    ATC_INPUT ('h', "Threshold", "mV", rd_threshold,
               ATC_MENU_INPUT_INT, 0, 5000, commit_threshold),
);

/* -------------------------------------------------------------- home menu */

ATC_MENU(home,
    ATC_GROUP (     "Quick view"),
    ATC_VALUE ('t', "MCU Temp", "C", rd_temp),
    ATC_VALUE ('v', "Battery",  "V", rd_vbat),

    ATC_GROUP_REFRESH ('e', "BME280 Env"),
    ATC_VALUE (0, "Temperature", "C",   rd_bme_t),
    ATC_VALUE (0, "Humidity",    "%",   rd_bme_h),
    ATC_VALUE (0, "Pressure",    "hPa", rd_bme_p),
    ATC_VALUE (0, "Altitude",    "m",   rd_bme_a),

    ATC_GROUP (     "Drill into"),
    ATC_SUBMENU('i', "MPU9250 IMU",    &imu),
    ATC_SUBMENU('p', "INA219 Power",   &power),
    ATC_SUBMENU('w', "Visual Widgets", &widgets),

    ATC_GROUP (     "Control"),
    ATC_STATE ('L', "LED",       rd_led, act_toggle_led),
    ATC_ACTION('1', "Self test", act_self_test),

    ATC_NOTE  ("Press a row's hotkey to interact with it."),
    ATC_NOTE  ("Type ':' for command mode (e.g., set temp 30)."),
);

/* -------------------------------------------------------------- command parser */

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

/* -------------------------------------------------------------- serial port */

static void com_write(const char *buf, size_t len) {
    g_tx_bytes += len;
#ifdef _WIN32
    DWORD written = 0;
    WriteFile(g_serial, buf, (DWORD)len, &written, NULL);
#else
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(g_serial, buf + off, len - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        off += (size_t)n;
    }
#endif
}

static const atc_menu_port_t serial_port = {
    .write = com_write,
    .cmd   = app_cmd,
};

#ifdef _WIN32

static bool serial_open(const char *port, unsigned baud) {
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

static int serial_read_byte(char *out) {
    DWORD nread = 0;
    if (ReadFile(g_serial, out, 1, &nread, NULL) && nread == 1) return 1;
    return 0;
}

#else  /* POSIX */

static speed_t baud_to_speed(unsigned baud) {
    switch (baud) {
    case 9600:   return B9600;
    case 19200:  return B19200;
    case 38400:  return B38400;
    case 57600:  return B57600;
    case 115200: return B115200;
    case 230400: return B230400;
#ifdef B460800
    case 460800: return B460800;
#endif
#ifdef B921600
    case 921600: return B921600;
#endif
    default:     return 0;
    }
}

static bool serial_open(const char *port, unsigned baud) {
    g_serial = open(port, O_RDWR | O_NOCTTY);
    if (g_serial < 0) {
        fprintf(stderr, "open %s failed: %s\n", port, strerror(errno));
        return false;
    }

    struct termios tio;
    if (tcgetattr(g_serial, &tio) != 0) {
        fprintf(stderr, "tcgetattr failed: %s\n", strerror(errno));
        return false;
    }

    cfmakeraw(&tio);
    tio.c_cflag |=  (CLOCAL | CREAD);
    tio.c_cflag &= ~(CSTOPB | PARENB | CSIZE);
    tio.c_cflag |=  CS8;
#ifdef CRTSCTS
    tio.c_cflag &= ~CRTSCTS;
#endif

    speed_t s = baud_to_speed(baud);
    if (s == 0) {
        fprintf(stderr, "unsupported baud: %u\n", baud);
        return false;
    }
    cfsetispeed(&tio, s);
    cfsetospeed(&tio, s);

    /* Non-blocking-ish read: return after up to 100 ms even with no data. */
    tio.c_cc[VMIN]  = 0;
    tio.c_cc[VTIME] = 1;

    if (tcsetattr(g_serial, TCSANOW, &tio) != 0) {
        fprintf(stderr, "tcsetattr failed: %s\n", strerror(errno));
        return false;
    }
    return true;
}

static int serial_read_byte(char *out) {
    ssize_t n = read(g_serial, out, 1);
    if (n == 1) return 1;
    return 0;
}

#endif

int main(int argc, char **argv) {
    if (argc < 2) {
#ifdef _WIN32
        fprintf(stderr, "usage: %s <COMx> [baud]\n", argv[0]);
#else
        fprintf(stderr, "usage: %s <serial-device> [baud]\n", argv[0]);
#endif
        return 1;
    }
    unsigned baud = (argc > 2) ? (unsigned)atoi(argv[2]) : 115200u;
    if (!serial_open(argv[1], baud)) return 1;

    sensor_sim_init();

    atc_menu_init(&home, &serial_port, &demo_info);
    atc_menu_render();
    fprintf(stderr, "initial render: %zu B\n", g_tx_bytes);

    for (;;) {
        char c = 0;

        sensor_sim_tick();
        if (app_load_target_ma >= 0.0f) g_ina.current_ma = app_load_target_ma;

        if (serial_read_byte(&c)) {
            size_t before = g_tx_bytes;
            atc_menu_handle_key(c);
            size_t emitted = g_tx_bytes - before;
            if (emitted) fprintf(stderr, "key %c: %zu B\n", c, emitted);
        }
    }
}
