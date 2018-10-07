/* mbed library for half bridge driver PWM generation
 * Copyright (c) 2016-2017 Martin Jäger (www.libre.solar)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UNIT_TEST

#include "half_bridge.h"
#include "config.h"

#if defined(STM32F0) && (PWM_TIM == 1)

static int _pwm_resolution;
static float _min_duty;
static float _max_duty;

static bool _enabled;

void _init_registers(int freq_kHz, int deadtime_ns)
{
    // Enable peripheral clock of GPIOA and GPIOB
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;

    // Enable TIM1 clock
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;

    // Select alternate function mode on PA8 und PB13
    GPIOA->MODER = (GPIOA->MODER & ~(GPIO_MODER_MODER8)) | GPIO_MODER_MODER8_1;
    GPIOB->MODER = (GPIOB->MODER & ~(GPIO_MODER_MODER13)) | GPIO_MODER_MODER13_1;

    // Select AF2 on PA8 (TIM1_CH1)
    GPIOA->AFR[1] |= 0x2 << ((8 - 8)*4);   // AFR[1] for pins 8-15

    // Select AF2 on PB13 (TIM1_CH1N)
    GPIOB->AFR[1] |= 0x2 << ((13 - 8)*4);  // AFR[1] for pins 8-15

    // No prescaler --> timer frequency = 48 MHz
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
    TIM1->CCER |= TIM_CCER_CC1E | TIM_CCER_CC1NE;// | TIM_CCER_CC1NP;

    // Control Register 1
    // TIM_CR1_CMS = 01: Select center-aligned mode 1
    // TIM_CR1_CEN =  1: Counter enable
    TIM1->CR1 |= TIM_CR1_CMS_0 | TIM_CR1_CEN;

    // Control Register 2
    // OIS1 = OIS1N = 0: Output Idle State is set to off
    //TIM1->CR2 |= ;

    // Force update generation (UG = 1)
    TIM1->EGR |= TIM_EGR_UG;

    // set PWM frequency and resolution
    _pwm_resolution = SystemCoreClock / (freq_kHz * 1000);

    // Auto Reload Register
    // center-aligned mode --> divide resolution by 2
    TIM1->ARR = _pwm_resolution / 2;

    uint8_t deadtime_clocks = (SystemCoreClock / 1000 / 1000) * deadtime_ns / 1000;

    // Break and Dead-Time Register
    // MOE  = 1: Main output enable
    // OSSR = 0: Off-state selection for Run mode -> OC/OCN = 0
    // OSSI = 0: Off-state selection for Idle mode -> OC/OCN = 0
    TIM1->BDTR |= (deadtime_clocks & (uint32_t)0x7F); // ensure that only the last 7 bits are changed

    // Lock Break and Dead-Time Register
    // TODO: does not work properly... maybe HW bug?
    TIM1->BDTR |= TIM_BDTR_LOCK_1 | TIM_BDTR_LOCK_0;
}

void half_bridge_init(int freq_kHz, int deadtime_ns, float min_duty, float max_duty)
{
    _init_registers(freq_kHz, deadtime_ns);

    _min_duty = min_duty;
    _max_duty = max_duty;
    half_bridge_set_duty_cycle(_max_duty);      // init with allowed value

    _enabled = false;
}

void half_bridge_set_duty_cycle(float duty)
{
    float duty_target;
    // protection against wrong settings which could destroy the hardware
    if (duty < _min_duty) {
        duty_target = _min_duty;
    }
    else if (duty > _max_duty) {
        duty_target = _max_duty;
    }
    else {
        duty_target = duty;
    }

    TIM1->CCR1 = _pwm_resolution / 2 * duty_target;
}

void half_bridge_duty_cycle_step(int delta)
{
    float duty_target;
    duty_target = (float)(TIM1->CCR1 + delta) / (_pwm_resolution / 2);

    // protection against wrong settings which could destroy the hardware
    if (duty_target < _min_duty) {
        half_bridge_set_duty_cycle(_min_duty);
    }
    else if (duty_target > _max_duty) {
        half_bridge_set_duty_cycle(_max_duty);
    }
    else {
        TIM1->CCR1 = TIM1->CCR1 + delta;
    }
}

float half_bridge_get_duty_cycle()
{
    return (float)(TIM1->CCR1) / (_pwm_resolution / 2);;
}

void half_bridge_start(float pwm_duty)
{
    half_bridge_set_duty_cycle(pwm_duty);

    // Break and Dead-Time Register
    // MOE  = 1: Main output enable
    TIM1->BDTR |= TIM_BDTR_MOE;

    _enabled = true;
}

void half_bridge_stop()
{
    // Break and Dead-Time Register
    // MOE  = 1: Main output enable
    TIM1->BDTR &= ~(TIM_BDTR_MOE);

    _enabled = false;
}

bool half_bridge_enabled()
{
    return _enabled;
}

#endif /* PWM_TIM */

#endif /* UNIT_TEST */