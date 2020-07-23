/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2020 Martin Jäger / Libre Solar
 */

/**
 * @file
 *
 * @brief Mock necessary functions and generated Devicetree or Kconfig defines from Zephyr
 *        for unit tests
 */

#include "devicetree.h"
#include "devicetree_unfixed.h"
#include "autoconf.h"

#ifndef __deprecated
#define __deprecated	__attribute__((deprecated))
#endif
#ifndef __weak
#define __weak __attribute__((__weak__))
#endif

#define printk printf

#define CONFIG_LOG_DEFAULT_LEVEL 2      // errors and warnings only

#define LOG_ERR(fmt, ...) \
    if (CONFIG_LOG_DEFAULT_LEVEL & (1 << 0)) { printf(fmt, ##__VA_ARGS__); printf("\n"); }

#define LOG_WRN(fmt, ...) \
    if (CONFIG_LOG_DEFAULT_LEVEL & (1 << 1)) { printf(fmt, ##__VA_ARGS__); printf("\n"); }

#define LOG_INF(fmt, ...) \
    if (CONFIG_LOG_DEFAULT_LEVEL & (1 << 2)) { printf(fmt, ##__VA_ARGS__); printf("\n"); }

#define LOG_DBG(fmt, ...) \
    if (CONFIG_LOG_DEFAULT_LEVEL & (1 << 3)) { printf(fmt, ##__VA_ARGS__); printf("\n"); }

// dummy
#define LOG_MODULE_REGISTER(a, b)
