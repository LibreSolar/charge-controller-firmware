/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Half-bridge driver for STM32 advanced-control timers (e.g. TIM1)
 *
 * The high-side switch is driven by output CHx and the low-side switch by the complementary
 * output CHxN. The timer runs in edge-aligned mode and the dead time is generated in hardware.
 *
 * Starting and stopping is done with the main output enable (MOE). While stopped, both outputs
 * are actively driven low.
 */

#define DT_DRV_COMPAT libresolar_stm32_advtim_half_bridge

#include "half_bridge_stm32_tim.h"

#include <libresolar/drivers/half_bridge.h>

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(half_bridge_stm32_advtim, CONFIG_HALF_BRIDGE_LOG_LEVEL);

#ifndef LL_TIM_CHANNEL_CH1N
#error "Advanced-control timers with complementary outputs not supported by this MCU"
#endif

struct half_bridge_stm32_advtim_config
{
    struct half_bridge_stm32_tim_config tim;
    uint32_t frequency;
    uint32_t deadtime_ns;
    /** Timer channel (1-4) for the high-side switch, CHxN is used for the low-side switch */
    uint8_t channel;
    bool adc_trigger;
};

struct half_bridge_stm32_advtim_data
{
    uint32_t compare_min;
    uint32_t compare_max;
    bool low_side_forced;
};

static const uint32_t ch2ll_n[] = {
    LL_TIM_CHANNEL_CH1N,
    LL_TIM_CHANNEL_CH2N,
    LL_TIM_CHANNEL_CH3N,
#ifdef LL_TIM_CHANNEL_CH4N
    LL_TIM_CHANNEL_CH4N,
#endif
};

static int half_bridge_stm32_advtim_start(const struct device *dev)
{
    const struct half_bridge_stm32_advtim_config *config = dev->config;
    struct half_bridge_stm32_advtim_data *data = dev->data;
    TIM_TypeDef *tim = config->tim.timer;

    if (data->low_side_forced) {
        return -EBUSY;
    }

    if (half_bridge_stm32_tim_get_compare(tim, config->channel) == 0U) {
        return -EINVAL;
    }

    /* main output enable */
    LL_TIM_EnableAllOutputs(tim);

    return 0;
}

static int half_bridge_stm32_advtim_stop(const struct device *dev)
{
    const struct half_bridge_stm32_advtim_config *config = dev->config;
    struct half_bridge_stm32_advtim_data *data = dev->data;

    LL_TIM_DisableAllOutputs(config->tim.timer);
    data->low_side_forced = false;

    return 0;
}

static bool half_bridge_stm32_advtim_is_enabled(const struct device *dev)
{
    const struct half_bridge_stm32_advtim_config *config = dev->config;

    return LL_TIM_IsEnabledAllOutputs(config->tim.timer);
}

static int half_bridge_stm32_advtim_set_compare(const struct device *dev, uint32_t compare)
{
    const struct half_bridge_stm32_advtim_config *config = dev->config;
    struct half_bridge_stm32_advtim_data *data = dev->data;
    TIM_TypeDef *tim = config->tim.timer;

    if (data->low_side_forced) {
        return -EBUSY;
    }

    /* protection against wrong settings which could destroy the hardware */
    compare = CLAMP(compare, data->compare_min, data->compare_max);

    half_bridge_stm32_tim_set_compare(tim, config->channel, compare);

#ifdef LL_TIM_CHANNEL_CH6
    if (config->adc_trigger) {
        /* trigger ADC for current measurement in the middle of the on-time */
        LL_TIM_OC_SetCompareCH6(tim, compare / 2);
    }
#endif

    return 0;
}

static uint32_t half_bridge_stm32_advtim_get_compare(const struct device *dev)
{
    const struct half_bridge_stm32_advtim_config *config = dev->config;

    return half_bridge_stm32_tim_get_compare(config->tim.timer, config->channel);
}

static uint32_t half_bridge_stm32_advtim_get_period(const struct device *dev)
{
    const struct half_bridge_stm32_advtim_config *config = dev->config;

    return LL_TIM_GetAutoReload(config->tim.timer);
}

static int half_bridge_stm32_advtim_set_compare_limits(const struct device *dev, uint32_t min,
                                                       uint32_t max)
{
    struct half_bridge_stm32_advtim_data *data = dev->data;

    if (data->low_side_forced) {
        return -EBUSY;
    }

    if (min > max || max > half_bridge_stm32_advtim_get_period(dev)) {
        return -EINVAL;
    }

    data->compare_min = min;
    data->compare_max = max;

    return half_bridge_stm32_advtim_set_compare(dev, half_bridge_stm32_advtim_get_compare(dev));
}

static int half_bridge_stm32_advtim_force_low_side(const struct device *dev)
{
    const struct half_bridge_stm32_advtim_config *config = dev->config;
    struct half_bridge_stm32_advtim_data *data = dev->data;
    TIM_TypeDef *tim = config->tim.timer;

    data->low_side_forced = true;

    /*
     * A compare value of 0 keeps OCxREF low in PWM mode 1, so CHx (high side) stays off and the
     * complementary output CHxN (low side) stays on. The output mode itself can't be changed, as
     * it is locked after initialization.
     */
    half_bridge_stm32_tim_set_compare(tim, config->channel, 0);

    /* load preload registers immediately */
    LL_TIM_GenerateEvent_UPDATE(tim);

    LL_TIM_EnableAllOutputs(tim);

    return 0;
}

static DEVICE_API(half_bridge, half_bridge_stm32_advtim_api) = {
    .start = half_bridge_stm32_advtim_start,
    .stop = half_bridge_stm32_advtim_stop,
    .is_enabled = half_bridge_stm32_advtim_is_enabled,
    .set_compare = half_bridge_stm32_advtim_set_compare,
    .get_compare = half_bridge_stm32_advtim_get_compare,
    .get_period = half_bridge_stm32_advtim_get_period,
    .set_compare_limits = half_bridge_stm32_advtim_set_compare_limits,
    .force_low_side = half_bridge_stm32_advtim_force_low_side,
};

static int half_bridge_stm32_advtim_init_adc_trigger(TIM_TypeDef *tim)
{
#ifdef LL_TIM_CHANNEL_CH6
    if (!IS_TIM_TRGO2_INSTANCE(tim)) {
        LOG_ERR("ADC trigger not supported by this timer");
        return -ENOTSUP;
    }

    /* use OC6 for ADC triggering, but don't configure any pin for PWM mode */
    LL_TIM_OC_SetMode(tim, LL_TIM_CHANNEL_CH6, LL_TIM_OCMODE_PWM1);
    LL_TIM_OC_EnablePreload(tim, LL_TIM_CHANNEL_CH6);
    LL_TIM_CC_EnableChannel(tim, LL_TIM_CHANNEL_CH6);

    /* trigger ADC via TRGO2 on OC6REF falling edge (OC4REF is not used) */
    LL_TIM_SetTriggerOutput2(tim, LL_TIM_TRGO2_OC4_RISING_OC6_FALLING);

    return 0;
#else
    LOG_ERR("ADC trigger not supported by this timer");
    return -ENOTSUP;
#endif
}

static int half_bridge_stm32_advtim_init(const struct device *dev)
{
    const struct half_bridge_stm32_advtim_config *config = dev->config;
    struct half_bridge_stm32_advtim_data *data = dev->data;
    TIM_TypeDef *tim = config->tim.timer;
    uint32_t ll_channel = half_bridge_stm32_tim_ll_channel(config->channel);
    uint32_t tim_clk;
    int err;

    if (!IS_TIM_BREAK_INSTANCE(tim) || config->channel > ARRAY_SIZE(ch2ll_n)) {
        LOG_ERR("Complementary output not supported for channel %u", config->channel);
        return -ENOTSUP;
    }

    err = half_bridge_stm32_tim_init(&config->tim, &tim_clk);
    if (err < 0) {
        return err;
    }

    /* edge-aligned mode to increase resolution (no division by 2 necessary) */
    LL_TIM_SetCounterMode(tim, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetAutoReload(tim, tim_clk / config->frequency);

    /*
     * Drive both outputs to their idle level (low, as OISx and OISxN are 0 after reset) while the
     * main output is disabled, instead of leaving them floating.
     */
    LL_TIM_SetOffStates(tim, LL_TIM_OSSI_ENABLE, LL_TIM_OSSR_DISABLE);

    /* PWM mode 1 with preload and active high polarity for both outputs */
    LL_TIM_OC_SetMode(tim, ll_channel, LL_TIM_OCMODE_PWM1);
    LL_TIM_OC_EnablePreload(tim, ll_channel);
    LL_TIM_CC_EnableChannel(tim, ll_channel | ch2ll_n[config->channel - 1]);

    if (config->adc_trigger) {
        err = half_bridge_stm32_advtim_init_adc_trigger(tim);
        if (err < 0) {
            return err;
        }
    }

    LL_TIM_OC_SetDeadTime(
        tim, __LL_TIM_CALC_DEADTIME(tim_clk, LL_TIM_CLOCKDIVISION_DIV1, config->deadtime_ns));

    /* lock break and dead-time register to prevent accidental changes */
    LL_TIM_CC_SetLockLevel(tim, LL_TIM_LOCKLEVEL_3);

    data->compare_min = 0;
    data->compare_max = LL_TIM_GetAutoReload(tim);

    /* force update generation to load the preload registers and start the counter */
    LL_TIM_GenerateEvent_UPDATE(tim);
    LL_TIM_EnableCounter(tim);

    return 0;
}

#define HALF_BRIDGE_STM32_ADVTIM_INIT(n) \
    HALF_BRIDGE_STM32_TIM_DEFINE(n) \
\
    static const struct half_bridge_stm32_advtim_config half_bridge_stm32_advtim_config_##n = { \
        .tim = HALF_BRIDGE_STM32_TIM_CONFIG(n), \
        .frequency = DT_INST_PROP(n, frequency), \
        .deadtime_ns = DT_INST_PROP(n, deadtime_ns), \
        .channel = DT_INST_PROP(n, channel), \
        .adc_trigger = DT_INST_PROP(n, adc_trigger), \
    }; \
\
    static struct half_bridge_stm32_advtim_data half_bridge_stm32_advtim_data_##n; \
\
    DEVICE_DT_INST_DEFINE(n, half_bridge_stm32_advtim_init, NULL, \
                          &half_bridge_stm32_advtim_data_##n, \
                          &half_bridge_stm32_advtim_config_##n, POST_KERNEL, \
                          CONFIG_HALF_BRIDGE_INIT_PRIORITY, &half_bridge_stm32_advtim_api);

DT_INST_FOREACH_STATUS_OKAY(HALF_BRIDGE_STM32_ADVTIM_INIT)
