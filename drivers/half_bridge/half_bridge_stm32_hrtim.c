/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Half-bridge driver for the STM32 high-resolution timer (HRTIM)
 *
 * This initial implementation only supports timer A with outputs TA1 (high-side) and TA2
 * (low-side) and does not make use of the possible higher resolution yet.
 */

#define DT_DRV_COMPAT libresolar_stm32_hrtim_half_bridge

#include <libresolar/drivers/half_bridge.h>

#include <zephyr/device.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/reset.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <soc.h>

LOG_MODULE_REGISTER(half_bridge_stm32_hrtim, CONFIG_HALF_BRIDGE_LOG_LEVEL);

/*
 * A negative offset of the ADC trigger was found to improve current measurement accuracy and
 * compensate the ADC delay (no risk of underflow as compare value is around 214 even at 0.1%
 * duty cycle).
 */
#define ADC_TRIGGER_OFFSET 80

/* Timeout for delay-locked loop (DLL) calibration */
#define DLL_CALIBRATION_TIMEOUT_US 10000

struct half_bridge_stm32_hrtim_config
{
    HRTIM_TypeDef *hrtim;
    const struct stm32_pclken *pclken;
    size_t pclk_len;
    struct reset_dt_spec reset;
    const struct pinctrl_dev_config *pcfg;
    uint32_t frequency;
    uint32_t deadtime_ns;
    bool adc_trigger;
};

struct half_bridge_stm32_hrtim_data
{
    uint32_t compare_min;
    uint32_t compare_max;
    bool low_side_forced;
};

static inline HRTIM_Timerx_TypeDef *timer_a(const struct device *dev)
{
    const struct half_bridge_stm32_hrtim_config *config = dev->config;

    return &config->hrtim->sTimerxRegs[0];
}

static int half_bridge_stm32_hrtim_start(const struct device *dev)
{
    const struct half_bridge_stm32_hrtim_config *config = dev->config;
    struct half_bridge_stm32_hrtim_data *data = dev->data;

    if (data->low_side_forced) {
        return -EBUSY;
    }

    config->hrtim->sCommonRegs.OENR = HRTIM_OENR_TA1OEN | HRTIM_OENR_TA2OEN;

    return 0;
}

static int half_bridge_stm32_hrtim_stop(const struct device *dev)
{
    const struct half_bridge_stm32_hrtim_config *config = dev->config;
    struct half_bridge_stm32_hrtim_data *data = dev->data;

    config->hrtim->sCommonRegs.ODISR = HRTIM_ODISR_TA1ODIS | HRTIM_ODISR_TA2ODIS;

    if (data->low_side_forced) {
        /* restore PWM generation for output 1 */
        HRTIM_Timerx_TypeDef *tim = timer_a(dev);
        tim->SETx1R = HRTIM_SET1R_PER;
        tim->RSTx1R = HRTIM_RST1R_CMP1;
        data->low_side_forced = false;
    }

    return 0;
}

static bool half_bridge_stm32_hrtim_is_enabled(const struct device *dev)
{
    const struct half_bridge_stm32_hrtim_config *config = dev->config;

    return (config->hrtim->sCommonRegs.OENR & (HRTIM_OENR_TA1OEN | HRTIM_OENR_TA2OEN)) > 0;
}

static int half_bridge_stm32_hrtim_set_compare(const struct device *dev, uint32_t compare)
{
    const struct half_bridge_stm32_hrtim_config *config = dev->config;
    struct half_bridge_stm32_hrtim_data *data = dev->data;
    HRTIM_Timerx_TypeDef *tim = timer_a(dev);

    if (data->low_side_forced) {
        return -EBUSY;
    }

    /* protection against wrong settings which could destroy the hardware */
    compare = CLAMP(compare, data->compare_min, data->compare_max);

    tim->CMP1xR = compare;

    if (config->adc_trigger) {
        /* trigger ADC for current measurement in the middle of the on-time */
        tim->CMP3xR = compare / 2 - ADC_TRIGGER_OFFSET;
    }

    return 0;
}

static uint32_t half_bridge_stm32_hrtim_get_compare(const struct device *dev)
{
    return timer_a(dev)->CMP1xR;
}

static uint32_t half_bridge_stm32_hrtim_get_period(const struct device *dev)
{
    return timer_a(dev)->PERxR;
}

static int half_bridge_stm32_hrtim_set_compare_limits(const struct device *dev, uint32_t min,
                                                      uint32_t max)
{
    struct half_bridge_stm32_hrtim_data *data = dev->data;

    if (data->low_side_forced) {
        return -EBUSY;
    }

    if (min > max || max > half_bridge_stm32_hrtim_get_period(dev)) {
        return -EINVAL;
    }

    data->compare_min = min;
    data->compare_max = max;

    return half_bridge_stm32_hrtim_set_compare(dev, half_bridge_stm32_hrtim_get_compare(dev));
}

static int half_bridge_stm32_hrtim_force_low_side(const struct device *dev)
{
    const struct half_bridge_stm32_hrtim_config *config = dev->config;
    struct half_bridge_stm32_hrtim_data *data = dev->data;
    HRTIM_Timerx_TypeDef *tim = timer_a(dev);

    data->low_side_forced = true;

    /*
     * Remove all set events and reset output 1 (high side) by software, so that it stays
     * inactive. Output 2 (low side) is generated as the complement with dead time and stays on.
     */
    tim->SETx1R = 0;
    tim->RSTx1R = HRTIM_RST1R_SRT;

    config->hrtim->sCommonRegs.OENR = HRTIM_OENR_TA1OEN | HRTIM_OENR_TA2OEN;

    return 0;
}

static DEVICE_API(half_bridge, half_bridge_stm32_hrtim_api) = {
    .start = half_bridge_stm32_hrtim_start,
    .stop = half_bridge_stm32_hrtim_stop,
    .is_enabled = half_bridge_stm32_hrtim_is_enabled,
    .set_compare = half_bridge_stm32_hrtim_set_compare,
    .get_compare = half_bridge_stm32_hrtim_get_compare,
    .get_period = half_bridge_stm32_hrtim_get_period,
    .set_compare_limits = half_bridge_stm32_hrtim_set_compare_limits,
    .force_low_side = half_bridge_stm32_hrtim_force_low_side,
};

static int half_bridge_stm32_hrtim_init(const struct device *dev)
{
    const struct half_bridge_stm32_hrtim_config *config = dev->config;
    struct half_bridge_stm32_hrtim_data *data = dev->data;
    const struct device *clk = DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE);
    HRTIM_TypeDef *hrtim = config->hrtim;
    HRTIM_Timerx_TypeDef *tim = timer_a(dev);
    uint32_t tim_clk;
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

    err = clock_control_get_rate(clk, (clock_control_subsys_t)&config->pclken[1], &tim_clk);
    if (err < 0) {
        LOG_ERR("Timer clock rate get error (%d)", err);
        return err;
    }

    if (config->reset.dev != NULL) {
        (void)reset_line_toggle_dt(&config->reset);
    }

    err = pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);
    if (err < 0) {
        LOG_ERR("Pinctrl setup failed (%d)", err);
        return err;
    }

    /* enable periodic calibration of delay-locked loop (DLL) with lowest calibration period */
    hrtim->sCommonRegs.DLLCR |= HRTIM_DLLCR_CALEN | HRTIM_DLLCR_CALRTE_0 | HRTIM_DLLCR_CALRTE_1;

    if (!WAIT_FOR((hrtim->sCommonRegs.ISR & HRTIM_ISR_DLLRDY) != 0, DLL_CALIBRATION_TIMEOUT_US,
                  k_busy_wait(1)))
    {
        LOG_ERR("DLL calibration timed out");
        return -ETIMEDOUT;
    }

    /* prescaler 32 --> counter frequency == timer clock (same as normal timers like TIM1) */
    tim->TIMxCR |= (5U << HRTIM_TIMCR_CK_PSC_Pos);

    /* dead time prescaler 8 / 2^3 --> same frequency as counter */
    tim->DTxR |= (3U << HRTIM_DTR_DTPRSC_Pos);

    /* continuous mode operation */
    tim->TIMxCR |= HRTIM_TIMCR_CONT;

    tim->PERxR = tim_clk / config->frequency + 1;

    /* output 1 set at period and reset at compare 1, output 2 complementary with dead time */
    tim->SETx1R = HRTIM_SET1R_PER;
    tim->RSTx1R = HRTIM_RST1R_CMP1;
    tim->OUTxR = HRTIM_OUTR_DTEN;

    /* set dead time values and lock dead time signs */
    uint32_t deadtime_clocks = ((tim_clk / 1000000) * config->deadtime_ns) / 1000;
    tim->DTxR |= HRTIM_DTR_DTFSLK | ((deadtime_clocks << HRTIM_DTR_DTF_Pos) & HRTIM_DTR_DTF)
                 | HRTIM_DTR_DTRSLK | ((deadtime_clocks << HRTIM_DTR_DTR_Pos) & HRTIM_DTR_DTR);

    if (config->adc_trigger) {
        /* ADC trigger update: timer A, ADC trigger event: timer A compare 3 */
        hrtim->sCommonRegs.CR1 |= HRTIM_CR1_ADC1USRC_0;
        hrtim->sCommonRegs.ADC1R = HRTIM_ADC1R_AD1TAC3;
    }

    data->compare_min = 0;
    data->compare_max = tim->PERxR;

    /* start timer A */
    hrtim->sMasterRegs.MCR = HRTIM_MCR_TACEN;

    return 0;
}

#define TIMER(n) DT_INST_PARENT(n)

#define HALF_BRIDGE_STM32_HRTIM_INIT(n) \
    PINCTRL_DT_INST_DEFINE(n); \
\
    static const struct stm32_pclken half_bridge_stm32_hrtim_pclken_##n[] = \
        STM32_DT_CLOCKS(TIMER(n)); \
\
    static const struct half_bridge_stm32_hrtim_config half_bridge_stm32_hrtim_config_##n = { \
        .hrtim = (HRTIM_TypeDef *)DT_REG_ADDR(TIMER(n)), \
        .pclken = half_bridge_stm32_hrtim_pclken_##n, \
        .pclk_len = DT_NUM_CLOCKS(TIMER(n)), \
        .reset = RESET_DT_SPEC_GET_OR(TIMER(n), { 0 }), \
        .pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n), \
        .frequency = DT_INST_PROP(n, frequency), \
        .deadtime_ns = DT_INST_PROP(n, deadtime_ns), \
        .adc_trigger = DT_INST_PROP(n, adc_trigger), \
    }; \
\
    static struct half_bridge_stm32_hrtim_data half_bridge_stm32_hrtim_data_##n; \
\
    DEVICE_DT_INST_DEFINE(n, half_bridge_stm32_hrtim_init, NULL, \
                          &half_bridge_stm32_hrtim_data_##n, &half_bridge_stm32_hrtim_config_##n, \
                          POST_KERNEL, CONFIG_HALF_BRIDGE_INIT_PRIORITY, \
                          &half_bridge_stm32_hrtim_api);

DT_INST_FOREACH_STATUS_OKAY(HALF_BRIDGE_STM32_HRTIM_INIT)
