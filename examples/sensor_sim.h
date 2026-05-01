/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sensor_sim.h
 * @brief Demo-side fake sensor backends: state structs + tick().
 *
 * In a real firmware these structs would be filled by I2C/SPI drivers.
 * Here sensor_sim_tick() just walks the values around plausible ranges
 * so the menu has something to show.
 */

#ifndef ATC_DEMO_SENSOR_SIM_H
#define ATC_DEMO_SENSOR_SIM_H

/** Single-output sensors (TMP102-like temp + battery monitor). */
typedef struct {
    float mcu_temp_c;
    float vbat_v;
} mcu_t;

/** INA219 power monitor: bus voltage (V), current (mA), power (mW). */
typedef struct {
    float bus_v;
    float current_ma;
    float power_mw;
} ina219_t;

/** BME280 environmental: temp (C), humidity (%RH), pressure (hPa), altitude (m). */
typedef struct {
    float temp_c;
    float humidity;
    float pressure_hpa;
    float altitude_m;
} bme280_t;

/** MPU9250 9-DoF IMU: accel (g), gyro (dps), mag (uT) on 3 axes each. */
typedef struct {
    float ax, ay, az;
    float gx, gy, gz;
    float mx, my, mz;
} mpu9250_t;

extern mcu_t     g_mcu;
extern ina219_t  g_ina;
extern bme280_t  g_bme;
extern mpu9250_t g_mpu;

/** Seed RNG and put each sensor at a sensible starting point. */
void sensor_sim_init(void);

/** Advance the simulation one step. Call from the main loop. */
void sensor_sim_tick(void);

#endif /* ATC_DEMO_SENSOR_SIM_H */
