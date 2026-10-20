/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Common functions for half-bridge drivers using STM32 timers (TIM)
 */

#ifndef HALF_BRIDGE_STM32_TIM_H_
#define HALF_BRIDGE_STM32_TIM_H_

#include <zephyr/device.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/reset.h>

#include <soc.h>
#include <stm32_ll_tim.h>

/**
 * Timer configuration obtained from the parent st,stm32-timers node
 */
struct half_bridge_stm32_tim_config
{
    TIM_TypeDef *timer;
    const struct stm32_pclken *pclken;
    size_t pclk_len;
    struct reset_dt_spec reset;
    const struct pinctrl_dev_config *pcfg;
};

/**
 * Define variables required by HALF_BRIDGE_STM32_TIM_CONFIG for instance n
 */
#define HALF_BRIDGE_STM32_TIM_DEFINE(n) \
    BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_INST_PARENT(n), st_stm32_timers), \
                 "Half bridge must be a child of an st,stm32-timers node"); \
\
    PINCTRL_DT_INST_DEFINE(n); \
\
    static const struct stm32_pclken half_bridge_stm32_tim_pclken_##n[] = \
        STM32_DT_CLOCKS(DT_INST_PARENT(n));

/**
 * Initializer for struct half_bridge_stm32_tim_config of instance n
 */
#define HALF_BRIDGE_STM32_TIM_CONFIG(n) \
    { \
        .timer = (TIM_TypeDef *)DT_REG_ADDR(DT_INST_PARENT(n)), \
        .pclken = half_bridge_stm32_tim_pclken_##n, \
        .pclk_len = DT_NUM_CLOCKS(DT_INST_PARENT(n)), \
        .reset = RESET_DT_SPEC_GET(DT_INST_PARENT(n)), \
        .pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n), \
    }

/** LL channel definitions for channels 1 to 4 */
extern const uint32_t half_bridge_stm32_tim_ll_channels[4];

/**
 * Get LL channel definition
 *
 * @param channel Timer channel (1-4)
 */
static inline uint32_t half_bridge_stm32_tim_ll_channel(uint8_t channel)
{
    return half_bridge_stm32_tim_ll_channels[channel - 1];
}

/**
 * Enable clock, reset the timer and apply pin configuration
 *
 * @param config Timer configuration
 * @param tim_clk Pointer to store timer clock frequency in Hz
 *
 * @retval 0 on success
 * @retval -errno otherwise
 */
int half_bridge_stm32_tim_init(const struct half_bridge_stm32_tim_config *config,
                               uint32_t *tim_clk);

/**
 * Set compare value of a timer channel
 *
 * @param tim Timer instance
 * @param channel Timer channel (1-4)
 * @param value Compare value
 */
void half_bridge_stm32_tim_set_compare(TIM_TypeDef *tim, uint8_t channel, uint32_t value);

/**
 * Get compare value of a timer channel
 *
 * @param tim Timer instance
 * @param channel Timer channel (1-4)
 *
 * @returns Compare value
 */
uint32_t half_bridge_stm32_tim_get_compare(TIM_TypeDef *tim, uint8_t channel);

#endif /* HALF_BRIDGE_STM32_TIM_H_ */
