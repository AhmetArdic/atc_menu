/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file log_ring.h
 * @brief Fixed-size, allocation-free line ring buffer.
 *
 * Storage is caller-owned: pass a 2-D buffer (rows x cols) plus its
 * dimensions to atc_menu_log_init(). Each push copies one line into the
 * next slot, wrapping when full. The fullscreen log view (triggered by
 * an ATC_ROW_LOG_VIEW row) renders the most recent lines from this ring.
 */

#ifndef ATC_MENU_LOG_RING_H
#define ATC_MENU_LOG_RING_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct atc_log_ring {
    char    *buf;    /**< Caller buffer of size rows * cols bytes. */
    uint16_t rows;   /**< Number of slots (each holds one line). */
    uint16_t cols;   /**< Slot width in bytes incl. terminating NUL. */
    uint16_t head;   /**< Next slot to be written (= one past newest). */
    uint16_t count;  /**< Number of slots currently holding data. */
    uint32_t seq;    /**< Monotonic push counter; render uses it to detect change. */
} atc_log_ring_t;

/**
 * @brief Initialise a ring over a caller-provided buffer.
 *
 * @param r     Ring header.
 * @param buf   Backing storage, sized rows * cols bytes.
 * @param rows  Number of slots.
 * @param cols  Bytes per slot (including the trailing NUL).
 */
void atc_menu_log_init(atc_log_ring_t *r, char *buf, uint16_t rows, uint16_t cols);

/**
 * @brief Append a line, truncating to fit cols-1 characters.
 *
 * NULL lines are stored as empty strings. Overwrites the oldest slot
 * when full.
 */
void atc_menu_log_push(atc_log_ring_t *r, const char *line);

/**
 * @brief Append a printf-formatted line.
 * @return Number of characters that would have been written if cols had
 *         been unlimited, clamped to cols-1 on truncation; <0 on error.
 */
int atc_menu_log_printf(atc_log_ring_t *r, const char *fmt, ...);

/**
 * @brief Read a line counted back from the most recent.
 * @param r            Ring.
 * @param from_newest  0 = most recent, 1 = next older, etc.
 * @return Pointer into the ring's storage, or NULL if the index is out
 *         of the populated range.
 */
const char *atc_menu_log_get(const atc_log_ring_t *r, uint16_t from_newest);

#ifdef __cplusplus
}
#endif

#endif /* ATC_MENU_LOG_RING_H */
