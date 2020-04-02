/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "half_bridge.h"

#include <stdio.h>

#include "mcu.h"
#include "pcb.h"
#include "config.h"

static int _pwm_resolution;
static float _min_duty;
static float _max_duty;
static uint8_t _deadtime_clocks;

static bool _enabled;

#if defined(STM32L0)

class PWM_TIM3
{
    public:

    static void _init_registers(uint32_t pwm_resolution)
    {
        // Enable peripheral clock of GPIOB
        RCC->IOPENR |= RCC_IOPENR_IOPBEN;

        // Enable TIM3 clock
        RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

        // Select alternate function mode on PB0 and PB1 (first bit _1 = 1, second bit _0 = 0)
        GPIOB->MODER = (GPIOB->MODER & ~(GPIO_MODER_MODE0)) | GPIO_MODER_MODE0_1;
        GPIOB->MODER = (GPIOB->MODER & ~(GPIO_MODER_MODE1)) | GPIO_MODER_MODE1_1;

        // Select AF2 on PB0 and PB1
#ifdef __MBED__
        GPIOB->AFR[0] |= 0x2 << GPIO_AFRL_AFRL0_Pos;
        GPIOB->AFR[0] |= 0x2 << GPIO_AFRL_AFRL1_Pos;
#else
        GPIOB->AFR[0] |= 0x2 << GPIO_AFRL_AFSEL0_Pos;
        GPIOB->AFR[0] |= 0x2 << GPIO_AFRL_AFSEL1_Pos;
#endif

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
        TIM3->ARR = pwm_resolution;
    }

    static void start()
    {
        // Capture/Compare Enable Register
        // CCxE = 1: Enable the output on OCx
        // CCxP = 0: Active high polarity on OCx (default)
        // CCxNE = 1: Enable the output on OC1N
        // CCxNP = 0: Active high polarity on OC1N (default)
        TIM3->CCER |= TIM_CCER_CC3E;
        TIM3->CCER |= TIM_CCER_CC4E;
    }

    static void stop()
    {
        TIM3->CCER &= ~(TIM_CCER_CC3E);
        TIM3->CCER &= ~(TIM_CCER_CC4E);
    }

    static int32_t get_ccr()
    {
        return TIM3->CCR3;
    }

    static void set_ccr(uint32_t val)
    {
        TIM3->CCR3 = val;                    // high-side
        TIM3->CCR4 = val + _deadtime_clocks; // low-side
    }
};

#elif defined(STM32F0) || defined(STM32G4)

class PWM_TIM1
{
    public:

    static void _init_registers(uint32_t pwm_resolution)
    {
        // Enable peripheral clock of GPIOA and GPIOB
#ifdef STM32F0
        RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;
#else // STM32G4
        RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;
#endif

        // Enable TIM1 clock
        RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;

        GPIOA->MODER = (GPIOA->MODER & ~(GPIO_MODER_MODER8)) | GPIO_MODER_MODER8_1;
        GPIOB->MODER = (GPIOB->MODER & ~(GPIO_MODER_MODER13)) | GPIO_MODER_MODER13_1;

        // Select AF2 on PA8 (TIM1_CH1) and PB13 (TIM1_CH1N)
#ifdef STM32F0
        GPIOA->AFR[1] |= 0x2 << GPIO_AFRH_AFSEL8_Pos;
        GPIOB->AFR[1] |= 0x2 << GPIO_AFRH_AFSEL13_Pos;
#else // STM32G4
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

#ifdef STM32G4
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
        TIM1->ARR = pwm_resolution;

        // Break and Dead-Time Register
        // MOE  = 1: Main output enable
        // OSSR = 0: Off-state selection for Run mode -> OC/OCN = 0
        // OSSI = 0: Off-state selection for Idle mode -> OC/OCN = 0
        TIM1->BDTR |= (_deadtime_clocks & (uint32_t)0x7F); // ensure that only the last 7 bits are changed

        // Lock Break and Dead-Time Register
        // TODO: does not work properly... maybe HW bug?
        TIM1->BDTR |= TIM_BDTR_LOCK_1 | TIM_BDTR_LOCK_0;
    }

    static void start()
    {
        // Break and Dead-Time Register
        // MOE  = 1: Main output enable
        TIM1->BDTR |= TIM_BDTR_MOE;
    }

    static void stop()
    {
        // Break and Dead-Time Register
        // MOE  = 1: Main output enable
        TIM1->BDTR &= ~(TIM_BDTR_MOE);
    }

    static int32_t get_ccr()
    {
        return TIM1->CCR1;
    }

    static void set_ccr(uint32_t val)
    {
        TIM1->CCR1 = val;

        // Trigger ADC for current measurement in the middle of the cycle.
        TIM1->CCR2 = 1;
    }
};

#else /* UNIT_TEST */

#ifdef UNIT_TEST
const uint32_t SystemCoreClock = 24000000;
#endif

class PWM_UT
{
    public:
    static uint32_t m_pwm_resolution;
    static bool     m_started;
    static uint32_t m_ccr;

    static void _init_registers(uint32_t pwm_resolution)
    {
        m_pwm_resolution = pwm_resolution;
    }

    static void start()
    {
        m_started = true;
    }

    static void stop()
    {
        m_started = false;
    }

    static int32_t get_ccr()
    {
        return m_ccr;
    }

    static void set_ccr(uint32_t val)
    {
        m_ccr = val;                    // high-side
    }
};

uint32_t PWM_UT::m_pwm_resolution = 0;
bool     PWM_UT::m_started = false;
uint32_t PWM_UT::m_ccr = 0;

#endif /* UNIT_TEST */

#if (PWM_TIM == 1)
typedef PWM_TIM1 PWM_TIM_HW;
#elif (PWM_TIM == 3)
typedef PWM_TIM3 PWM_TIM_HW;
#else
typedef PWM_UT PWM_TIM_HW;
#endif

void half_bridge_init(int freq_kHz, int deadtime_ns, float min_duty, float max_duty)
{
    // timers are counting at system clock
    // we are using a mode which counts up and down in each period
    // so we need half the impluses as resolution for the given frequency.
    _pwm_resolution = SystemCoreClock / (freq_kHz * 1000) / 2;

    // (clocks per ms * deadtime in ns) / 1000 == (clocks per ms * deadtime in ms)
    // although the C operator precedence does the "right thing" to allow deadtime_ns < 1000 to
    // be handled nicely, parentheses make this more explicit
    _deadtime_clocks = ((SystemCoreClock / (1000000)) * deadtime_ns) / 1000;

    // set PWM frequency and resolution (in fact half the resolution)
    PWM_TIM_HW::_init_registers(_pwm_resolution);

    _min_duty = min_duty * _pwm_resolution;
    _max_duty = max_duty * _pwm_resolution;

    half_bridge_set_duty_cycle(max_duty);      // init with allowed value

    _enabled = false;
}

static void _half_bridge_set_duty_cycle(int duty_target)
{
    // protection against wrong settings which could destroy the hardware
    if (duty_target < _min_duty) {
        duty_target = _min_duty;
    }
    else if (duty_target > _max_duty) {
        duty_target = _max_duty;
    }

    PWM_TIM_HW::set_ccr(duty_target);
}

void half_bridge_set_duty_cycle(float duty)
{
    _half_bridge_set_duty_cycle(_pwm_resolution * duty);
}

void half_bridge_duty_cycle_step(int delta)
{
    _half_bridge_set_duty_cycle(PWM_TIM_HW::get_ccr() + delta);
}

float half_bridge_get_duty_cycle()
{
    return (float)(PWM_TIM_HW::get_ccr()) / _pwm_resolution;
}

void half_bridge_start(float pwm_duty)
{
    half_bridge_set_duty_cycle(pwm_duty);

    PWM_TIM_HW::start();
    _enabled = true;
}

void half_bridge_stop()
{
    PWM_TIM_HW::stop();
    _enabled = false;
}

bool half_bridge_enabled()
{
    return _enabled;
}
