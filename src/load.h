/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LOAD_H
#define LOAD_H

/** @file
 *
 * @brief
 * Load/USB output functions and data types
 */

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "power_port.h"

/** Load/USB output states
 *
 * Used for USB or normal load outputs connecting to battery via switch.
 *
 * DEPRECATED: These states will be removed in the future and the status will be determined only
 * by the error flags.
 */
enum LoadState {
    LOAD_STATE_DISABLED = 0,        ///< Actively disabled
    LOAD_STATE_ON,                  ///< Normal state: On
    LOAD_STATE_OFF_LOW_SOC,         ///< Off to protect battery (overrules target setting)
    LOAD_STATE_OFF_OVERCURRENT,     ///< Off to protect charge controller (overrules target setting)
    LOAD_STATE_OFF_OVERVOLTAGE,     ///< Off to protect loads (overrules target setting)
    LOAD_STATE_OFF_SHORT_CIRCUIT,   ///< Off to protect charge controller (overrules target setting)
    LOAD_STATE_OFF_TEMPERATURE      ///< Off because of battery or charge controller temperature
};

/** Load output type
 *
 * Stores status of load output incl. 5V USB output (if existing on PCB)
 */
class LoadOutput
{
public:
    /** Initialize LoadOutput struct and overcurrent / short circuit protection comparator (if existing)
     *
     * @param load Load output struct
     * @param bus_int Internal DC bus the load switch is connected to (e.g. battery and DC/DC junction)
     * @param terminal Load terminal DC bus (external interface)
     */
    LoadOutput(PowerPort *pwr_port);

    /** Enable/disable load switch
     */
    void switch_set(bool enabled);

    /** Enable/disable USB output
     */
    void usb_set(bool enabled);

    /** USB state machine, called every second.
     */
    void usb_state_machine();

    /** Main load control function, should be called by control timer
     *
     * This function includes the load state machine
     */
    void control();

    /** Fast emergency stop function
     *
     * May be called from an ISR which detected overvoltage / overcurrent conditions
     *
     * @param error_flag Optional error flag that should be set
     */
    void stop(uint32_t error_flag = 0);

    uint32_t state;             ///< Current state of load output switch (DEPRECATED)
    uint32_t usb_state;         ///< Current state of USB output (DEPRECATED)

    PowerPort *port;            ///< Pointer to DC bus containting actual voltage and current
                                ///< measurement of (external) load output terminal

    bool enable = false;        ///< Target on state set via communication port (overruled if
                                ///< battery is empty or any errors occured)
    bool usb_enable = false;    ///< Target on state for USB output

    bool pgood = false;         ///< Power good flag that is true if load switch is on
    bool usb_pgood = false;     ///< Power good flag for USB output

    time_t oc_timestamp;        ///< Time when last overcurrent event occured
    int32_t oc_recovery_delay;  ///< Seconds before we attempt to re-enable the load
                                ///< after an overcurrent event

    float disconnect_voltage = 0;   ///< Low voltage disconnect (LVD) setpoint
    float reconnect_voltage = 0;    ///< Low voltage reconnect (LVD) setpoint

    time_t lvd_timestamp;       ///< Time when last low voltage disconnect happened
    int32_t lvd_recovery_delay; ///< Seconds before we re-enable the load after a low voltage
                                ///< disconnect

    float junction_temperature; ///< calculated using thermal model based on current and ambient
                                ///< temperature measurement (unit: °C)

    float overvoltage = 0;
    float ov_hysteresis;        ///< Hysteresis to switch back on after an overvoltage event

private:
    int determine_load_state();
    void short_circuit_comp_init();

    int ov_debounce_counter = 0;

#ifdef __ZEPHYR__
    void get_bindings();

    struct device *dev_load;
    struct device *dev_usb;
#endif
};

#endif /* LOAD_H */
