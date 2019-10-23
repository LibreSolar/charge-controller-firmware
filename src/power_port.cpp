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

void PowerPort::init_solar()
{
    neg_current_limit = -50;    // derating based on max. DC/DC or PWM switch current only
    pos_current_limit = 0;      // no current towards solar panel allowed

    // also initialize the connected bus
    src_voltage_start = 16.0;
    src_voltage_stop = 14.0;
}

void PowerPort::init_nanogrid()
{
    neg_current_limit = -5.0;
    neg_droop_res = 0.1;            // 1 Ohm means 1V change of target voltage per amp

    pos_current_limit = 5.0;
    pos_droop_res = 0.1;           // 1 Ohm means 1V change of target voltage per amp

    // also initialize the connected bus
    src_voltage_start = 30.0;       // starting buck mode above this point
    src_voltage_stop = 20.0;        // stopping buck mode below this point

    sink_voltage_max = 28.0;     // starting idle mode above this point
    sink_voltage_min = 10.0;
}

// must be called exactly once per second, otherwise energy calculation gets wrong
void PowerPort::energy_balance()
{
    // remark: timespan = 1s, so no multiplication with time necessary for energy calculation
    if (current >= 0) {
        pos_energy_Wh += voltage * current / 3600.0;
    }
    else {
        neg_energy_Wh -= voltage * current / 3600.0;
    }
}

void PowerPort::pass_voltage_targets(PowerPort *port)
{
    // droop resistance added only to limits where current actually flows
    port->sink_voltage_min = sink_voltage_min;
    port->sink_voltage_max = sink_voltage_max + pos_droop_res * current;
    port->src_voltage_start = src_voltage_start + pos_droop_res * current;
    port->src_voltage_stop = src_voltage_stop + pos_droop_res * current;
}

void ports_update_current_limits(PowerPort *p_dcdc, PowerPort *p_bat, PowerPort *p_load)
{
    p_dcdc->pos_current_limit = p_bat->pos_current_limit - p_bat->current + p_load->current;

    // this is a bit tricky... in theory, we could add the available DC/DC current to keep the
    // load switched on as long as solar power is available, even if the battery is empty. This
    // needs a fast point-of-load (PoL) control of the DC/DC, which is not possible (yet).
    p_dcdc->neg_current_limit = p_bat->neg_current_limit - p_bat->current;
}
