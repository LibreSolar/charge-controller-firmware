/*
 * Copyright (c) 2017 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef POWER_PORT_H
#define POWER_PORT_H

/** @file
 *
 * @brief Definition of charge controller terminals and internal DC buses
 */

#include <stdbool.h>


/**
 * These values may be calculated based on external data, if multiple devices are connected to the
 * bus.
 */
class DcBus
{
public:
    float voltage;          ///< Measured bus voltage

    float voltage_max;      ///< Maximum voltage of a DC bus, equivalent to battery
                            ///< charging target voltage if hard-wired to a battery

    float voltage_min;      ///< Absolute minimum for both chg and discharging direction
};


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
    DcBus *bus;                     ///< Pointer to DC bus containing voltage measurement

    float sink_voltage_max;         ///< Maximum voltage if connected device acts as a sink
                                    ///< (equivalent to battery charging target voltage)
    float sink_voltage_min;         ///< Minimum voltage to allow positive current (necessary
                                    ///< to prevent charging of deep-discharged Li-ion batteries)

    float src_voltage_start;        ///< Minimum voltage to allow source current from the device
                                    ///< (equal to load reconnect voltage)
    float src_voltage_stop;         ///< Absolute minimum = load disconnect for batteries

    float current = 0;              ///< Measured current through this port (positive sign =
                                    ///< sinking current into the named external device)
    float power = 0;                ///< Product of port current and bus voltage

    float pos_current_limit;        ///< Maximum positive current (valid values >= 0.0)
    float pos_droop_res;            ///< control voltage = nominal voltage - droop_res * current

    float neg_current_limit;        ///< Maximum negative current (valid values <= 0.0)
    float neg_droop_res;            ///< control voltage = nominal voltage - droop_res * current

    float pos_energy_Wh;            ///< Cumulated sunk energy since last counter reset (Wh)
    float neg_energy_Wh;            ///< Cumulated sourced energy since last counter reset (Wh)

    PowerPort(DcBus *dc_bus) :
        bus(dc_bus)
    {}

    /** Initialize power port for solar panel connection
     *
     * @param max_abs_current Maximum input current allowed by PCB (as a positive value)
     */
    void init_solar();

    /** Initialize power port for nanogrid connection
     */
    void init_nanogrid();

    /** Initialize power port for load output connection
     */
    void init_load(float max_load_voltage);

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

/** Sets current limits of DC/DC or PWM switch according to load and battery status
 *
 * @param internal DC/DC or PWM switch internal port that can be controlled
 * @param p_bat Battery terminal port whose current limits have to be respected
 * @param p_load Load output port with current determined by external consumers
 */
void ports_update_current_limits(PowerPort *internal, PowerPort *p_bat, PowerPort *p_load);

#endif /* POWER_PORT_H */
