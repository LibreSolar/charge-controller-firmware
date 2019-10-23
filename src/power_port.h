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
 * @brief Definition of charge controller terminals and internal DC buses
 */

#include <stdbool.h>

/** Power Port class
 *
 * Stores actual measurements and voltage / current limits for external terminals or internal ports
 */
class PowerPort
{
public:
    float voltage;                  ///< Measured bus voltage

    float chg_voltage_target;       ///< Target voltage if bus is charged
    float chg_voltage_min;          ///< Minimum voltage to allow charge current (necessary
                                    ///< to prevent charging of deep-discharged Li-ion batteries)

    float dis_voltage_start;        ///< Minimum voltage to allow discharge current from the bus
                                    ///< (equal to load reconnect voltage)
    float dis_voltage_stop;         ///< Absolute minimum = load disconnect for batteries

    float current;                  ///< Measured current through this port (positive sign =
                                    ///< increasing voltage/charge of the named device)
    float power;                    ///< Product of current & voltage

    float chg_current_limit;        ///< Maximum current charging the bus, i.e. increasing its
                                    ///< voltage (charging direction for battery terminal)
    float chg_droop_res;            ///< control voltage = nominal voltage - droop_res * current

    float dis_current_limit;        ///< Maximum current to discharge the bus, i.e. decrease its
                                    ///< voltage, value must be negative!!!
    float dis_droop_res;            ///< control voltage = nominal voltage - droop_res * current

    float chg_energy_Wh;            ///< cumulated energy in charge direction since last counter reset (Wh)
    float dis_energy_Wh;            ///< cumulated energy in discharge direction since last counter reset (Wh)

    /** Initialize power port for solar panel connection
     *
     * @param max_abs_current Maximum input current allowed by PCB (as a positive value)
     */
    void init_solar();

    /** Initialize power port for nanogrid connection
     */
    void init_nanogrid();

    /** Energy balance calculation for power port
     *
     * Must be called exactly once per second, otherwise energy calculation gets wrong.
     */
    void energy_balance();

    /** Passes own voltage target settings to another port
     *
     * @param port Port where the voltage settings will be adjusted
     */
    void pass_voltage_targets(PowerPort *port);
};

/** Sets current limits of DC/DC and load according to battery status
 */
void ports_update_current_limits(PowerPort *p_dcdc, PowerPort *p_bat, PowerPort *p_load);

#endif /* POWER_PORT_H */
