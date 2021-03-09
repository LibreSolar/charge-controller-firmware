/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "half_bridge.h"

#include <zephyr.h>

#include <stdio.h>

#include "mcu.h"
#include "setup.h"

#ifndef UNIT_TEST
#include <soc.h>
#include <pinmux/stm32/pinmux_stm32.h>
#include <stm32_ll_bus.h>
#endif

#if BOARD_HAS_DCDC

#define DT_DRV_COMPAT half_bridge

// Get address of used timer from board dts
#define TIMER_ADDR DT_REG_ADDR(DT_PARENT(DT_DRV_INST(0)))

static uint16_t tim_ccr_min;        // capture/compare register min/max
static uint16_t tim_ccr_max;
static uint16_t tim_dt_clocks = 0;

static uint16_t clamp_ccr(uint16_t ccr_target)
{
    // protection against wrong settings which could destroy the hardware
    if (ccr_target <= tim_ccr_min) {
        return tim_ccr_min;
    }
    else if (ccr_target >= tim_ccr_max) {
        return tim_ccr_max;
    }
    else {
        return ccr_target;
    }
}

#ifndef UNIT_TEST

static const struct soc_gpio_pinctrl tim_pinctrl[] = ST_STM32_DT_INST_PINCTRL(0, 0);

#if TIMER_ADDR == TIM3_BASE

static void tim_init_registers(int freq_kHz)
{
    stm32_dt_pinctrl_configure(tim_pinctrl, ARRAY_SIZE(tim_pinctrl), TIMER_ADDR);

    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);

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

    // Force update generation (UG = 1)
    TIM3->EGR |= TIM_EGR_UG;

    // Auto Reload Register
    // center-aligned mode counts up and down in each period, so we need half the clocks as
    // resolution for the given frequency.
    TIM3->ARR = SystemCoreClock / (freq_kHz * 1000) / 2;;
}

void half_bridge_start()
{
    if (TIM3->CCR3 != 0U) {
        // Capture/Compare Enable Register
        // CCxE = 1: Enable the output on OCx
        // CCxP = 0: Active high polarity on OCx (default)
        TIM3->CCER |= TIM_CCER_CC3E;
        TIM3->CCER |= TIM_CCER_CC4E;
    }
}

void half_bridge_stop()
{
    TIM3->CCER &= ~(TIM_CCER_CC3E);
    TIM3->CCER &= ~(TIM_CCER_CC4E);
}

uint16_t half_bridge_get_arr()
{
    return TIM3->ARR;
}

uint16_t half_bridge_get_ccr()
{
    return TIM3->CCR3;
}

void half_bridge_set_ccr(uint16_t ccr)
{
    uint16_t ccr_clamped = clamp_ccr(ccr);

    TIM3->CCR3 = ccr_clamped;                   // high-side
    TIM3->CCR4 = ccr_clamped + tim_dt_clocks;   // low-side
}

bool half_bridge_enabled()
{
    return TIM3->CCER & TIM_CCER_CC3E;
}

#elif TIMER_ADDR == TIM1_BASE

static void tim_init_registers(int freq_kHz)
{
    stm32_dt_pinctrl_configure(tim_pinctrl, ARRAY_SIZE(tim_pinctrl), TIMER_ADDR);

#ifdef LL_APB2_GRP1_PERIPH_TIM1
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1);
#else
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_TIM1);
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
    TIM1->CCER |= TIM_CCER_CC1E | TIM_CCER_CC1NE;

    // Control Register 1
    // TIM_CR1_CMS = 00: Select edge-aligned mode
    // TIM_CR1_CEN =  1: Counter enable
    TIM1->CR1 |= TIM_CR1_CEN;

#ifdef CONFIG_SOC_SERIES_STM32G4X
    // Boards with STM32G4 MCU use ADC2 for synchronized ADC sampling. We use OC6
    // for triggering, but but don't configure any pin for PWM mode
    TIM1->CCMR3 |= TIM_CCMR3_OC6M_2 | TIM_CCMR3_OC6M_1 | TIM_CCMR3_OC6PE;
    TIM1->CCER |= TIM_CCER_CC6E;
    // Trigger ADC via TIM1_TRGO2 on OC6ref falling edge signal event
    TIM1->CR2 |= TIM_CR2_MMS2_3 | TIM_CR2_MMS2_2 |  TIM_CR2_MMS2_0;
#endif

    // Force update generation (UG = 1)
    TIM1->EGR |= TIM_EGR_UG;

    // Auto Reload Register
    // edge-aligned mode possible with TIM1 to increase resolution (no division by 2 necessary)
    TIM1->ARR = SystemCoreClock / (freq_kHz * 1000);

    // Break and Dead-Time Register
    // DTG[7:0]: Dead-time generator setup
    TIM1->BDTR |= (tim_dt_clocks & (uint32_t)0x7F); // ensure that only the last 7 bits are changed

    // Lock Break and Dead-Time Register
    // TODO: does not work properly... maybe HW bug?
    TIM1->BDTR |= TIM_BDTR_LOCK_1 | TIM_BDTR_LOCK_0;
}

void half_bridge_start()
{
    if (TIM1->CCR1 != 0U) {
        // Break and Dead-Time Register
        // MOE  = 1: Main output enable
        TIM1->BDTR |= TIM_BDTR_MOE;
    }
}

void half_bridge_stop()
{
    // Break and Dead-Time Register
    // MOE  = 1: Main output enable
    TIM1->BDTR &= ~(TIM_BDTR_MOE);
}

uint16_t half_bridge_get_arr()
{
    return TIM1->ARR;
}

uint16_t half_bridge_get_ccr()
{
    return TIM1->CCR1;
}

void half_bridge_set_ccr(uint16_t ccr)
{
    TIM1->CCR1 = clamp_ccr(ccr);

#ifdef CONFIG_SOC_SERIES_STM32G4X
    // Trigger ADC for current measurement in the middle of the cycle.
    TIM1->CCR6 = TIM1->CCR1 / 2;
#endif
}

bool half_bridge_enabled()
{
    return TIM1->BDTR & TIM_BDTR_MOE;
}

#elif TIMER_ADDR == HRTIM1_BASE

/*
 * Below initial HRTIM implementation only works for channel A and it does not make use of the
 * possible higher resolution yet.
 */

#include <stm32_ll_system.h>
#include <stm32_ll_bus.h>

static void tim_init_registers(int freq_kHz)
{
    stm32_dt_pinctrl_configure(tim_pinctrl, ARRAY_SIZE(tim_pinctrl), TIMER_ADDR);

    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_HRTIM1);

    // Enable periodic calibration of delay-locked loop (DLL) with lowest calibration period
    HRTIM1->sCommonRegs.DLLCR |= HRTIM_DLLCR_CALEN | HRTIM_DLLCR_CALRTE_0 | HRTIM_DLLCR_CALRTE_1;

    // Wait for calibration to finish
    while ((HRTIM1_COMMON->ISR & HRTIM_ISR_DLLRDY) == 0) {;}

    // Prescaler 32 --> SystemClock (same as normal timers like TIM1)
    HRTIM1_TIMA->TIMxCR |= (5U << HRTIM_TIMCR_CK_PSC_Pos);

    // Prescaler 8 / 2^3 --> same frequency as HRTIM
    HRTIM1_TIMA->DTxR |= (3U << HRTIM_DTR_DTPRSC_Pos);

    // Continuous mode operation
    HRTIM1_TIMA->TIMxCR |= HRTIM_TIMCR_CONT;

    // Enable preloading with timer reset update trigger
    //HRTIM1_TIMA->TIMxCR |= HRTIM_TIMCR_PREEN | HRTIM_TIMCR_TRSTU;

    // Enable preloading with timer E update trigger
    //HRTIM1_TIMA->TIMxCR |= HRTIM_TIMCR_PREEN | HRTIM_TIMCR_TEU;

    // Timer period register
    HRTIM1_TIMA->PERxR = SystemCoreClock / (freq_kHz * 1000) + 1;

    HRTIM1_TIMA->SETx1R = HRTIM_SET1R_PER;
    HRTIM1_TIMA->RSTx1R = HRTIM_SET1R_CMP1;

    HRTIM1_TIMA->OUTxR = HRTIM_OUTR_DTEN;

    // Set deadtime values and lock deadtime signs
    HRTIM1_TIMA->DTxR |=
        HRTIM_DTR_DTFSLK | (tim_dt_clocks << 16U) |
        HRTIM_DTR_DTRSLK | tim_dt_clocks;

    // activate trigger for ADC
    HRTIM1_COMMON->CR1 = HRTIM_CR1_ADC1USRC_0; // ADC trigger update: Timer A
    HRTIM1_COMMON->ADC1R = HRTIM_ADC1R_AD1TAC3; // ADC trigger event: Timer A compare 3

    // Start timer A
    HRTIM1->sMasterRegs.MCR = HRTIM_MCR_TACEN;
}

void half_bridge_start()
{
    HRTIM1->sCommonRegs.OENR = HRTIM_OENR_TA1OEN | HRTIM_OENR_TA2OEN;
}

void half_bridge_stop()
{
    HRTIM1->sCommonRegs.ODISR = HRTIM_ODISR_TA1ODIS | HRTIM_ODISR_TA2ODIS;
}

uint16_t half_bridge_get_arr()
{
    return HRTIM1_TIMA->PERxR;
}

uint16_t half_bridge_get_ccr()
{
    return HRTIM1_TIMA->CMP1xR;
}

void half_bridge_set_ccr(uint16_t ccr)
{
    HRTIM1_TIMA->CMP1xR = clamp_ccr(ccr);

    // Trigger ADC for current measurement in the middle of the cycle.
    // A negative offset of 80 clocks was found to improve current measurement accuracy and
    // compensate the ADC delay (no risk of underflow as CCR is around 214 even at 0.1% duty).
    HRTIM1_TIMA->CMP3xR = HRTIM1_TIMA->CMP1xR / 2 - 80;
}

bool half_bridge_enabled()
{
    return (HRTIM1->sCommonRegs.OENR & (HRTIM_OENR_TA1OEN | HRTIM_OENR_TA2OEN)) > 0;
}

#endif // HRTIM1

#else // UNIT_TEST

const uint32_t SystemCoreClock = 24000000;

// dummy registers
uint32_t tim_ccr = 0;
uint32_t tim_arr = 0;
bool pwm_enabled = false;

static void tim_init_registers(int freq_kHz)
{
    // assuming edge-aligned PWM like with TIM1
    tim_arr = SystemCoreClock / (freq_kHz * 1000);
}

void half_bridge_start()
{
    pwm_enabled = true;
}

void half_bridge_stop()
{
    pwm_enabled = false;
}

uint16_t half_bridge_get_arr()
{
    return tim_arr;
}

uint16_t half_bridge_get_ccr()
{
    return tim_ccr;
}

void half_bridge_set_ccr(uint16_t ccr)
{
    tim_ccr = clamp_ccr(ccr);          // high-side
}

bool half_bridge_enabled()
{
    return pwm_enabled;
}

#endif /* UNIT_TEST */

static void tim_calculate_dt_clocks(int deadtime_ns)
{
    // (clocks per ms * deadtime in ns) / 1000 == (clocks per ms * deadtime in ms)
    // although the C operator precedence does the "right thing" to allow deadtime_ns < 1000 to
    // be handled nicely, parentheses make this more explicit
    tim_dt_clocks = ((SystemCoreClock / (1000000)) * deadtime_ns) / 1000;
}

void half_bridge_init(int freq_kHz, int deadtime_ns, float min_duty, float max_duty)
{
    tim_calculate_dt_clocks(deadtime_ns);

    tim_init_registers(freq_kHz);

    tim_ccr_min = min_duty * half_bridge_get_arr();
    tim_ccr_max = max_duty * half_bridge_get_arr();

    half_bridge_set_duty_cycle(max_duty);      // init with allowed value
}

void half_bridge_set_duty_cycle(float duty)
{
    if (duty >= 0.0 && duty <= 1.0) {
        half_bridge_set_ccr(half_bridge_get_arr() * duty);
    }
}

float half_bridge_get_duty_cycle()
{
    return (float)(half_bridge_get_ccr()) / half_bridge_get_arr();
}

#endif // BOARD_HAS_DCDC
