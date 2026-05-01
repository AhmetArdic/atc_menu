/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file sensor_helpers.h
 * @brief Demo-side helpers to cut boilerplate when registering many
 *        value rows backed by sensor structs. Not part of the public
 *        atc_menu API; lives only under examples/.
 *
 * Pattern: one sensor → one struct + one tick() → many READ_F rows.
 */

#ifndef DEMO_SENSOR_HELPERS_H
#define DEMO_SENSOR_HELPERS_H

#include "atc_menu/atc_menu.h"
#include <stdio.h>

/**
 * @brief Generate a `static void rd_<name>(...)` callback that snprintf's
 *        @p expr with @p fmt and assigns @p status_expr to @c *st.
 *
 * Use at file scope, before the menu table.
 *
 *   READ_F(ina_v, g_ina.bus_v, "%.2f",
 *          st_range(g_ina.bus_v, 3.0f, 4.2f))
 */
#define READ_F(name, expr, fmt, status_expr)                      \
    static void rd_##name(char *b, size_t n, atc_status_t *st) {  \
        snprintf(b, n, fmt, (double)(expr));                      \
        *st = (status_expr);                                      \
    }

/** OK if @p v is in [lo,hi], else WARN. */
static inline atc_status_t st_range(float v, float lo, float hi) {
    return (v >= lo && v <= hi) ? ATC_ST_OK : ATC_ST_WARN;
}

/** OK below @p warn, WARN below @p err, ERR otherwise. */
static inline atc_status_t st_max(float v, float warn, float err) {
    if (v >= err)  return ATC_ST_ERR;
    if (v >= warn) return ATC_ST_WARN;
    return ATC_ST_OK;
}

/** OK above @p warn, WARN above @p err, ERR otherwise. */
static inline atc_status_t st_min(float v, float warn, float err) {
    if (v <= err)  return ATC_ST_ERR;
    if (v <= warn) return ATC_ST_WARN;
    return ATC_ST_OK;
}

#endif /* DEMO_SENSOR_HELPERS_H */
