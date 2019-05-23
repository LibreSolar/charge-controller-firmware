/* LibreSolar charge controller firmware
 * Copyright (c) 2016-2019 Martin Jäger (www.libre.solar)
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

#include "pwm_switch.h"
#include "config.h"
#include "pcb.h"

#include <time.h>       // for time(NULL) function
#include <math.h>       // for fabs function


// timer used for PWM generation has to be globally selected for the used PCB
#if (PWM_TIM == 3) && defined(CHARGER_TYPE_PWM)

static int _pwm_resolution;

static bool _enabled;

void pwm_switch_init_registers(int freq_Hz)
{
    // Enable peripheral clock of GPIOB
    RCC->IOPENR |= RCC_IOPENR_IOPBEN;

    // Enable TIM3 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    // Select alternate function mode on PB1 (first bit _1 = 1, second bit _0 = 0)
    GPIOB->MODER = (GPIOB->MODER & ~(GPIO_MODER_MODE1)) | GPIO_MODER_MODE1_1;

    // Select AF2 on PB1
    GPIOB->AFR[0] |= 0x2 << GPIO_AFRL_AFRL1_Pos;

    // Set timer clock to 10 kHz
    TIM3->PSC = SystemCoreClock / 10000;

    // Capture/Compare Mode Register 1
    // OCxM = 110: Select PWM mode 1 on OCx
    // OCxPE = 1:  Enable preload register on OCx (reset value)
    TIM3->CCMR2 |= TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4PE;

    // Capture/Compare Enable Register
    // CCxP: Active high polarity on OCx (default = 0)
    TIM3->CCER &= ~(TIM_CCER_CC4P);     // PB1 / TIM3_CH4: high-side

    // Control Register 1
    // TIM_CR1_CEN =  1: Counter enable
    TIM3->CR1 |= TIM_CR1_CEN;   // edge-aligned mode

    // Force update generation (UG = 1)
    TIM3->EGR |= TIM_EGR_UG;

    // set PWM frequency and resolution
    _pwm_resolution = 10000 / freq_Hz;

    // Auto Reload Register
    TIM3->ARR = _pwm_resolution;
}

void pwm_switch_set_duty_cycle(float duty)
{
    TIM3->CCR4 = _pwm_resolution * duty;
}

void pwm_switch_duty_cycle_step(int delta)
{
    if ((int)TIM3->CCR4 + delta <= (_pwm_resolution + 1) && (int)TIM3->CCR4 + delta >= 0) {
        TIM3->CCR4 += delta;
    }
}

bool pwm_switch_enabled()
{
    return _enabled;
}

float pwm_switch_get_duty_cycle() {
    // TODO: check why we might get > 100
    return (float)(TIM3->CCR4) / (_pwm_resolution);
}

void pwm_switch_start(float pwm_duty)
{
    pwm_switch_set_duty_cycle(pwm_duty);

    // Capture/Compare Enable Register
    // CCxE = 1: Enable the output on OCx
    // CCxP = 0: Active high polarity on OCx (default)
    // CCxNE = 1: Enable the output on OC1N
    // CCxNP = 0: Active high polarity on OC1N (default)
    TIM3->CCER |= TIM_CCER_CC4E;

    _enabled = true;
}

void pwm_switch_stop()
{
    TIM3->CCER &= ~(TIM_CCER_CC4E);

    _enabled = false;
}

void pwm_switch_init(pwm_switch_t *pwm_switch)
{
    pwm_switch->solar_current_max = DCDC_CURRENT_MAX;
    pwm_switch->solar_current_min = 0.05;

    //pwm_switch->pwm_delta = 1;
    pwm_switch->off_timestamp = -10000;              // start immediately

    // calibration parameters
    //pwm_switch->offset_voltage_start = 3.0;     // V  charging switched on if Vsolar > Vbat + offset
    //pwm_switch->offset_voltage_stop = 1.0;      // V  charging switched off if Vsolar < Vbat + offset
    pwm_switch->restart_interval = 60;          // s  When should we retry to start charging after low solar power cut-off?

    pwm_switch_init_registers(20);

    pwm_switch->enabled = true;     // enable charging in general
    _enabled = false;               // still disable actual switch
}

void pwm_switch_control(pwm_switch_t *pwm_switch, dc_bus_t *solar_port, dc_bus_t *bat_port)
{
    if (_enabled) {
        if (bat_port->chg_allowed == false || solar_port->dis_allowed == false
            || (solar_port->voltage < solar_port->dis_voltage_stop && solar_port->current > 0)
            || pwm_switch->enabled == false)
        {
            pwm_switch_stop();
            pwm_switch->off_timestamp = time(NULL);
            printf("PWM charger stop.\n");
        }
        else if (bat_port->voltage > (bat_port->chg_voltage_target - bat_port->chg_droop_res * bat_port->current)    // output voltage above target
            || bat_port->current > bat_port->chg_current_max         // output current limit exceeded
            || solar_port->current < solar_port->dis_current_max)    // input current (negative signs) above limit
        {
            if (pwm_switch_get_duty_cycle() > 0.9)
                pwm_switch_duty_cycle_step(-1);
        }
        else {
            pwm_switch_duty_cycle_step(1);
        }
    }
    else {
        if (bat_port->chg_allowed == true
            && bat_port->voltage < bat_port->chg_voltage_target
            && bat_port->voltage > bat_port->chg_voltage_min
            && solar_port->dis_allowed == true
            && solar_port->voltage > solar_port->dis_voltage_start
            && time(NULL) > (pwm_switch->off_timestamp + pwm_switch->restart_interval)
            && pwm_switch->enabled == true)
        {
            pwm_switch_start(1);
            printf("PWM charger start.\n");
        }
    }
}

#else

// dummy functions for non-PWM charge controllers
void pwm_switch_init(pwm_switch_t *pwm_switch) {}
void pwm_switch_control(pwm_switch_t *pwm_switch, dc_bus_t *solar_port, dc_bus_t *bat_port) {}
void pwm_switch_duty_cycle_step(int delta) {}
bool pwm_switch_enabled() {return false;}

#endif

#endif /* UNIT_TEST */
