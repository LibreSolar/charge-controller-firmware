/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Input capture of the loop-back signals using an STM32 timer
 *
 * Both channels of the capture timer share the same 32-bit counter, so the captured timestamps of
 * the high-side and low-side signals can be compared directly. Each channel captures both edges.
 * The register is polled with interrupts locked, and the signal level is read via GPIO right
 * after each edge to determine its direction.
 */

#include "capture.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/irq.h>
#include <zephyr/sys/util.h>

#include <soc.h>
#include <stm32_ll_tim.h>

#define LOOPBACK_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(test_half_bridge_loopback)
#define TIMER_NODE    DT_PHANDLE(LOOPBACK_NODE, capture_timer)

#define HS_CHANNEL DT_PROP(LOOPBACK_NODE, high_side_channel)
#define LS_CHANNEL DT_PROP(LOOPBACK_NODE, low_side_channel)

BUILD_ASSERT(HS_CHANNEL != LS_CHANNEL, "Capture channels must be different");

PINCTRL_DT_DEFINE(LOOPBACK_NODE);

static const struct stm32_pclken pclken[] = STM32_DT_CLOCKS(TIMER_NODE);

static TIM_TypeDef *const tim = (TIM_TypeDef *)DT_REG_ADDR(TIMER_NODE);

static const struct gpio_dt_spec hs_gpio = GPIO_DT_SPEC_GET(LOOPBACK_NODE, high_side_gpios);
static const struct gpio_dt_spec ls_gpio = GPIO_DT_SPEC_GET(LOOPBACK_NODE, low_side_gpios);

static const uint32_t ll_channels[] = {
    LL_TIM_CHANNEL_CH1,
    LL_TIM_CHANNEL_CH2,
    LL_TIM_CHANNEL_CH3,
    LL_TIM_CHANNEL_CH4,
};

static uint32_t tim_clk;

/* capture/compare registers CCR1 to CCR4 are located at consecutive addresses */
static inline volatile uint32_t *ccr(uint8_t channel)
{
    return &tim->CCR1 + (channel - 1);
}

static inline uint32_t flag_captured(uint8_t channel)
{
    return TIM_SR_CC1IF << (channel - 1);
}

static inline uint32_t flag_overrun(uint8_t channel)
{
    return TIM_SR_CC1OF << (channel - 1);
}

static inline bool read_level(const struct gpio_dt_spec *spec)
{
    gpio_port_value_t value;

    gpio_port_get_raw(spec->port, &value);

    return (value & BIT(spec->pin)) != 0;
}

static void configure_channel(uint8_t channel)
{
    uint32_t ll_channel = ll_channels[channel - 1];

    LL_TIM_IC_SetActiveInput(tim, ll_channel, LL_TIM_ACTIVEINPUT_DIRECTTI);
    LL_TIM_IC_SetPrescaler(tim, ll_channel, LL_TIM_ICPSC_DIV1);
    LL_TIM_IC_SetFilter(tim, ll_channel, LL_TIM_IC_FILTER_FDIV1);
    LL_TIM_IC_SetPolarity(tim, ll_channel, LL_TIM_IC_POLARITY_BOTHEDGE);
    LL_TIM_CC_EnableChannel(tim, ll_channel);
}

int capture_init(void)
{
    const struct device *clk = DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE);
    int err;

    if (!IS_TIM_32B_COUNTER_INSTANCE(tim)) {
        return -ENOTSUP;
    }

    if (!device_is_ready(clk) || !gpio_is_ready_dt(&hs_gpio) || !gpio_is_ready_dt(&ls_gpio)) {
        return -ENODEV;
    }

    err = clock_control_on(clk, (clock_control_subsys_t)&pclken[0]);
    if (err < 0) {
        return err;
    }

    err = clock_control_get_rate(clk, (clock_control_subsys_t)&pclken[1], &tim_clk);
    if (err < 0) {
        return err;
    }

    err = pinctrl_apply_state(PINCTRL_DT_DEV_CONFIG_GET(LOOPBACK_NODE), PINCTRL_STATE_DEFAULT);
    if (err < 0) {
        return err;
    }

    LL_TIM_DisableCounter(tim);
    LL_TIM_SetPrescaler(tim, 0);
    LL_TIM_SetCounterMode(tim, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetAutoReload(tim, UINT32_MAX);

    configure_channel(HS_CHANNEL);
    configure_channel(LS_CHANNEL);

    LL_TIM_GenerateEvent_UPDATE(tim);
    LL_TIM_EnableCounter(tim);

    return 0;
}

static inline void store_edge(struct capture_signal *signal, uint32_t time, bool level)
{
    if (signal->num_edges < CAPTURE_MAX_EDGES) {
        signal->edges[signal->num_edges].time = time;
        signal->edges[signal->num_edges].level = level;
        signal->num_edges++;
    }
}

void capture_record(struct capture_result *result, uint32_t timeout_us)
{
    uint32_t timeout_clocks = (uint64_t)tim_clk * timeout_us / 1000000;

    result->hs.num_edges = 0;
    result->ls.num_edges = 0;

    unsigned int key = irq_lock();

    /* discard edges captured before */
    (void)*ccr(HS_CHANNEL);
    (void)*ccr(LS_CHANNEL);
    tim->SR = 0;

    result->hs.level_at_start = read_level(&hs_gpio);
    result->ls.level_at_start = read_level(&ls_gpio);

    uint32_t start = tim->CNT;

    while ((result->hs.num_edges < CAPTURE_MAX_EDGES || result->ls.num_edges < CAPTURE_MAX_EDGES)
           && tim->CNT - start < timeout_clocks)
    {
        uint32_t sr = tim->SR;

        /* reading the capture register clears the flag */
        if (sr & flag_captured(HS_CHANNEL)) {
            store_edge(&result->hs, *ccr(HS_CHANNEL), read_level(&hs_gpio));
        }
        if (sr & flag_captured(LS_CHANNEL)) {
            store_edge(&result->ls, *ccr(LS_CHANNEL), read_level(&ls_gpio));
        }
    }

    result->hs.level_at_end = read_level(&hs_gpio);
    result->ls.level_at_end = read_level(&ls_gpio);
    result->overrun = (tim->SR & (flag_overrun(HS_CHANNEL) | flag_overrun(LS_CHANNEL))) != 0;

    irq_unlock(key);
}

static inline uint32_t clocks_to_ns(uint32_t clocks)
{
    return (uint64_t)clocks * 1000000000 / tim_clk;
}

void capture_analyze(const struct capture_result *result, struct capture_timing *timing)
{
    const struct capture_signal *signals[2] = { &result->hs, &result->ls };
    size_t next[2] = { 0, 0 };
    int level[2] = { -1, -1 }; /* unknown until first edge */
    uint32_t last_rise[2] = { 0, 0 };
    bool rise_seen[2] = { false, false };
    uint64_t on_sum[2] = { 0, 0 };
    int on_count[2] = { 0, 0 };
    uint64_t period_sum = 0;
    uint32_t last_time = 0;
    bool started = false;

    *timing = (struct capture_timing){ 0 };

    if (result->hs.num_edges == 0 || result->ls.num_edges == 0) {
        return;
    }

    timing->deadtime_min = UINT32_MAX;

    /* timestamps relative to the first edge to handle counter overflow */
    uint32_t t0 = (int32_t)(result->hs.edges[0].time - result->ls.edges[0].time) <= 0
                      ? result->hs.edges[0].time
                      : result->ls.edges[0].time;

    /* merge edges of both signals in chronological order */
    while (next[0] < signals[0]->num_edges || next[1] < signals[1]->num_edges) {
        int s;

        if (next[0] >= signals[0]->num_edges) {
            s = 1;
        }
        else if (next[1] >= signals[1]->num_edges) {
            s = 0;
        }
        else {
            s = (signals[0]->edges[next[0]].time - t0 <= signals[1]->edges[next[1]].time - t0) ? 0
                                                                                               : 1;
        }

        const struct capture_edge *edge = &signals[s]->edges[next[s]++];
        uint32_t time = edge->time - t0;

        /* time interval since last edge with constant levels of both signals */
        if (started && level[0] >= 0 && level[1] >= 0) {
            uint32_t duration = time - last_time;

            if (level[0] == 0 && level[1] == 0) {
                timing->num_deadtimes++;
                timing->deadtime_min = MIN(timing->deadtime_min, duration);
                timing->deadtime_max = MAX(timing->deadtime_max, duration);
            }
            else if (level[0] == 1 && level[1] == 1) {
                timing->overlap += duration;
            }
        }

        if (level[s] == (int)edge->level) {
            /* no level change: an edge was missed */
            timing->invalid = true;
        }

        if (edge->level) {
            if (s == 0 && rise_seen[0]) {
                period_sum += time - last_rise[0];
                timing->num_periods++;
            }
            last_rise[s] = time;
            rise_seen[s] = true;
        }
        else if (rise_seen[s]) {
            on_sum[s] += time - last_rise[s];
            on_count[s]++;
        }

        level[s] = edge->level;
        last_time = time;
        started = true;
    }

    if (timing->num_periods > 0) {
        timing->period_avg = clocks_to_ns(period_sum / timing->num_periods);
    }
    if (on_count[0] > 0) {
        timing->hs_on_avg = clocks_to_ns(on_sum[0] / on_count[0]);
    }
    if (on_count[1] > 0) {
        timing->ls_on_avg = clocks_to_ns(on_sum[1] / on_count[1]);
    }
    if (timing->num_deadtimes > 0) {
        timing->deadtime_min = clocks_to_ns(timing->deadtime_min);
        timing->deadtime_max = clocks_to_ns(timing->deadtime_max);
    }
    else {
        timing->deadtime_min = 0;
    }
    timing->overlap = clocks_to_ns(timing->overlap);
}
