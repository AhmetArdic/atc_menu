/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "sensor_sim.h"

#include <stdlib.h>

mcu_t     g_mcu;
ina219_t  g_ina;
bme280_t  g_bme;
mpu9250_t g_mpu;

static float jitter(float amp) {
    float r = (float)rand() / (float)RAND_MAX;
    return (r * 2.0f - 1.0f) * amp;
}

/* Pull v toward target by k, plus noise. */
static float drift(float v, float target, float k, float noise_amp) {
    return v + (target - v) * k + jitter(noise_amp);
}

void sensor_sim_init(void) {
    srand(0xA7C0u);

    g_mcu.mcu_temp_c = 24.5f;
    g_mcu.vbat_v     = 3.71f;

    g_ina.bus_v      = 3.78f;
    g_ina.current_ma = 420.0f;
    g_ina.power_mw   = g_ina.bus_v * g_ina.current_ma;

    g_bme.temp_c       = 24.5f;
    g_bme.humidity     = 42.0f;
    g_bme.pressure_hpa = 1013.2f;
    g_bme.altitude_m   = 0.0f;

    g_mpu.ax = 0.00f; g_mpu.ay = 0.00f; g_mpu.az = 1.00f;
    g_mpu.gx = 0.00f; g_mpu.gy = 0.00f; g_mpu.gz = 0.00f;
    g_mpu.mx = 24.0f; g_mpu.my = -18.0f; g_mpu.mz = 42.0f;
}

void sensor_sim_tick(void) {
    g_mcu.mcu_temp_c = drift(g_mcu.mcu_temp_c, 25.0f, 0.02f, 0.15f);
    g_mcu.vbat_v     = drift(g_mcu.vbat_v,     3.75f, 0.02f, 0.01f);

    g_ina.bus_v      = drift(g_ina.bus_v,      3.80f,  0.05f, 0.02f);
    g_ina.current_ma = drift(g_ina.current_ma, 450.0f, 0.10f, 35.0f);
    if (g_ina.current_ma < 0.0f) g_ina.current_ma = 0.0f;
    g_ina.power_mw   = g_ina.bus_v * g_ina.current_ma;

    g_bme.temp_c       = drift(g_bme.temp_c,       24.0f,   0.02f, 0.10f);
    g_bme.humidity     = drift(g_bme.humidity,     42.0f,   0.02f, 0.40f);
    g_bme.pressure_hpa = drift(g_bme.pressure_hpa, 1013.2f, 0.02f, 0.15f);
    g_bme.altitude_m   = (1013.25f - g_bme.pressure_hpa) * 8.43f;

    g_mpu.ax = drift(g_mpu.ax, 0.00f, 0.20f, 0.01f);
    g_mpu.ay = drift(g_mpu.ay, 0.00f, 0.20f, 0.01f);
    g_mpu.az = drift(g_mpu.az, 1.00f, 0.20f, 0.01f);
    g_mpu.gx = drift(g_mpu.gx, 0.00f, 0.30f, 0.50f);
    g_mpu.gy = drift(g_mpu.gy, 0.00f, 0.30f, 0.50f);
    g_mpu.gz = drift(g_mpu.gz, 0.00f, 0.30f, 0.50f);
    g_mpu.mx = drift(g_mpu.mx, 24.0f, 0.05f, 0.30f);
    g_mpu.my = drift(g_mpu.my, -18.0f, 0.05f, 0.30f);
    g_mpu.mz = drift(g_mpu.mz, 42.0f, 0.05f, 0.30f);
}
