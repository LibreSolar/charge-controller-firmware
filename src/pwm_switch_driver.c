/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "pwm_switch.h"

#include <zephyr.h>

#include <stdio.h>
#include <time.h>

#include "daq.h"
#include "helper.h"
#include "mcu.h"

#if defined(CONFIG_SOC_SERIES_STM32L0X)
#include <stm32l0xx_ll_tim.h>
#include <stm32l0xx_ll_rcc.h>
#include <stm32l0xx_ll_bus.h>
#elif defined(CONFIG_SOC_SERIES_STM32G4X)
#include <stm32g4xx_ll_tim.h>
#include <stm32g4xx_ll_rcc.h>
#include <stm32g4xx_ll_bus.h>
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), pwm_switch))

#ifndef UNIT_TEST

// all PWM charge controllers use TIM3 at the moment
static TIM_TypeDef *tim = TIM3;

#if DT_PWMS_CHANNEL(DT_CHILD(DT_PATH(outputs), pwm_switch)) == 1
#define LL_TIM_CHANNEL LL_TIM_CHANNEL_CH1
#define LL_TIM_OC_SetCompare LL_TIM_OC_SetCompareCH1
#define LL_TIM_OC_GetCompare LL_TIM_OC_GetCompareCH1
#elif DT_PWMS_CHANNEL(DT_CHILD(DT_PATH(outputs), pwm_switch)) == 2
#define LL_TIM_CHANNEL LL_TIM_CHANNEL_CH2
#define LL_TIM_OC_SetCompare LL_TIM_OC_SetCompareCH2
#define LL_TIM_OC_GetCompare LL_TIM_OC_GetCompareCH2
#elif DT_PWMS_CHANNEL(DT_CHILD(DT_PATH(outputs), pwm_switch)) == 3
#define LL_TIM_CHANNEL LL_TIM_CHANNEL_CH3
#define LL_TIM_OC_SetCompare LL_TIM_OC_SetCompareCH3
#define LL_TIM_OC_GetCompare LL_TIM_OC_GetCompareCH3
#elif DT_PWMS_CHANNEL(DT_CHILD(DT_PATH(outputs), pwm_switch)) == 4
#define LL_TIM_CHANNEL LL_TIM_CHANNEL_CH4
#define LL_TIM_OC_SetCompare LL_TIM_OC_SetCompareCH4
#define LL_TIM_OC_GetCompare LL_TIM_OC_GetCompareCH4
#else
#error "PWM Switch channel not defined properly!"
#endif

// to check if PWM signal is high or low (not sure how to get pin config from devicetree...)
#if defined(CONFIG_BOARD_PWM_2420_LUS)
#define PWM_GPIO_PIN_HIGH (GPIOB->IDR & GPIO_IDR_ID1)
#elif defined(CONFIG_BOARD_MPPT_2420_HPX)
#define PWM_GPIO_PIN_HIGH (GPIOC->IDR & GPIO_IDR_ID7)
#endif

static bool _pwm_active;
static int _pwm_resolution;

static void TIM3_IRQHandler(void *args)
{
    LL_TIM_ClearFlag_UPDATE(tim);

    if ((int)LL_TIM_OC_GetCompare(tim) < _pwm_resolution) {
        // turning the PWM switch on creates a short voltage rise, so inhibit alerts by 10 ms
        // at each rising edge if switch is not continuously on
        adc_upper_alert_inhibit(ADC_POS(v_low), 10);
    }
}

void pwm_signal_init_registers(int freq_Hz)
{
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);

    // Set timer clock to 10 kHz
    LL_TIM_SetPrescaler(TIM3, SystemCoreClock / 10000 - 1);

    LL_TIM_OC_SetMode(tim, LL_TIM_CHANNEL, LL_TIM_OCMODE_PWM1);
    LL_TIM_OC_EnablePreload(tim, LL_TIM_CHANNEL);
    LL_TIM_OC_SetPolarity(tim, LL_TIM_CHANNEL, LL_TIM_OCPOLARITY_HIGH);

    // Interrupt on timer update
    LL_TIM_EnableIT_UPDATE(tim);

    // Force update generation (UG = 1)
    LL_TIM_GenerateEvent_UPDATE(tim);

    // set PWM frequency and resolution
    _pwm_resolution = 10000 / freq_Hz;

    // Period goes from 0 to ARR (including ARR value), so substract 1 clock cycle
    LL_TIM_SetAutoReload(tim, _pwm_resolution - 1);

    // 1 = second-highest priority of STM32L0/F0
    IRQ_CONNECT(TIM3_IRQn, 1, TIM3_IRQHandler, 0, 0);
    irq_enable(TIM3_IRQn);

    LL_TIM_EnableCounter(tim);
}

void pwm_signal_set_duty_cycle(float duty)
{
    LL_TIM_OC_SetCompare(tim, _pwm_resolution * duty);
}

void pwm_signal_duty_cycle_step(int delta)
{
    uint32_t ccr_new = (int)LL_TIM_OC_GetCompare(tim) + delta;
    if (ccr_new <= _pwm_resolution && ccr_new >= 0) {
        LL_TIM_OC_SetCompare(tim, ccr_new);
    }
}

float pwm_signal_get_duty_cycle()
{
    return (float)(LL_TIM_OC_GetCompare(tim)) / _pwm_resolution;
}

void pwm_signal_start(float pwm_duty)
{
    pwm_signal_set_duty_cycle(pwm_duty);
    LL_TIM_CC_EnableChannel(tim, LL_TIM_CHANNEL);
    _pwm_active = true;
}

void pwm_signal_stop()
{
    LL_TIM_CC_DisableChannel(tim, LL_TIM_CHANNEL);
    _pwm_active = false;
}

bool pwm_signal_high()
{
    return PWM_GPIO_PIN_HIGH;
}

bool pwm_active()
{
    return _pwm_active;
}

#else

// dummy functions for unit tests
float pwm_signal_get_duty_cycle() { return 0; }
void pwm_signal_set_duty_cycle(float duty) {;}
void pwm_signal_duty_cycle_step(int delta) {;}
void pwm_signal_init_registers(int freq_Hz) {;}
void pwm_signal_start(float pwm_duty) {;}
void pwm_signal_stop() {;}
bool pwm_signal_high() { return false; }
bool pwm_active() { return false; }

#endif

#endif /* DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), pwm_switch)) */
