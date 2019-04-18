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

#include "power_port.h"

void power_port_init_bat(power_port_t *port, battery_conf_t *bat)
{
    port->input_allowed = true;     // discharging allowed
    port->output_allowed = true;    // charging allowed

    port->voltage_input_start = bat->voltage_load_reconnect;
    port->voltage_input_stop = bat->voltage_load_disconnect;
    port->current_input_max = -bat->charge_current_max;          // TODO: discharge current
    port->droop_res_input = -(bat->internal_resistance + -bat->wire_resistance);  // negative sign for compensation of actual resistance

    port->voltage_output_target = bat->voltage_max;
    port->voltage_output_min = bat->voltage_absolute_min;
    port->current_output_max = bat->charge_current_max;
    port->droop_res_output = -bat->wire_resistance;             // negative sign for compensation of actual resistance
}

void power_port_init_solar(power_port_t *port)
{
    port->input_allowed = true;         // PV panel may provide power to solar input of DC/DC
    port->output_allowed = false;

    port->voltage_input_start = 16.0;
    port->voltage_input_stop = 14.0;
    port->current_input_max = -18.0;
}

void power_port_init_nanogrid(power_port_t *port)
{
    port->input_allowed = true;
    port->output_allowed = true;

    port->voltage_input_start = 30.0;       // starting buck mode above this point
    port->voltage_input_stop = 20.0;        // stopping buck mode below this point
    port->current_input_max = -5.0;
    port->droop_res_input = 0.1;            // 1 Ohm means 1V change of target voltage per amp

    port->voltage_output_target = 28.0;     // starting idle mode above this point
    port->current_output_max = 5.0;
    port->voltage_output_min = 10.0;
    port->droop_res_output = 0.1;           // 1 Ohm means 1V change of target voltage per amp
}
