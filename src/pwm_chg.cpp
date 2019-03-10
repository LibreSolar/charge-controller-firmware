/* LibreSolar MPPT charge controller firmware
 * Copyright (c) 2016-2018 Martin Jäger (www.libre.solar)
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

#include "pwm_chg.h"
#include "config.h"
#include "pcb.h"

#include <time.h>       // for time(NULL) function
#include <math.h>       // for fabs function


// timer used for PWM generation has to be globally selected for the used PCB
#if (PWM_TIM == 3)

static int _pwm_resolution;

static bool _enabled;

void pwm_chg_init_registers(int freq_Hz)
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

void pwm_chg_set_duty_cycle(float duty)
{
    TIM3->CCR4 = _pwm_resolution * duty;
}

void pwm_chg_duty_cycle_step(int delta)
{
    if ((int)TIM3->CCR4 + delta <= (_pwm_resolution + 1) && (int)TIM3->CCR4 + delta >= 0) {
        TIM3->CCR4 += delta;
    }
}

float pwm_chg_get_duty_cycle() {
    return (float)(TIM3->CCR4) / (_pwm_resolution);;
}

void pwm_chg_start(float pwm_duty)
{
    pwm_chg_set_duty_cycle(pwm_duty);

    // Capture/Compare Enable Register
    // CCxE = 1: Enable the output on OCx
    // CCxP = 0: Active high polarity on OCx (default)
    // CCxNE = 1: Enable the output on OC1N
    // CCxNP = 0: Active high polarity on OC1N (default)
    TIM3->CCER |= TIM_CCER_CC4E;

    _enabled = true;
}

void pwm_chg_stop()
{
    TIM3->CCER &= ~(TIM_CCER_CC4E);

    _enabled = false;
}

void pwm_chg_init(pwm_chg_t *pwm_chg)
{
    pwm_chg->solar_current_max = DCDC_CURRENT_MAX;
    pwm_chg->solar_current_min = 0.05;

    //pwm_chg->pwm_delta = 1;
    pwm_chg->off_timestamp = -10000;              // start immediately

    // calibration parameters
    //pwm_chg->offset_voltage_start = 3.0;     // V  charging switched on if Vsolar > Vbat + offset
    //pwm_chg->offset_voltage_stop = 1.0;      // V  charging switched off if Vsolar < Vbat + offset
    pwm_chg->restart_interval = 60;          // s  When should we retry to start charging after low solar power cut-off?

    pwm_chg_init_registers(20);

    _enabled = false;
}

void pwm_chg_control(pwm_chg_t *pwm_chg, power_port_t *solar_port, power_port_t *bat_port)
{
    // for testing
    //solar_port->voltage_input_start = 14.0;
    //solar_port->voltage_input_stop = 13.0;

    //printf("P: %.2f, P_prev: %.2f, v_in: %.2f, v_out: %.2f, i_in: %.2f, i_out: %.2f, i_max: %.2f, PWM: %.1f, chg_en: %d",
    //     dcdc_power_new, dcdc->power, solar_port->voltage, bat_port->voltage, solar_port->current, bat_port->current,
    //     bat_port->current_output_max, half_bridge_get_duty_cycle() * 100.0, bat_port->output_allowed);

    if (_enabled) {
        if (bat_port->output_allowed == false || solar_port->input_allowed == false
            || (solar_port->voltage < solar_port->voltage_input_stop && bat_port->current < 0.1))
        {
            pwm_chg_stop();
        }
        else if (bat_port->voltage > (bat_port->voltage_output_target - bat_port->droop_resistance * bat_port->current)    // output voltage above target
            || bat_port->current > bat_port->current_output_max         // output current limit exceeded
            //|| (solar_port->voltage < (solar_port->voltage_input_start - solar_port->droop_resistance * solar_port->current) && bat_port->current > 0.1)        // input voltage below limit
            || solar_port->current < solar_port->current_input_max      // input current (negative signs) above limit
            /*|| fabs(dcdc->ls_current) > dcdc->ls_current_max            // current above hardware maximum
            || dcdc->temp_mosfets > 80*/)                               // temperature limits exceeded
        {
            pwm_chg_duty_cycle_step(-1);
        }
        else {
            pwm_chg_duty_cycle_step(1);
        }
    }
    else {
        if (bat_port->output_allowed == true
            && bat_port->voltage < bat_port->voltage_output_target
            && bat_port->voltage > bat_port->voltage_output_min
            && solar_port->input_allowed == true
            && solar_port->voltage > solar_port->voltage_input_start
            && time(NULL) > (pwm_chg->off_timestamp + pwm_chg->restart_interval))
        {
            pwm_chg_start(1);
            printf("PWM charger start.\n");
        }
    }
}

#else

// dummy functions for non-PWM charge controllers
void pwm_chg_init(pwm_chg_t *pwm_chg) {}
void pwm_chg_control(pwm_chg_t *pwm_chg, power_port_t *solar_port, power_port_t *bat_port) {}
void pwm_chg_duty_cycle_step(int delta) {}

#endif