/*
 * Copyright (c) 2020 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HELPER_H
#define HELPER_H

/** @file
 *
 * @brief
 * General helper functions
 */

#include <string.h>
#include <stdint.h>
#include <time.h>

#include <zephyr.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Framework-independent system uptime
 *
 * @returns seconds since the system booted
 */
static inline uint32_t uptime()
{
#ifdef __ZEPHYR__
    return k_uptime_get() / 1000;
#else
    return time(NULL);
#endif
}

/**
 * Sets one or more flags in given field
 *
 * @param field pointer to the field that will be manipulated
 * @param mask a single flag or bitwise OR-ed flags
 */
inline void flags_set(uint32_t *field, uint32_t mask) { *field |= mask; }

/**
 * Clears one or more flags in the bit field
 *
 * @param field pointer to the field that will be manipulated
 * @param mask a single flag or bitwise OR-ed flags
 */
inline void flags_clear(uint32_t *field, uint32_t mask) { *field &= ~mask; }

/**
 * Queries one or more flags in the bit field
 *
 * @param field pointer to the field that will be manipulated
 * @param mask a single flag or bitwise OR-ed
 * @returns true if any of the flags given in mask are set in the bit field
 */
inline bool flags_check(uint32_t *field, uint32_t mask) { return (*field & mask) != 0; }

#ifdef __cplusplus
}
#endif

#endif /* HELPER_H */
