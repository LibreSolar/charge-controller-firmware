/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "half_bridge.h"

#include <stdio.h>

#include "mcu.h"

static uint16_t tim_arr;            // auto-reload register (defines PWM frequency)
static uint16_t tim_ccr_min;        // capture/compare register min/max
static uint16_t tim_ccr_max;
static uint16_t tim_dt_clocks;

static bool pwm_enabled;

#if defined(CONFIG_SOC_SERIES_STM32L0X)

static void pwm_init_registers(uint16_t arr)
{
    // Enable peripheral clock of GPIOB
    RCC->IOPENR |= RCC_IOPENR_IOPBEN;

    // Enable TIM3 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    // Select alternate function mode on PB0 and PB1 (first bit _1 = 1, second bit _0 = 0)
    GPIOB->MODER = (GPIOB->MODER & ~(GPIO_MODER_MODE0)) | GPIO_MODER_MODE0_1;
    GPIOB->MODER = (GPIOB->MODER & ~(GPIO_MODER_MODE1)) | GPIO_MODER_MODE1_1;

    // Select AF2 on PB0 and PB1
    GPIOB->AFR[0] |= 0x2 << GPIO_AFRL_AFSEL0_Pos;
    GPIOB->AFR[0] |= 0x2 << GPIO_AFRL_AFSEL1_Pos;

    // No prescaler --> timer frequency == SystemClock
    TIM3->PSC = 0;

    // Capture/Compare Mode Register 1
    // OCxM = 110: Select PWM mode 1 on OCx
    // OCxPE = 1:  Enable preload register on OCx (reset value)
    TIM3->CCMR2 |= TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3PE;
    TIM3->CCMR2 |= TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4PE;

    // Capture/Compare Enable Register
    // CCxP: Active high polarity on OCx (default = 0)
    TIM3->CCER &= ~(TIM_CCER_CC3P); // PB0 / TIM3_CH3: high-side
    TIM3->CCER |= TIM_CCER_CC4P;    // PB1 / TIM3_CH4: low-side

    // Control Register 1
    // TIM_CR1_CMS = 01: Select center-aligned mode 1
    // TIM_CR1_CEN =  1: Counter enable
    TIM3->CR1 |= TIM_CR1_CMS_0 | TIM_CR1_CEN;

    // Control Register 2
    // OIS1 = OIS1N = 0: Output Idle State is set to off
    //TIM3->CR2 |= ;

    // Force update generation (UG = 1)
    TIM3->EGR |= TIM_EGR_UG;

    // Auto Reload Register
    // center-aligned mode --> resolution is double
    TIM3->ARR = arr;
}

static void pwm_start()
{
    // Capture/Compare Enable Register
    // CCxE = 1: Enable the output on OCx
    // CCxP = 0: Active high polarity on OCx (default)
    // CCxNE = 1: Enable the output on OC1N
    // CCxNP = 0: Active high polarity on OC1N (default)
    TIM3->CCER |= TIM_CCER_CC3E;
    TIM3->CCER |= TIM_CCER_CC4E;
}

static void pwm_stop()
{
    TIM3->CCER &= ~(TIM_CCER_CC3E);
    TIM3->CCER &= ~(TIM_CCER_CC4E);
}

static uint16_t pwm_get_ccr()
{
    return TIM3->CCR3;
}

static void pwm_set_ccr(uint16_t ccr)
{
    TIM3->CCR3 = ccr;                   // high-side
    TIM3->CCR4 = ccr + tim_dt_clocks;   // low-side
}

#elif defined(CONFIG_SOC_SERIES_STM32F0X) || defined(CONFIG_SOC_SERIES_STM32G4X)

static void pwm_init_registers(uint16_t arr)
{
    // Enable peripheral clock of GPIOA and GPIOB
#ifdef CONFIG_SOC_SERIES_STM32F0X
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;
#else // CONFIG_SOC_SERIES_STM32G4X
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;
#endif

    // Enable TIM1 clock
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;

    GPIOA->MODER = (GPIOA->MODER & ~(GPIO_MODER_MODER8)) | GPIO_MODER_MODER8_1;
    GPIOB->MODER = (GPIOB->MODER & ~(GPIO_MODER_MODER13)) | GPIO_MODER_MODER13_1;

    // Select AF2 on PA8 (TIM1_CH1) and PB13 (TIM1_CH1N)
#ifdef CONFIG_SOC_SERIES_STM32F0X
    GPIOA->AFR[1] |= 0x2 << GPIO_AFRH_AFSEL8_Pos;
    GPIOB->AFR[1] |= 0x2 << GPIO_AFRH_AFSEL13_Pos;
#else // CONFIG_SOC_SERIES_STM32G4X
    GPIOA->AFR[1] |= 0x6 << GPIO_AFRH_AFSEL8_Pos;
    GPIOB->AFR[1] |= 0x6 << GPIO_AFRH_AFSEL13_Pos;
#endif
    // No prescaler --> timer frequency == SystemClock
    TIM1->PSC = 0;

    // Capture/Compare Mode Register 1
    // OC1M = 110: Select PWM mode 1 on OC1
    // OC1PE = 1:  Enable preload register on OC1 (reset value)
    TIM1->CCMR1 |= TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1PE;

    // Capture/Compare Enable Register
    // CC1E = 1: Enable the output on OC1
    // CC1P = 0: Active high polarity on OC1 (default)
    // CC1NE = 1: Enable the output on OC1N
    // CC1NP = 0: Active high polarity on OC1N (default)
    TIM1->CCER |= TIM_CCER_CC1E | TIM_CCER_CC1NE; // | TIM_CCER_CC1NP;

    // Control Register 1
    // TIM_CR1_CMS = 01: Select center-aligned mode 1
    // TIM_CR1_CEN =  1: Counter enable
    TIM1->CR1 |= TIM_CR1_CMS_0 | TIM_CR1_CEN;

    // Control Register 2
    // OIS1 = OIS1N = 0: Output Idle State is set to off
    //TIM1->CR2 |= ;

#ifdef CONFIG_SOC_SERIES_STM32G4X
    // Boards with STM32G4 MCU use ADC2 for synchronized ADC sampling. We use OC2
    // for triggering, but but don't configure any pin for PWM mode
    TIM1->CCMR1 |= TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2PE;
    TIM1->CCER |= TIM_CCER_CC2E | TIM_CCER_CC2NE;
    // Trigger ADC via TIM1_TRGO2 on OC2ref signal event
    TIM1->CR2 |= TIM_CR2_MMS2_2 |  TIM_CR2_MMS2_0;
#endif

    // Force update generation (UG = 1)
    TIM1->EGR |= TIM_EGR_UG;

    // Auto Reload Register
    // center-aligned mode --> frequency is half the pwm_resolution;
    TIM1->ARR = arr;

    // Break and Dead-Time Register
    // MOE  = 1: Main output enable
    // OSSR = 0: Off-state selection for Run mode -> OC/OCN = 0
    // OSSI = 0: Off-state selection for Idle mode -> OC/OCN = 0
    TIM1->BDTR |= (tim_dt_clocks & (uint32_t)0x7F); // ensure that only the last 7 bits are changed

    // Lock Break and Dead-Time Register
    // TODO: does not work properly... maybe HW bug?
    TIM1->BDTR |= TIM_BDTR_LOCK_1 | TIM_BDTR_LOCK_0;
}

static void pwm_start()
{
    // Break and Dead-Time Register
    // MOE  = 1: Main output enable
    TIM1->BDTR |= TIM_BDTR_MOE;
}

static void pwm_stop()
{
    // Break and Dead-Time Register
    // MOE  = 1: Main output enable
    TIM1->BDTR &= ~(TIM_BDTR_MOE);
}

static uint32_t pwm_get_ccr()
{
    return TIM1->CCR1;
}

static void pwm_set_ccr(uint32_t ccr)
{
    TIM1->CCR1 = ccr;

    // Trigger ADC for current measurement in the middle of the cycle.
    TIM1->CCR2 = 1;
}

#elif defined(UNIT_TEST)

const uint32_t SystemCoreClock = 24000000;

// dummy registers
uint32_t _tim_ccr = 0;

static void pwm_init_registers(uint16_t arr)
{
    tim_arr = arr;
}

static void pwm_start()
{
    // nothing to do
}

static void pwm_stop()
{
    // nothing to do
}

static uint16_t pwm_get_ccr()
{
    return _tim_ccr;
}

static void pwm_set_ccr(uint16_t ccr)
{
    _tim_ccr = ccr;                // high-side
}

#endif /* UNIT_TEST */

void half_bridge_init(int freq_kHz, int deadtime_ns, float min_duty, float max_duty)
{
    // The timer runs at system clock speed.
    // We are using a mode which counts up and down in each period,
    // so we need half the clocks as resolution for the given frequency.
    tim_arr = SystemCoreClock / (freq_kHz * 1000) / 2;

    // (clocks per ms * deadtime in ns) / 1000 == (clocks per ms * deadtime in ms)
    // although the C operator precedence does the "right thing" to allow deadtime_ns < 1000 to
    // be handled nicely, parentheses make this more explicit
    tim_dt_clocks = ((SystemCoreClock / (1000000)) * deadtime_ns) / 1000;

    // set PWM frequency and resolution (in fact half the resolution)
    pwm_init_registers(tim_arr);

    tim_ccr_min = min_duty * tim_arr;
    tim_ccr_max = max_duty * tim_arr;

    half_bridge_set_duty_cycle(max_duty);      // init with allowed value

    pwm_enabled = false;
}

void half_bridge_set_ccr(uint16_t ccr_target)
{
    // protection against wrong settings which could destroy the hardware
    if (ccr_target < tim_ccr_min) {
        pwm_set_ccr(tim_ccr_min);
    }
    else if (ccr_target > tim_ccr_max) {
        pwm_set_ccr(tim_ccr_max);
    }
    else {
        pwm_set_ccr(ccr_target);
    }
}

uint16_t half_bridge_get_ccr()
{
    return pwm_get_ccr();
}

uint16_t half_bridge_get_arr()
{
    return tim_arr;
}

void half_bridge_set_duty_cycle(float duty)
{
    if (duty >= 0.0 && duty <= 1.0) {
        half_bridge_set_ccr(tim_arr * duty);
    }
}

float half_bridge_get_duty_cycle()
{
    return (float)(pwm_get_ccr()) / tim_arr;
}

void half_bridge_start()
{
    if (pwm_get_ccr() >= tim_ccr_min && pwm_get_ccr() <= tim_ccr_max) {
        pwm_start();
        pwm_enabled = true;
    }
}

void half_bridge_stop()
{
    pwm_stop();
    pwm_enabled = false;
}

bool half_bridge_enabled()
{
    return pwm_enabled;
}
