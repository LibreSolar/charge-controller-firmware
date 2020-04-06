/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "pwm_switch.h"

#ifndef UNIT_TEST
#include <zephyr.h>
#endif

#include <stdio.h>
#include <time.h>

#include "board.h"
#include "daq.h"
#include "debug.h"
#include "helper.h"

#ifdef CONFIG_SOC_STM32L073XX
#include "stm32l073xx.h"
#endif

#ifdef CONFIG_SOC_STM32G431XX
#include "stm32g431xx.h"
#endif

#if DT_OUTPUTS_PWM_SWITCH_PRESENT

#if !defined(UNIT_TEST) && !defined(CONFIG_SOC_STM32G431XX)     // not yet working for STM32G431

static bool _pwm_active;
static int _pwm_resolution;

static void TIM3_IRQHandler(void *args)
{
    TIM3->SR &= ~TIM_SR_UIF;       // clear update interrupt flag

    if ((int)TIM3->CCR4 < _pwm_resolution) {
        // turning the PWM switch on creates a short voltage rise, so inhibit alerts by 10 ms
        // at each rising edge if switch is not continuously on
        adc_upper_alert_inhibit(ADC_POS_V_LOW, 10);
    }
}

void pwm_signal_init_registers(int freq_Hz)
{
    // Enable peripheral clock of GPIOB
    RCC->IOPENR |= RCC_IOPENR_IOPBEN;

    // Enable TIM3 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    // Select alternate function mode on PB1 (first bit _1 = 1, second bit _0 = 0)
    GPIOB->MODER = (GPIOB->MODER & ~(GPIO_MODER_MODE1)) | GPIO_MODER_MODE1_1;

    // Select AF2 on PB1
    GPIOB->AFR[0] |= 0x2 << GPIO_AFRL_AFSEL1_Pos;

    // Set timer clock to 10 kHz
    TIM3->PSC = SystemCoreClock / 10000 - 1;

    // Capture/Compare Mode Register 1
    // OCxM = 110: Select PWM mode 1 on OCx
    // OCxPE = 1:  Enable preload register on OCx (reset value)
    TIM3->CCMR2 |= TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4PE;

    // Capture/Compare Enable Register
    // CCxP: Active high polarity on OCx (default = 0)
    TIM3->CCER &= ~(TIM_CCER_CC4P);     // PB1 / TIM3_CH4: high-side

    // Interrupt on timer update
    TIM3->DIER |= TIM_DIER_UIE;

    // Auto Reload Register sets interrupt frequency
    TIM3->ARR = 10000 / freq_Hz - 1;

    // 1 = second-highest priority of STM32L0/F0
    IRQ_CONNECT(TIM3_IRQn, 1, TIM3_IRQHandler, 0, 0);
    irq_enable(TIM3_IRQn);

    // Force update generation (UG = 1)
    TIM3->EGR |= TIM_EGR_UG;

    // set PWM frequency and resolution
    _pwm_resolution = 10000 / freq_Hz;

    // Auto Reload Register
    // Period goes from 0 to ARR (including ARR value), so substract 1 clock cycle
    TIM3->ARR = _pwm_resolution - 1;
}

void pwm_signal_set_duty_cycle(float duty)
{
    TIM3->CCR4 = _pwm_resolution * duty;
}

void pwm_signal_duty_cycle_step(int delta)
{
    if ((int)TIM3->CCR4 + delta <= _pwm_resolution && (int)TIM3->CCR4 + delta >= 0) {
        TIM3->CCR4 += delta;
    }
}

float pwm_signal_get_duty_cycle()
{
    return (float)(TIM3->CCR4) / _pwm_resolution;
}

void pwm_signal_start(float pwm_duty)
{
    pwm_signal_set_duty_cycle(pwm_duty);

    // Control Register 1
    // TIM_CR1_CEN =  1: Counter enable
    TIM3->CR1 |= TIM_CR1_CEN;   // edge-aligned mode

    // Capture/Compare Enable Register
    // CCxE = 1: Enable the output on OCx
    // CCxP = 0: Active high polarity on OCx (default)
    // CCxNE = 1: Enable the output on OC1N
    // CCxNP = 0: Active high polarity on OC1N (default)
    TIM3->CCER |= TIM_CCER_CC4E;

    _pwm_active = true;
}

void pwm_signal_stop()
{
    TIM3->CR1 &= ~(TIM_CR1_CEN);
    TIM3->CCER &= ~(TIM_CCER_CC4E);
    TIM3->CCR4 = 0;
    _pwm_active = false;
}

bool pwm_signal_high()
{
    return (GPIOB->IDR & GPIO_IDR_ID1);
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

#endif /* DT_OUTPUTS_PWM_SWITCH_PRESENT */
