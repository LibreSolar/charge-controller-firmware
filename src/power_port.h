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

#ifndef POWER_PORT_H
#define POWER_PORT_H

/** @file
 *
 * @brief Definition of power ports of a DC/DC converter (high-side or low-side terminal)
 */

#include <stdbool.h>
#include "battery.h"

/** Power port type
 *
 * Saves current target settings of either high-side or low-side port of a
 * DC/DC converter. In this way, e.g. a battery can be configured to be
 * connected to high or low side of DC/DC converter w/o having to rewrite
 * the control algorithm.
 */
typedef struct {
    float voltage;                  // actual measurements
    float current;

    float voltage_output_target;    // target voltage if port is configured as output
    float droop_resistance;         // v_target = v_out_max - r_droop * current
    float voltage_output_min;       // minimum voltage to allow current output (necessary
                                    // to prevent charging of deep-discharged Li-ion batteries)

    // minimum voltage to allow current input (= discharging of batteries)
    float voltage_input_start;      // starting point for discharging of batteries (load reconnect)
    float voltage_input_stop;       // absolute minimum = load disconnect for batteries

    float current_output_max;       // charging direction for battery port
    float current_input_max;        // discharging direction for battery port: must be negative value !!!

    bool output_allowed;            // charging direction for battery port
    bool input_allowed;             // discharging direction for battery port
} power_port_t;

void power_port_init_bat(power_port_t *port, battery_conf_t *bat);

void power_port_init_solar(power_port_t *port);

void power_port_init_nanogrid(power_port_t *port);

#endif /* POWER_PORT_H */
