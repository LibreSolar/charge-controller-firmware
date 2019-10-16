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
 * @brief Definition of power converter terminals (e.g. high voltage or low voltage side of a DC/DC)
 */

#include <stdbool.h>

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
    float power;                    ///< multiplication of power & voltage

    float chg_voltage_target;       ///< target voltage if port is configured as output
    float chg_droop_res;            ///< v_target = v_out_max - r_droop_output * current
    float chg_voltage_min;          ///< minimum voltage to allow current output (necessary
                                    ///< to prevent charging of deep-discharged Li-ion batteries)
    float chg_current_limit;        ///< current charging the bus, i.e. increasing its voltage
                                    ///< (equals charging direction for battery terminal)

    float dis_voltage_start;        ///< minimum voltage to allow current input (= discharging of batteries),
                                    ///< starting point for discharging of batteries (load reconnect)
    float dis_voltage_stop;         ///< absolute minimum = load disconnect for batteries
    float dis_droop_res;            ///< v_stop = v_input_stop - r_droop_input * current
    float dis_current_limit;        ///< current which discharges the bus, i.e. decreases its
                                    ///< voltage, value must be negative!!!

    float chg_energy_Wh;            ///< cumulated energy in charge direction since last counter reset (Wh)
    float dis_energy_Wh;            ///< cumulated energy in discharge direction since last counter reset (Wh)

} DcBus;

/** Initialize dc bus for solar panel connection
 *
 * @param bus The DC bus struct to initialize
 * @param max_abs_current Maximum input current allowed by PCB (as a positive value)
 */
void dc_bus_init_solar(DcBus *bus, float max_abs_current);

/** Initialize dc bus for nanogrid connection
 */
void dc_bus_init_nanogrid(DcBus *bus);

/** Energy balance calculation for dc bus
 *
 * Must be called exactly once per second, otherwise energy calculation gets wrong.
 */
void dc_bus_energy_balance(DcBus *bus);

#endif /* DC_BUS_H */
