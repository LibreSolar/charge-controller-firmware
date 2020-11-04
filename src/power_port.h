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

#include <stdint.h>
#include <stdbool.h>

class PowerPort;    // forward-declaration

/**
 * DC bus class
 *
 * Stores measurement data and settings necessary for voltage control
 */
class DcBus
{
public:
    /**
     * Measured bus voltage
     */
    float voltage;

    /**
     * Multiplier for series connection of batteries
     *
     * Used for automatic 12V/24V battery detection at start-up (can be 1 or 2 only)
     *
     * This factor must be applied to all voltage setpoints
     */
    int16_t series_multiplier = 1;

    /**
     * Upper voltage boundary where this bus may be used to sink current.
     *
     * This value is the voltage at zero current. Values for other currents are calculated
     * using the droop resistance.
     */
    float sink_voltage_intercept;

    /**
     * Lower voltage boundary where this bus may be used to source current.
     *
     * This value is the voltage at zero current. Values for other currents are calculated
     * using the droop resistance.
     */
    float src_voltage_intercept;

    /**
     * Droop resistance to adjust voltage bounds for current in sourcing direction
     *
     * control voltage = nominal voltage - droop_res * current
     */
    float src_droop_res;

    /**
     * Droop resistance to adjust voltage bounds for current in sinking direction
     *
     * control voltage = nominal voltage - droop_res * current
     */
    float sink_droop_res;

    /**
     * Pointer to current measurement that is used to determine the droop. This is typically the
     * battery or nanogrid terminal.
     */
    float *ref_current;

    /**
     * Available additional current towards the DC bus until limits of the connected ports are
     * reached
     */
    float sink_current_margin;

    /**
     * Available additional current from the DC bus until limits of the connected ports are reached
     * (has a negative sign)
     */
    float src_current_margin;

    /**
     * Calculate current-compensated src control voltage, considering droop and series multiplier
     *
     * @param voltage_zero_current Voltage at zero current (without droop). If this parameter is
     *                             left empty, the src_voltage_intercept is considered.
     */
    inline float src_control_voltage(float voltage_zero_current = 0)
    {
        float v0 = (voltage_zero_current == 0) ? src_voltage_intercept : voltage_zero_current;
        if (ref_current != nullptr) {
            return (v0 - src_droop_res * (*ref_current)) * series_multiplier;
        }
        else {
            return v0 * series_multiplier;
        }
    }

    /**
     * Calculate current-compensated sink control voltage, considering droop and series multiplier
     *
     * @param voltage_zero_current Voltage at zero current (without droop). If this parameter is
     *                             left empty, the sink_voltage_intercept is considered.
     */
    inline float sink_control_voltage(float voltage_zero_current = 0)
    {
        float v0 = (voltage_zero_current == 0) ? sink_voltage_intercept : voltage_zero_current;
        if (ref_current != nullptr) {
            return (v0 - sink_droop_res * (*ref_current)) * series_multiplier;
        }
        else {
            return v0 * series_multiplier;
        }
    }

    /**
     * Calculate voltage for series connected batteries based on setpoint for single battery
     *
     * @param single_voltage Voltage for single battery
     *
     * @returns Total voltage
     */
    inline float series_voltage(float single_voltage)
    {
        return single_voltage * series_multiplier;
    }
};

/**
 * Power Port class
 *
 * Stores current measurements and limits for external terminals or internal ports
 *
 * The signs follow the passive sign convention. Current or power from the considered system or
 * circuit towards an external device connected to the port has a positive sign. For all terminals,
 * the entire charge controller is considered as the system boundary and acts as a source or a sink.
 * For internal sub-circuits, e.g. the DC/DC converter circuit defines the sub-system boundaries.
 *
 *    -----------------
 *    |               |    >> positive current/power direction
 *    |               o---->----
 *    |               |     +  |
 *    |  considered   |       | | external device: battery / solar panel / load / DC grid
 *    |   system or   |       | |
 *    |  sub-circuit  |       | | (the port should be named after the external device)
 *    |               |     -  |
 *    |               o---------
 *    |               |
 *    -----------------
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
    /**
     * Each power port is connected to a DC bus, containing relevant voltage information
     */
    DcBus *bus;

    /**
     * Measured current through this port (positive sign = current into the external device)
     */
    float current = 0;

    /**
     * Product of port current and bus voltage
     */
    float power = 0;

    /**
     * Maximum positive current (valid values >= 0.0)
     */
    float pos_current_limit = 0;

    /**
     * Maximum negative current (valid values <= 0.0)
     */
    float neg_current_limit = 0;

    /**
     * Cumulated energy in positive current direction since last counter reset (Wh)
     */
    float pos_energy_Wh = 0;

    /**
     * Cumulated energy in negative current direction since last counter reset (Wh)
     */
    float neg_energy_Wh = 0;

    /**
     * Constructor assigning the port to a DC bus
     *
     * @param dc_bus The DC bus this port is assigned to
     * @param assign_ref_current defines if the bus ref_current should point to the current
     *                           of this port (must be true for at least one port)
     */
    PowerPort(DcBus *dc_bus, bool assign_ref_current = false) :
        bus(dc_bus)
    {
        if (assign_ref_current) {
            bus->ref_current = &current;
        }
    }

    /**
     * Initialize power port for solar panel connection
     */
    void init_solar();

    /**
     * Initialize power port for nanogrid connection
     */
    void init_nanogrid();

    /**
     * Energy balance calculation for power port
     *
     * Must be called exactly once per second, otherwise energy calculation gets wrong.
     */
    void energy_balance();

    /**
     * Sets current limits for control of the bus voltage
     *
     * This function has to be called using the port defining the bus control targets, i.e.
     * the battery, the solar panel or the DC grid.
     */
    void update_bus_current_margins() const;
};

#endif /* POWER_PORT_H */
