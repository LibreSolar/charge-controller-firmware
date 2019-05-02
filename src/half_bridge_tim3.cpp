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


/* Generates PWM signal on PB0 (high-side) and PB1 (low-side) using timer TIM3
 */

#include "half_bridge.h"
#include "pcb.h"
#include "config.h"

// timer used for PWM generation has to be globally selected for the used PCB
#if (PWM_TIM == 3)

static int _pwm_resolution;
static float _min_duty;
static float _max_duty;
static uint8_t _deadtime_clocks;

static bool _enabled;

void _init_registers(int freq_kHz)
{
    // Enable peripheral clock of GPIOB
#if defined(STM32F0)
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
#elif defined(STM32L0)
    RCC->IOPENR |= RCC_IOPENR_IOPBEN;
#endif

    // Enable TIM3 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    // Select alternate function mode on PB0 and PB1 (first bit _1 = 1, second bit _0 = 0)
#if defined(STM32F0)
    GPIOB->MODER = (GPIOB->MODER & ~(GPIO_MODER_MODER0)) | GPIO_MODER_MODER0_1;
    GPIOB->MODER = (GPIOB->MODER & ~(GPIO_MODER_MODER1)) | GPIO_MODER_MODER1_1;
#elif defined(STM32L0)
    GPIOB->MODER = (GPIOB->MODER & ~(GPIO_MODER_MODE0)) | GPIO_MODER_MODE0_1;
    GPIOB->MODER = (GPIOB->MODER & ~(GPIO_MODER_MODE1)) | GPIO_MODER_MODE1_1;
#endif

#if defined(STM32F0)
    // Select AF1 on PB0 and PB1
    GPIOB->AFR[0] |= 0x1 << GPIO_AFRL_AFSEL0_Pos;
    GPIOB->AFR[0] |= 0x1 << GPIO_AFRL_AFSEL1_Pos;
#elif defined(STM32L0)
    // Select AF2 on PB0 and PB1
    GPIOB->AFR[0] |= 0x2 << GPIO_AFRL_AFRL0_Pos;
    GPIOB->AFR[0] |= 0x2 << GPIO_AFRL_AFRL1_Pos;
#endif

    // No prescaler --> timer frequency = 32/48 MHz (for L0/F0)
    TIM3->PSC = 0;

    // Capture/Compare Mode Register 1
    // OCxM = 110: Select PWM mode 1 on OCx
    // OCxPE = 1:  Enable preload register on OCx (reset value)
    TIM3->CCMR2 |= TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3PE;
    TIM3->CCMR2 |= TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4PE;

    // Capture/Compare Enable Register
    // CCxP: Active high polarity on OCx (default = 0)
    TIM3->CCER &= ~(TIM_CCER_CC3P);     // PB0 / TIM3_CH3: high-side
    TIM3->CCER |= TIM_CCER_CC4P;        // PB1 / TIM3_CH4: low-side

    // Control Register 1
    // TIM_CR1_CMS = 01: Select center-aligned mode 1
    // TIM_CR1_CEN =  1: Counter enable
    TIM3->CR1 |= TIM_CR1_CMS_0 | TIM_CR1_CEN;

    // Control Register 2
    // OIS1 = OIS1N = 0: Output Idle State is set to off
    //TIM3->CR2 |= ;

    // Force update generation (UG = 1)
    TIM3->EGR |= TIM_EGR_UG;

    // set PWM frequency and resolution
    _pwm_resolution = SystemCoreClock / (freq_kHz * 1000);

    // Auto Reload Register
    // center-aligned mode --> divide resolution by 2
    TIM3->ARR = _pwm_resolution / 2;
}

void half_bridge_init(int freq_kHz, int deadtime_ns, float min_duty, float max_duty)
{
    _init_registers(freq_kHz);

    _deadtime_clocks = (SystemCoreClock / 1000 / 1000) * deadtime_ns / 1000;

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

    TIM3->CCR3 = _pwm_resolution / 2 * duty_target; // high-side
    TIM3->CCR4 = _pwm_resolution / 2 * duty_target + _deadtime_clocks; // low-side
}

void half_bridge_duty_cycle_step(int delta)
{
    float duty_target;
    duty_target = (float)(TIM3->CCR3 + delta) / (_pwm_resolution / 2);

    half_bridge_set_duty_cycle(duty_target);
}

float half_bridge_get_duty_cycle() {
    return (float)(TIM3->CCR3) / (_pwm_resolution / 2);;
}

void half_bridge_start(float pwm_duty)
{
    half_bridge_set_duty_cycle(pwm_duty);

#ifndef PIL_TESTING
    // Capture/Compare Enable Register
    // CCxE = 1: Enable the output on OCx
    // CCxP = 0: Active high polarity on OCx (default)
    // CCxNE = 1: Enable the output on OC1N
    // CCxNP = 0: Active high polarity on OC1N (default)
    TIM3->CCER |= TIM_CCER_CC3E;
    TIM3->CCER |= TIM_CCER_CC4E;
#endif

    _enabled = true;
}

void half_bridge_stop()
{
    TIM3->CCER &= ~(TIM_CCER_CC3E);
    TIM3->CCER &= ~(TIM_CCER_CC4E);

    _enabled = false;
}

bool half_bridge_enabled()
{
    return _enabled;
}

#endif /* PWM_TIM */
