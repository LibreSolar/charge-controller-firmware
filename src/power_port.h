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


//    -----------------
//    |               |    >> positive current/power direction
//    |               o---->----
//    |               |     +  |
//    |  considered   |       | | external device: battery / solar panel / load / DC grid
//    |   system or   |       | |
//    |  sub-circuit  |       | | (the port should be named after the external device)
//    |               |     -  |
//    |               o---------
//    |               |
//    -----------------

/** Power Port class
 *
 * Stores actual measurements and voltage / current limits for external terminals or internal ports
 *
 * The signs follow the passive sign convention. Current or power from the considered system or
 * circuit towards an external device connected to the port has a positive sign. For all terminals,
 * the entire charge controller is considered as the system boundary and acts as a source or a sink.
 * For internal sub-circuits, e.g. the DC/DC converter circuit defines the sub-system boundaries.
 *
 * Examples:
 * - Charging a connected battery has a positive sign, as current flows from the charge controller
 *   into the battery, i.e. the battery acts as a sink.
 * - Power from a solar panel (power source) has a negative sign, as the charge controller acts as
 *   the sink and power flows from the external device into the charge controller.
 * - A DC/DC converter in buck mode results in a positive current flow at the low voltage side and
 *   a negative current at the high voltage side. The system boundary is the DC/DC sub-circuit,
 *   which sources current from the high side port and sinks it through the low side port.
 */
class PowerPort
{
public:
    float voltage;                  ///< Measured port voltage

    float sink_voltage_max;         ///< Maximum voltage if connected device acts as a sink
                                    ///< (equivalent to battery charging target voltage)
    float sink_voltage_min;         ///< Minimum voltage to allow positive current (necessary
                                    ///< to prevent charging of deep-discharged Li-ion batteries)

    float src_voltage_start;        ///< Minimum voltage to allow source current from the device
                                    ///< (equal to load reconnect voltage)
    float src_voltage_stop;         ///< Absolute minimum = load disconnect for batteries

    float current;                  ///< Measured current through this port (positive sign =
                                    ///< sinking current into the named external device)
    float power;                    ///< Product of current & voltage

    float pos_current_limit;        ///< Maximum positive current (valid values >= 0.0)
    float pos_droop_res;            ///< control voltage = nominal voltage - droop_res * current

    float neg_current_limit;        ///< Maximum negative current (valid values <= 0.0)
    float neg_droop_res;            ///< control voltage = nominal voltage - droop_res * current

    float pos_energy_Wh;            ///< Cumulated sunk energy since last counter reset (Wh)
    float neg_energy_Wh;            ///< Cumulated sourced energy since last counter reset (Wh)

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
