/* mbed library for MPPT buck converter
 * Copyright (c) 2016 Martin Jäger (www.libre.solar)
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

#include "BuckConverter.h"

BuckConverter::BuckConverter(int freq_kHz)
{
    init_registers();
    frequency_kHz(freq_kHz);
    last_time_CV_reset();

    _pwm_delta = 1;
}

void BuckConverter::update(int input_voltage_mV, int output_voltage_mV, int output_current_mA) {

    int output_power_mW = output_voltage_mV * output_current_mA / 1000;

    //printf("P=%d P_prev=%d Vi=%d Vo=%d Io=%d\n", output_power_mW, _output_power_prev_mW, input_voltage_mV, output_voltage_mV, output_current_mA);

    if (output_voltage_mV > _max_voltage_mV) {
        // increase input voltage to lower output voltage
        duty_cycle_step(-1);
        _time_voltage_limit_reached = time(NULL);
        //printf("output_voltage_mV = %d, _max_voltage_mV = %d\n", output_voltage_mV, _max_voltage_mV);
        fflush(stdout);
    }
    else if (output_current_mA > _max_current_mA) {
        // increase input voltage to decrease current
        duty_cycle_step(-1);
    }
    else {
        // start MPPT
        if (_output_power_prev_mW > output_power_mW) {
            _pwm_delta = -_pwm_delta;
        }
        duty_cycle_step(_pwm_delta);
    }

    _output_power_prev_mW = output_power_mW;
}

void BuckConverter::init_registers() {

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
}

void BuckConverter::set_voltage_limit(int voltage_mV) {
    _max_voltage_mV = voltage_mV;
}

void BuckConverter::set_current_limit(int current_mA) {
    _max_current_mA = current_mA;
}

time_t BuckConverter::last_time_CV() {
    return _time_voltage_limit_reached;
}

void BuckConverter::last_time_CV_reset() {
    _time_voltage_limit_reached = 0x7FFFFFFF;   // maximum value
}

void BuckConverter::frequency_kHz(int freq) {

    _pwm_resolution = SystemCoreClock / (freq * 1000);

    // Auto Reload Register
    // center-aligned mode --> divide resolution by 2
    TIM1->ARR = _pwm_resolution / 2;
}


void BuckConverter::set_duty_cycle(float duty) {

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

void BuckConverter::duty_cycle_step(int delta)
{
    float duty_target;
    duty_target = (float)(TIM1->CCR1 + delta) / (_pwm_resolution / 2);

    // protection against wrong settings which could destroy the hardware
    if (duty_target < _min_duty) {
        set_duty_cycle(_min_duty);
    }
    else if (duty_target > _max_duty) {
        set_duty_cycle(_max_duty);
    }
    else {
        TIM1->CCR1 = TIM1->CCR1 + delta;
    }
}


float BuckConverter::get_duty_cycle() {
    return (float)(TIM1->CCR1) / (_pwm_resolution / 2);;
}


void BuckConverter::deadtime_ns(int deadtime) {

    uint8_t deadtime_clocks = (SystemCoreClock / 1000 / 1000) * deadtime / 1000;

    // Break and Dead-Time Register
    // MOE  = 1: Main output enable
    // OSSR = 0: Off-state selection for Run mode -> OC/OCN = 0
    // OSSI = 0: Off-state selection for Idle mode -> OC/OCN = 0
    TIM1->BDTR |= (deadtime_clocks & (uint32_t)0x7F); // ensure that only the last 7 bits are changed
}


void BuckConverter::start(float pwm_duty) {
    set_duty_cycle(pwm_duty);

    // Break and Dead-Time Register
    // MOE  = 1: Main output enable
    TIM1->BDTR |= TIM_BDTR_MOE;
}


void BuckConverter::stop() {

    // Break and Dead-Time Register
    // MOE  = 1: Main output enable
    TIM1->BDTR &= ~(TIM_BDTR_MOE);
}


void BuckConverter::lock_settings() {

    // TODO: does not work properly... maybe HW bug?

    // Break and Dead-Time Register
    TIM1->BDTR |= TIM_BDTR_LOCK_1 | TIM_BDTR_LOCK_0;
}

void BuckConverter::duty_cycle_limits(float min_duty, float max_duty) {

    _min_duty = min_duty;
    _max_duty = max_duty;

    // adjust set value to new limits
    if (get_duty_cycle() < _min_duty) {
        set_duty_cycle(_min_duty);
    }
    else if (get_duty_cycle() > _max_duty) {
        set_duty_cycle(_max_duty);
    }
}
