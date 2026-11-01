/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Power switch driven by a PWM signal of an STM32 timer
 *
 * Used for PWM solar charge controllers, where the solar panel is connected to the battery via a
 * switch with low frequency PWM.
 *
 * The output channel is always enabled after initialization. Switching on and off is done by
 * changing between PWM mode and forced inactive level, so that the output is actively driven
 * while the switch is off.
 *
 * The timer update interrupt (start of each PWM period) is used to report the rising edges of the
 * PWM signal.
 */

#define DT_DRV_COMPAT libresolar_stm32_pwm_power_switch

#include "stm32_tim.h"

#include <libresolar/drivers/power_switch.h>

#include <zephyr/device.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(power_switch_stm32_pwm, CONFIG_POWER_SWITCH_LOG_LEVEL);

/* 1 = second-highest priority of STM32L0/F0 */
#define TIM_IRQ_PRIORITY 1

struct power_switch_stm32_pwm_config
{
    struct stm32_tim_config tim;
    void (*irq_config_func)(void);
    uint32_t frequency;
    /** Timer channel (1-4) */
    uint8_t channel;
};

struct power_switch_stm32_pwm_data
{
    power_switch_callback_t callback;
    void *user_data;
    uint32_t callback_events;
    /** Number of timer counts per PWM period */
    uint32_t period;
    volatile bool on;
};

static int power_switch_stm32_pwm_set(const struct device *dev, bool on)
{
    const struct power_switch_stm32_pwm_config *config = dev->config;
    struct power_switch_stm32_pwm_data *data = dev->data;

    LL_TIM_OC_SetMode(config->tim.timer, stm32_tim_ll_channel(config->channel),
                      on ? LL_TIM_OCMODE_PWM1 : LL_TIM_OCMODE_FORCED_INACTIVE);

    data->on = on;

    return 0;
}

static bool power_switch_stm32_pwm_is_on(const struct device *dev)
{
    struct power_switch_stm32_pwm_data *data = dev->data;

    return data->on;
}

static bool power_switch_stm32_pwm_is_conducting(const struct device *dev)
{
    const struct power_switch_stm32_pwm_config *config = dev->config;
    struct power_switch_stm32_pwm_data *data = dev->data;
    TIM_TypeDef *tim = config->tim.timer;

    if (!data->on) {
        return false;
    }

    /* PWM mode 1 in up-counting mode: output active as long as counter < compare */
    return LL_TIM_GetCounter(tim) < stm32_tim_get_compare(tim, config->channel);
}

static int power_switch_stm32_pwm_set_duty_cycle(const struct device *dev, float duty)
{
    const struct power_switch_stm32_pwm_config *config = dev->config;
    struct power_switch_stm32_pwm_data *data = dev->data;

    /* round to nearest value, so that small duty cycle steps are not lost */
    stm32_tim_set_compare(config->tim.timer, config->channel,
                          (uint32_t)(duty * data->period + 0.5F));

    return 0;
}

static float power_switch_stm32_pwm_get_duty_cycle(const struct device *dev)
{
    const struct power_switch_stm32_pwm_config *config = dev->config;
    struct power_switch_stm32_pwm_data *data = dev->data;

    return (float)stm32_tim_get_compare(config->tim.timer, config->channel) / data->period;
}

static int power_switch_stm32_pwm_set_callback(const struct device *dev, uint32_t events,
                                               power_switch_callback_t callback, void *user_data)
{
    struct power_switch_stm32_pwm_data *data = dev->data;

    if (events & ~POWER_SWITCH_EVENT_PWM_EDGE) {
        return -ENOTSUP;
    }

    unsigned int key = irq_lock();

    data->callback = callback;
    data->user_data = user_data;
    data->callback_events = events;

    irq_unlock(key);

    return 0;
}

static void power_switch_stm32_pwm_isr(const struct device *dev)
{
    const struct power_switch_stm32_pwm_config *config = dev->config;
    struct power_switch_stm32_pwm_data *data = dev->data;
    TIM_TypeDef *tim = config->tim.timer;

    if (!LL_TIM_IsActiveFlag_UPDATE(tim)) {
        return;
    }

    LL_TIM_ClearFlag_UPDATE(tim);

    if (data->on && data->callback != NULL && (data->callback_events & POWER_SWITCH_EVENT_PWM_EDGE))
    {
        uint32_t compare = stm32_tim_get_compare(tim, config->channel);

        /* no rising edge at 0% or 100% duty cycle */
        if (compare > 0 && compare < data->period) {
            data->callback(dev, POWER_SWITCH_EVENT_PWM_EDGE, data->user_data);
        }
    }
}

static DEVICE_API(power_switch, power_switch_stm32_pwm_api) = {
    .set = power_switch_stm32_pwm_set,
    .is_on = power_switch_stm32_pwm_is_on,
    .is_conducting = power_switch_stm32_pwm_is_conducting,
    .set_duty_cycle = power_switch_stm32_pwm_set_duty_cycle,
    .get_duty_cycle = power_switch_stm32_pwm_get_duty_cycle,
    .set_callback = power_switch_stm32_pwm_set_callback,
};

static int power_switch_stm32_pwm_init(const struct device *dev)
{
    const struct power_switch_stm32_pwm_config *config = dev->config;
    struct power_switch_stm32_pwm_data *data = dev->data;
    TIM_TypeDef *tim = config->tim.timer;
    uint32_t ll_channel = stm32_tim_ll_channel(config->channel);
    uint32_t tim_clk;
    int err;

    err = stm32_tim_init(&config->tim, &tim_clk);
    if (err < 0) {
        return err;
    }

    /* smallest prescaler which allows the period to fit into a 16-bit counter */
    uint32_t clocks_per_period = tim_clk / config->frequency;
    uint32_t prescaler = (clocks_per_period - 1) / 0x10000;

    data->period = clocks_per_period / (prescaler + 1);
    if (data->period < 2) {
        LOG_ERR("Frequency %u Hz too high", config->frequency);
        return -EINVAL;
    }

    LL_TIM_SetPrescaler(tim, prescaler);
    LL_TIM_SetCounterMode(tim, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetAutoReload(tim, data->period - 1);
    LL_TIM_EnableARRPreload(tim);

    LL_TIM_OC_SetMode(tim, ll_channel, LL_TIM_OCMODE_FORCED_INACTIVE);
    LL_TIM_OC_SetPolarity(tim, ll_channel, LL_TIM_OCPOLARITY_HIGH);
    LL_TIM_OC_EnablePreload(tim, ll_channel);
    stm32_tim_set_compare(tim, config->channel, 0);
    LL_TIM_CC_EnableChannel(tim, ll_channel);

#ifdef IS_TIM_BREAK_INSTANCE
    if (IS_TIM_BREAK_INSTANCE(tim)) {
        /* main output enable required for advanced-control timers */
        LL_TIM_EnableAllOutputs(tim);
    }
#endif

    /* load prescaler and preload registers */
    LL_TIM_GenerateEvent_UPDATE(tim);
    LL_TIM_ClearFlag_UPDATE(tim);

    config->irq_config_func();
    LL_TIM_EnableIT_UPDATE(tim);

    LL_TIM_EnableCounter(tim);

    return 0;
}

/* advanced-control timers have a separate update interrupt */
#define TIM_IRQ_BY_IDX(n, cell) \
    COND_CODE_1(DT_IRQ_HAS_NAME(DT_INST_PARENT(n), up), \
                (DT_IRQ_BY_NAME(DT_INST_PARENT(n), up, cell)), \
                (DT_IRQ_BY_NAME(DT_INST_PARENT(n), global, cell)))

#define POWER_SWITCH_STM32_PWM_INIT(n) \
    STM32_TIM_DEFINE(n); \
\
    static void power_switch_stm32_pwm_irq_config_##n(void) \
    { \
        IRQ_CONNECT(TIM_IRQ_BY_IDX(n, irq), TIM_IRQ_PRIORITY, power_switch_stm32_pwm_isr, \
                    DEVICE_DT_INST_GET(n), 0); \
        irq_enable(TIM_IRQ_BY_IDX(n, irq)); \
    } \
\
    static const struct power_switch_stm32_pwm_config power_switch_stm32_pwm_config_##n = { \
        .tim = STM32_TIM_CONFIG(n), \
        .irq_config_func = power_switch_stm32_pwm_irq_config_##n, \
        .frequency = DT_INST_PROP(n, frequency), \
        .channel = DT_INST_PROP(n, channel), \
    }; \
\
    static struct power_switch_stm32_pwm_data power_switch_stm32_pwm_data_##n; \
\
    DEVICE_DT_INST_DEFINE(n, power_switch_stm32_pwm_init, NULL, &power_switch_stm32_pwm_data_##n, \
                          &power_switch_stm32_pwm_config_##n, POST_KERNEL, \
                          CONFIG_POWER_SWITCH_INIT_PRIORITY, &power_switch_stm32_pwm_api);

DT_INST_FOREACH_STATUS_OKAY(POWER_SWITCH_STM32_PWM_INIT)
