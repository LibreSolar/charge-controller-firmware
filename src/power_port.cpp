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
    dis_current_limit = -50;    // derating based on max. DC/DC or PWM switch current only
    chg_current_limit = 0;      // no current towards solar panel allowed

    // also initialize the connected bus
    dis_voltage_start = 16.0;
    dis_voltage_stop = 14.0;
}

void PowerPort::init_nanogrid()
{
    dis_current_limit = -5.0;
    dis_droop_res = 0.1;            // 1 Ohm means 1V change of target voltage per amp

    chg_current_limit = 5.0;
    chg_droop_res = 0.1;           // 1 Ohm means 1V change of target voltage per amp

    // also initialize the connected bus
    dis_voltage_start = 30.0;       // starting buck mode above this point
    dis_voltage_stop = 20.0;        // stopping buck mode below this point

    chg_voltage_target = 28.0;     // starting idle mode above this point
    chg_voltage_min = 10.0;
}

// must be called exactly once per second, otherwise energy calculation gets wrong
void PowerPort::energy_balance()
{
    // remark: timespan = 1s, so no multiplication with time necessary for energy calculation
    if (current >= 0) {
        chg_energy_Wh += voltage * current / 3600.0;
    }
    else {
        dis_energy_Wh -= voltage * current / 3600.0;
    }
}

void PowerPort::pass_voltage_targets(PowerPort *port)
{
    // droop resistance added only to limits where current actually flows
    port->chg_voltage_min = chg_voltage_min;
    port->chg_voltage_target = chg_voltage_target + chg_droop_res * current;
    port->dis_voltage_start = dis_voltage_start + chg_droop_res * current;
    port->dis_voltage_stop = dis_voltage_stop + chg_droop_res * current;
}

void ports_update_current_limits(PowerPort *p_dcdc, PowerPort *p_bat, PowerPort *p_load)
{
    p_dcdc->chg_current_limit = p_bat->chg_current_limit - p_bat->current + p_load->current;

    // this is a bit tricky... in theory, we could add the available DC/DC current to keep the
    // load switched on as long as solar power is available, even if the battery is empty. This
    // needs a fast point-of-load (PoL) control of the DC/DC, which is not possible (yet).
    p_dcdc->dis_current_limit = p_bat->dis_current_limit - p_bat->current;
}
