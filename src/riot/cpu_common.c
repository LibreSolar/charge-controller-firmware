/* RIOT/cpu/stm32/cpu_common.c */
/*
 * Copyright (C) 2016 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_stm32
 * @{
 *
 * @file
 * @brief       Shared CPU specific function for the STM32 CPU family
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include "periph_cpu.h"
//~ #include "periph_conf.h"

#define CLOCK_APB1 (CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC / CONFIG_CLOCK_STM32_APB1_PRESCALER)

uint32_t periph_apb_clk(uint8_t bus)
{
#ifdef CLOCK_APB2
    if (bus == APB2) {
        return CLOCK_APB2;
    }
#else
    (void)bus;
#endif
    return CLOCK_APB1;
}

void periph_clk_en(bus_t bus, uint32_t mask)
{
    switch (bus) {
        case APB1:
            RCC->APB1ENR |= mask;
            break;
        case APB2:
            RCC->APB2ENR |= mask;
            break;
        case AHB:
            RCC->AHBENR |= mask;
            break;
        default:
            //~ DEBUG("unsupported bus %d\n", (int)bus);
            break;
    }
    /* stm32xx-errata: Delay after a RCC peripheral clock enable */
    __DSB();
}
