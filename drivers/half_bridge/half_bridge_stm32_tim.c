/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "half_bridge_stm32_tim.h"

#include <zephyr/drivers/clock_control.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(half_bridge_stm32_tim, CONFIG_HALF_BRIDGE_LOG_LEVEL);

const uint32_t half_bridge_stm32_tim_ll_channels[4] = {
    LL_TIM_CHANNEL_CH1,
    LL_TIM_CHANNEL_CH2,
    LL_TIM_CHANNEL_CH3,
    LL_TIM_CHANNEL_CH4,
};

int half_bridge_stm32_tim_init(const struct half_bridge_stm32_tim_config *config, uint32_t *tim_clk)
{
    const struct device *clk = DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE);
    int err;

    if (!device_is_ready(clk)) {
        LOG_ERR("Clock control device not ready");
        return -ENODEV;
    }

    err = clock_control_on(clk, (clock_control_subsys_t)&config->pclken[0]);
    if (err < 0) {
        LOG_ERR("Could not initialize clock (%d)", err);
        return err;
    }

    if (config->pclk_len < 2) {
        LOG_ERR("Timer clock source is not specified");
        return -EINVAL;
    }

    err = clock_control_configure(clk, (clock_control_subsys_t)&config->pclken[1], NULL);
    if (err < 0) {
        LOG_ERR("Could not configure clock (%d)", err);
        return err;
    }

    err = clock_control_get_rate(clk, (clock_control_subsys_t)&config->pclken[1], tim_clk);
    if (err < 0) {
        LOG_ERR("Timer clock rate get error (%d)", err);
        return err;
    }

    /* reset timer to default state using RCC */
    (void)reset_line_toggle_dt(&config->reset);

    err = pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);
    if (err < 0) {
        LOG_ERR("Pinctrl setup failed (%d)", err);
        return err;
    }

    /* no prescaler --> counter frequency == timer clock */
    LL_TIM_SetPrescaler(config->timer, 0);

    return 0;
}

void half_bridge_stm32_tim_set_compare(TIM_TypeDef *tim, uint8_t channel, uint32_t value)
{
    switch (channel) {
        case 1:
            LL_TIM_OC_SetCompareCH1(tim, value);
            break;
        case 2:
            LL_TIM_OC_SetCompareCH2(tim, value);
            break;
        case 3:
            LL_TIM_OC_SetCompareCH3(tim, value);
            break;
        case 4:
            LL_TIM_OC_SetCompareCH4(tim, value);
            break;
    }
}

uint32_t half_bridge_stm32_tim_get_compare(TIM_TypeDef *tim, uint8_t channel)
{
    switch (channel) {
        case 1:
            return LL_TIM_OC_GetCompareCH1(tim);
        case 2:
            return LL_TIM_OC_GetCompareCH2(tim);
        case 3:
            return LL_TIM_OC_GetCompareCH3(tim);
        case 4:
            return LL_TIM_OC_GetCompareCH4(tim);
        default:
            return 0;
    }
}
