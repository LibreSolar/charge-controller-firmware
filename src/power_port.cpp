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

void PowerPort::init_solar(float max_abs_current)
{
    dis_current_limit = -max_abs_current;
    chg_current_limit = 0;     // no current towards solar panel allowed

    // also initialize the connected bus
    bus->dis_voltage_start = 16.0;
    bus->dis_voltage_stop = 14.0;
}

void PowerPort::init_nanogrid()
{
    dis_current_limit = -5.0;
    dis_droop_res = 0.1;            // 1 Ohm means 1V change of target voltage per amp

    chg_current_limit = 5.0;
    chg_droop_res = 0.1;           // 1 Ohm means 1V change of target voltage per amp

    // also initialize the connected bus
    bus->dis_voltage_start = 30.0;       // starting buck mode above this point
    bus->dis_voltage_stop = 20.0;        // stopping buck mode below this point

    bus->chg_voltage_target = 28.0;     // starting idle mode above this point
    bus->chg_voltage_min = 10.0;
}

// must be called exactly once per second, otherwise energy calculation gets wrong
void PowerPort::energy_balance()
{
    // remark: timespan = 1s, so no multiplication with time necessary for energy calculation
    if (current >= 0) {
        chg_energy_Wh += bus->voltage * current / 3600.0;
    }
    else {
        dis_energy_Wh -= bus->voltage * current / 3600.0;
    }

}
