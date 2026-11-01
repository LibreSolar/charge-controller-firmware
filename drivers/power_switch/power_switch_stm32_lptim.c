/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Power switch with hardware short-circuit protection for STM32L0
 *
 * The switch is driven by the LPTIM1 output while it is on. The comparator COMP2 compares the
 * current measurement with a fraction of VREFINT and triggers LPTIM1 in set-once mode, which
 * switches off the output a few microseconds after a short circuit without any software
 * interaction. The comparator interrupt is used afterwards to report the fault.
 *
 * While the switch is off, the pin is configured as a GPIO output driven inactive.
 *
 * Only a single instance is supported, as LPTIM1 and COMP2 are hard-wired.
 */

#define DT_DRV_COMPAT libresolar_stm32_lptim_power_switch

#include <libresolar/drivers/power_switch.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>

#include <soc.h>
#include <stm32_ll_bus.h>

LOG_MODULE_REGISTER(power_switch_stm32_lptim, CONFIG_POWER_SWITCH_LOG_LEVEL);

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) <= 1,
             "Only a single STM32 LPTIM power switch is supported");

/* 1 = second-highest priority of STM32L0/F0 */
#define COMP_IRQ_PRIORITY 1

struct power_switch_stm32_lptim_config
{
    struct gpio_dt_spec gpio;
    const struct pinctrl_dev_config *pcfg;
};

struct power_switch_stm32_lptim_data
{
    power_switch_callback_t callback;
    void *user_data;
    bool on;
};

static void lptim_init(void)
{
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_LPTIM1);

    /* set prescaler to 32 (resulting in 1 MHz timer frequency) */
    LPTIM1->CFGR |= 0x5U << LPTIM_CFGR_PRESC_Pos;

    /* enable trigger (rising edge) */
    LPTIM1->CFGR |= LPTIM_CFGR_TRIGEN_0;

    /* select trigger 7 (COMP2_OUT) */
    LPTIM1->CFGR |= 0x7U << LPTIM_CFGR_TRIGSEL_Pos;

    LPTIM1->CFGR |= LPTIM_CFGR_PRELOAD;

    /* glitch filter of 8 cycles */
    LPTIM1->CFGR |= LPTIM_CFGR_TRGFLT_0 | LPTIM_CFGR_TRGFLT_1;

    /* enable set-once mode */
    LPTIM1->CFGR |= LPTIM_CFGR_WAVE;

    /* enable timer (must be done *before* changing ARR or CMP, but *after* changing CFGR) */
    LPTIM1->CR |= LPTIM_CR_ENABLE;

    /* auto reload register */
    LPTIM1->ARR = 1000;

    /* switch-off delay in microseconds (actually takes approx. 4 us longer than this setting) */
    LPTIM1->CMP = 10;
}

static void comp_init(void)
{
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);

    /* enable VREFINT buffer */
    SYSCFG->CFGR3 |= SYSCFG_CFGR3_ENBUFLP_VREFINT_COMP;

    /* select PB4 as positive input */
    COMP2->CSR |= COMP_INPUT_PLUS_IO2;

    /* select VREFINT divider as negative input */
    COMP2->CSR |= COMP_INPUT_MINUS_1_4VREFINT;

    /* propagate comparator value to LPTIM input */
    COMP2->CSR |= COMP_CSR_COMP2LPTIM1IN1;

    /* enable COMP2 */
    COMP2->CSR |= COMP_CSR_COMP2EN;

    /* enable EXTI software interrupt / event */
    EXTI->IMR |= EXTI_IMR_IM22;
    EXTI->EMR |= EXTI_EMR_EM22;
    EXTI->RTSR |= EXTI_RTSR_RT22;
    EXTI->FTSR |= EXTI_FTSR_FT22;
    EXTI->SWIER |= EXTI_SWIER_SWI22;
}

static int power_switch_stm32_lptim_set(const struct device *dev, bool on)
{
    const struct power_switch_stm32_lptim_config *config = dev->config;
    struct power_switch_stm32_lptim_data *data = dev->data;
    int err;

    /* back to GPIO mode, which also switches off the output if it was driven by the LPTIM */
    err = gpio_pin_configure_dt(&config->gpio, GPIO_OUTPUT_INACTIVE);
    if (err != 0) {
        return err;
    }

    if (on) {
        lptim_init();

        /* hand over the pin to the LPTIM output */
        err = pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);
        if (err != 0) {
            return err;
        }
    }

    data->on = on;

    return 0;
}

static bool power_switch_stm32_lptim_is_on(const struct device *dev)
{
    struct power_switch_stm32_lptim_data *data = dev->data;

    return data->on;
}

static int power_switch_stm32_lptim_set_callback(const struct device *dev, uint32_t events,
                                                 power_switch_callback_t callback, void *user_data)
{
    struct power_switch_stm32_lptim_data *data = dev->data;

    if (events & ~POWER_SWITCH_EVENT_FAULT) {
        return -ENOTSUP;
    }

    unsigned int key = irq_lock();

    data->callback = callback;
    data->user_data = user_data;

    irq_unlock(key);

    return 0;
}

static void power_switch_stm32_lptim_isr(const struct device *dev)
{
    struct power_switch_stm32_lptim_data *data = dev->data;

    if (COMP2->CSR & COMP_CSR_COMP2VALUE) {
        /*
         * The output was already switched off by the LPTIM trigger. Keep it off permanently
         * by configuring the pin as GPIO again.
         */
        power_switch_stm32_lptim_set(dev, false);

        if (data->callback != NULL) {
            data->callback(dev, POWER_SWITCH_EVENT_FAULT, data->user_data);
        }
    }

    /* clear interrupt flag */
    EXTI->PR = EXTI_PR_PIF22;
}

static DEVICE_API(power_switch, power_switch_stm32_lptim_api) = {
    .set = power_switch_stm32_lptim_set,
    .is_on = power_switch_stm32_lptim_is_on,
    .set_callback = power_switch_stm32_lptim_set_callback,
};

static int power_switch_stm32_lptim_init(const struct device *dev)
{
    const struct power_switch_stm32_lptim_config *config = dev->config;
    int err;

    if (!gpio_is_ready_dt(&config->gpio)) {
        LOG_ERR("GPIO not ready");
        return -ENODEV;
    }

    err = gpio_pin_configure_dt(&config->gpio, GPIO_OUTPUT_INACTIVE);
    if (err != 0) {
        return err;
    }

    comp_init();

    IRQ_CONNECT(ADC1_COMP_IRQn, COMP_IRQ_PRIORITY, power_switch_stm32_lptim_isr,
                DEVICE_DT_INST_GET(0), 0);
    irq_enable(ADC1_COMP_IRQn);

    return 0;
}

#define POWER_SWITCH_STM32_LPTIM_INIT(n) \
    PINCTRL_DT_INST_DEFINE(n); \
\
    static const struct power_switch_stm32_lptim_config power_switch_stm32_lptim_config_##n = { \
        .gpio = GPIO_DT_SPEC_INST_GET(n, gpios), \
        .pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n), \
    }; \
\
    static struct power_switch_stm32_lptim_data power_switch_stm32_lptim_data_##n; \
\
    DEVICE_DT_INST_DEFINE(n, power_switch_stm32_lptim_init, NULL, \
                          &power_switch_stm32_lptim_data_##n, \
                          &power_switch_stm32_lptim_config_##n, POST_KERNEL, \
                          CONFIG_POWER_SWITCH_INIT_PRIORITY, &power_switch_stm32_lptim_api);

DT_INST_FOREACH_STATUS_OKAY(POWER_SWITCH_STM32_LPTIM_INIT)
