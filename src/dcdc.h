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

#ifndef __DCDC_H_
#define __DCDC_H_

#include <stdbool.h>
#include "battery.h"
#include "power_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* DC/DC basic operation mode
 *
 * Defines which type of device is connected to the high side and low side ports
 */
enum dcdc_control_mode
{
    MODE_MPPT_BUCK,     // solar panel at high side port, battery / load at low side port (typical MPPT)
    MODE_MPPT_BOOST,    // battery at high side port, solar panel at low side (e.g. e-bike charging)
    MODE_NANOGRID       // accept input power (if available and need for charging) or provide output power
                        // (if no other power source on the grid and battery charged) on the high side port
                        // and dis/charge battery on the low side port, battery voltage must be lower than
                        // nano grid voltage.
};

/* DC/DC type
 *
 * Contains all data belonging to the DC/DC sub-component of the PCB, incl.
 * actual measurements and calibration parameters.
 */
typedef struct {
    dcdc_control_mode mode;

    // actual measurements
    float ls_current;           // inductor current
    float temp_mosfets;

    // current state
    float power;                // power at low-side (calculated by dcdc controller)
    int pwm_delta;              // direction of PWM change for MPPT
    int off_timestamp;          // time when DC/DC was switched off last time

    // maximum allowed values
    float ls_current_max;       // PCB inductor maximum
    float ls_current_min;       // --> if lower, charger is switched off
    float hs_voltage_max;
    float ls_voltage_max;

    // calibration parameters
    //float offset_voltage_start;  // V  charging switched on if Vsolar > Vbat + offset
    //float offset_voltage_stop;   // V  charging switched off if Vsolar < Vbat + offset
    int restart_interval;   // s    --> when should we retry to start charging after low solar power cut-off?
} dcdc_t;


/* Initialize DC/DC and DC/DC port structs
 *
 * See http://libre.solar/docs/dcdc_control for detailed information
 */
void dcdc_init(dcdc_t *dcdc);

void dcdc_control(dcdc_t *dcdc, power_port_t *high_side, power_port_t *low_side);

#ifdef __cplusplus
}
#endif

#endif // DCDC_H
