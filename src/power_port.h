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

/** DC bus type
 *
 * Stores the voltage measurement and voltage limits for a DC bus inside the charge controller PCB
 *
 * A DC bus is considered a connection of two or more electrical circuits in one point, such that
 * they have the same electric potential.
 */
class DcBus
{
public:
    float voltage;                  ///< Measured bus voltage

    float chg_voltage_target;       ///< Target voltage if bus is charged
    float chg_voltage_min;          ///< Minimum voltage to allow charge current (necessary
                                    ///< to prevent charging of deep-discharged Li-ion batteries)

    float dis_voltage_start;        ///< Minimum voltage to allow discharge current from the bus
                                    ///< (equal to load reconnect voltage)
    float dis_voltage_stop;         ///< Absolute minimum = load disconnect for batteries
};


/** Power Port type
 *
 * Stores the current and power measurements and limits for one connection to a DC bus. One power
 * port is assigned to exactly ony DC bus, but one DC bus can have multiple ports.
 */
class PowerPort
{
public:
    PowerPort(DcBus *dc_bus):
        bus(dc_bus) {};

    DcBus *bus;                     ///< Pointer to the DC bus this port is connected to (also
                                    ///< storing its voltage measurement)

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
};


#endif /* POWER_PORT_H */
