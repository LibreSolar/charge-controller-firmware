/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MCU_H_
#define MCU_H_

#ifdef CONFIG_SOC_STM32F072XB
#include "stm32f072xb.h"
#endif

#ifdef CONFIG_SOC_STM32L072XX
#include "stm32l072xx.h"
#endif

#if defined(CONFIG_SOC_STM32G431XX)
#include "stm32g431xx.h"
#elif defined(CONFIG_SOC_STM32G474XX)
#include "stm32g474xx.h"
#endif

#endif // MCU_H_
