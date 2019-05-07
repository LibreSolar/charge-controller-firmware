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

#ifndef DC_BUS_H
#define DC_BUS_H

/** @file
 *
 * @brief Definition of power ports of a DC/DC converter (high-side or low-side terminal)
 */

#include <stdbool.h>
#include "battery.h"

/** DC bus type
 *
 * Saves current target settings of either high-side or low-side port of a
 * DC/DC converter. In this way, e.g. a battery can be configured to be
 * connected to high or low side of DC/DC converter w/o having to rewrite
 * the control algorithm.
 */
typedef struct {
    float voltage;                  ///< measured voltage
    float current;                  ///< sum of currents (positive sign = increasing voltage/charge of the bus)

    bool chg_allowed;               ///< charging direction for battery port
    float chg_voltage_target;       ///< target voltage if port is configured as output
    float chg_droop_res;            ///< v_target = v_out_max - r_droop_output * current
    float chg_voltage_min;          ///< minimum voltage to allow current output (necessary
                                    ///< to prevent charging of deep-discharged Li-ion batteries)
    float chg_current_max;          ///< charging direction for battery port

    bool dis_allowed;               ///< discharging direction for battery port
    float dis_voltage_start;        ///< minimum voltage to allow current input (= discharging of batteries),
                                    ///< starting point for discharging of batteries (load reconnect)
    float dis_voltage_stop;         ///< absolute minimum = load disconnect for batteries
    float dis_droop_res;            ///< v_stop = v_input_stop - r_droop_input * current
    float dis_current_max;          ///< discharging direction for battery port: must be negative value !!!

    float chg_energy_Wh;            ///< cumulated energy in charge direction since last counter reset (Wh)
    float dis_energy_Wh;            ///< cumulated energy in discharge direction since last counter reset (Wh)

} dc_bus_t;

/** Initialize dc bus for battery connection
 */
void dc_bus_init_bat(dc_bus_t *bus, battery_conf_t *bat);

/** Initialize dc bus for solar panel connection
 */
void dc_bus_init_solar(dc_bus_t *bus);

/** Initialize dc bus for nanogrid connection
 */
void dc_bus_init_nanogrid(dc_bus_t *bus);

/** Energy balance calculation for dc bus
 *
 * Must be called exactly once per second, otherwise energy calculation gets wrong.
 */
void dc_bus_energy_balance(dc_bus_t *bus);

#endif /* DC_BUS_H */
