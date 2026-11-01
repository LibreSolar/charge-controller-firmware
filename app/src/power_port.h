/*
 * Copyright (c) The Libre Solar Project Contributors
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
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * DC bus class
 *
 * Stores measurement data and settings necessary for voltage control
 */
struct dc_bus
{
    /**
     * Measured bus voltage
     */
    float voltage;

    /**
     * Filtered bus voltage
     *
     * Suitable filter time constants depend on type of DC bus. This filtering is applied in
     * addition to the raw ADC value filtering.
     */
    float voltage_filtered;

    /**
     * Multiplier for series connection of batteries
     *
     * Used for automatic 12V/24V battery detection at start-up (can be 1 or 2 only)
     *
     * This factor must be applied to all voltage setpoints
     */
    int16_t series_multiplier;

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
     *
     * If for example a battery connected to the bus is not allowed to be discharged anymore, but
     * we still get power via solar, current up to the amount of solar input can be diverted into
     * the load (until the margin gets 0 and the battery would be discharged).
     */
    float src_current_margin;
};

/**
 * Power port
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
struct power_port
{
    /**
     * Each power port is connected to a DC bus, containing relevant voltage information
     */
    struct dc_bus *bus;

    /**
     * Measured current through this port (positive sign = current into the external device)
     */
    float current;

    /**
     * Filtered port current
     *
     * Suitable filter time constants depend on type of port. This filtering is applied in
     * addition to the raw ADC value filtering.
     */
    float current_filtered;

    /**
     * Product of port current and bus voltage
     */
    float power;

    /**
     * Maximum positive current (valid values >= 0.0)
     */
    float pos_current_limit;

    /**
     * Maximum negative current (valid values <= 0.0)
     */
    float neg_current_limit;

    /**
     * Cumulated energy in positive current direction since last counter reset (Wh)
     */
    float pos_energy_Wh;

    /**
     * Cumulated energy in negative current direction since last counter reset (Wh)
     */
    float neg_energy_Wh;
};

/**
 * Initialize a DC bus
 *
 * All values are set to zero, except for the series multiplier, which is set to 1.
 *
 * @param bus DC bus
 */
void dc_bus_init(struct dc_bus *bus);

/**
 * Calculate current-compensated src control voltage, considering droop and series multiplier
 *
 * @param bus DC bus
 * @param voltage_zero_current Voltage at zero current (without droop). If this parameter is
 *                             0, the src_voltage_intercept is considered.
 */
static inline float dc_bus_src_control_voltage(const struct dc_bus *bus, float voltage_zero_current)
{
    float v0 = (voltage_zero_current == 0) ? bus->src_voltage_intercept : voltage_zero_current;
    if (bus->ref_current != NULL) {
        return (v0 - bus->src_droop_res * (*bus->ref_current)) * bus->series_multiplier;
    }
    else {
        return v0 * bus->series_multiplier;
    }
}

/**
 * Calculate current-compensated sink control voltage, considering droop and series multiplier
 *
 * @param bus DC bus
 * @param voltage_zero_current Voltage at zero current (without droop). If this parameter is
 *                             0, the sink_voltage_intercept is considered.
 */
static inline float dc_bus_sink_control_voltage(const struct dc_bus *bus,
                                                float voltage_zero_current)
{
    float v0 = (voltage_zero_current == 0) ? bus->sink_voltage_intercept : voltage_zero_current;
    if (bus->ref_current != NULL) {
        return (v0 - bus->sink_droop_res * (*bus->ref_current)) * bus->series_multiplier;
    }
    else {
        return v0 * bus->series_multiplier;
    }
}

/**
 * Calculate voltage for series connected batteries based on setpoint for single battery
 *
 * @param bus DC bus
 * @param single_voltage Voltage for single battery
 *
 * @returns Total voltage
 */
static inline float dc_bus_series_voltage(const struct dc_bus *bus, float single_voltage)
{
    return single_voltage * bus->series_multiplier;
}

/**
 * Initialize a power port and assign it to a DC bus
 *
 * All values are set to zero.
 *
 * @param port Power port
 * @param bus The DC bus this port is assigned to
 * @param assign_ref_current defines if the bus ref_current should point to the current
 *                           of this port (must be true for at least one port)
 */
void power_port_init(struct power_port *port, struct dc_bus *bus, bool assign_ref_current);

/**
 * Initialize power port for solar panel connection
 *
 * @param port Power port
 */
void power_port_init_solar(struct power_port *port);

/**
 * Initialize power port for nanogrid connection
 *
 * @param port Power port
 */
void power_port_init_nanogrid(struct power_port *port);

/**
 * Energy balance calculation for power port
 *
 * Must be called exactly once per second, otherwise energy calculation gets wrong.
 *
 * @param port Power port
 */
void power_port_energy_balance(struct power_port *port);

/**
 * Sets current limits for control of the bus voltage
 *
 * This function has to be called using the port defining the bus control targets, i.e.
 * the battery, the solar panel or the DC grid.
 *
 * @param port Power port
 */
void power_port_update_bus_current_margins(const struct power_port *port);

#ifdef __cplusplus
}
#endif

#endif /* POWER_PORT_H */
