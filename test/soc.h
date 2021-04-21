/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 * @brief Mock some functions from STM32 LL API or MCU registers
 */

#include <stdint.h>

static inline uint32_t LL_GetFlashSize(void)
{
    return 128;
}

#define FLASH_PAGE_SIZE 2048
