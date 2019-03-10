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

#ifndef PWM_CHG_H
#define PWM_CHG_H

/** @file
 *
 * @brief PWM charger MOSFET switch control functions
 */

#include <stdbool.h>
#include "power_port.h"

/** PWM charger type
 *
 * Contains all data belonging to the DC/DC sub-component of the PCB, incl.
 * actual measurements and calibration parameters.
 */
typedef struct {
    // actual measurements
    float solar_current;
    float temp_mosfets;
    float solar_power;

    float solar_current_max;        // MOSFET maximum (continuous)
    float solar_current_min;        // --> if lower, charger is switched off

    int pwm_delta;                  // direction of PWM change
    int off_timestamp;              // time when charger was switched off last time

    // calibration parameters
    float offset_voltage_start;     // V  charging switched on if Vsolar > Vbat + offset
    float offset_voltage_stop;      // V  charging switched off if Vsolar < Vbat + offset
    int restart_interval;           // s  When should we retry to start charging after low solar power cut-off?
} pwm_chg_t;


void pwm_chg_init(pwm_chg_t *pwm_chg);

void pwm_chg_control(pwm_chg_t *pwm_chg, power_port_t *solar_port, power_port_t *bat_port);

void pwm_chg_duty_cycle_step(int delta);

#endif /* PWM_CHG_H */
