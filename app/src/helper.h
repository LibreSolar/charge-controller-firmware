/*
 * Copyright (c) The Libre Solar Project Contributors
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

#include <stdint.h>
#include <string.h>
#include <time.h>

#ifdef __INTELLISENSE__
/*
 * VS Code intellisense can't cope with all the Zephyr macro layers for logging, so provide it
 * with something more simple and make it silent.
 */

#define LOG_DBG(...) printf(__VA_ARGS__)

#define LOG_INF(...) printf(__VA_ARGS__)

#define LOG_WRN(...) printf(__VA_ARGS__)

#define LOG_ERR(...) printf(__VA_ARGS__)

#define LOG_MODULE_REGISTER(...)

#else

#include <zephyr/logging/log.h>

#endif /* VSCODE_INTELLISENSE_HACK */

#include <zephyr/kernel.h>

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
#ifndef UNIT_TEST
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
inline void flags_set(uint32_t *field, uint32_t mask)
{
    *field |= mask;
}

/**
 * Clears one or more flags in the bit field
 *
 * @param field pointer to the field that will be manipulated
 * @param mask a single flag or bitwise OR-ed flags
 */
inline void flags_clear(uint32_t *field, uint32_t mask)
{
    *field &= ~mask;
}

/**
 * Queries one or more flags in the bit field
 *
 * @param field pointer to the field that will be manipulated
 * @param mask a single flag or bitwise OR-ed
 * @returns true if any of the flags given in mask are set in the bit field
 */
inline bool flags_check(uint32_t *field, uint32_t mask)
{
    return (*field & mask) != 0;
}

#ifdef __cplusplus
}
#endif

#endif /* HELPER_H */
