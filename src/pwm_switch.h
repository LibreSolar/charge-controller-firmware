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

#ifndef PWM_SWITCH_H
#define PWM_SWITCH_H

/** @file
 *
 * @brief PWM charger MOSFET switch control functions
 */

#include <stdbool.h>
#include "dc_bus.h"

/** PWM charger type
 *
 * Contains all data belonging to the PWM switching sub-component.
 */
typedef struct {
    bool enabled;                   ///< Can be used to disable the PWM power stage
    float offset_voltage_start;     ///< Offset voltage of solar panel vs. battery to start charging (V)
    int restart_interval;           ///< Interval to wait before retrying charging after low solar power cut-off (s)
    int off_timestamp;              ///< Time when charger was switched off last time
} PwmSwitch;

/** Initialization of PWM switch struct
 */
void pwm_switch_init(PwmSwitch *pwm_switch);

/** Main control function for the PWM switching algorithm
 *
 * @param pwm_switch PWM switch struct
 * @param solar_bus Solar connector DC bus struct
 * @param bat_bus Battery connector DC bus struct
 */
void pwm_switch_control(PwmSwitch *pwm_switch, DcBus *solar_bus, DcBus *bat_bus);

/** Read the on/off status of the PWM switch
 *
 * @returns true if enabled
 */
bool pwm_switch_enabled();

/** Read the currently set duty cycle
 *
 * @returns Duty cycle between 0.0 and 1.0
 */
float pwm_switch_get_duty_cycle();

#endif /* PWM_SWITCH_H */
