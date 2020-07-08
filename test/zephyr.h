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

#ifndef __deprecated
#define __deprecated	__attribute__((deprecated))
#endif
#ifndef __weak
#define __weak __attribute__((__weak__))
#endif
