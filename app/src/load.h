/*
 * Copyright (c) The Libre Solar Project Contributors
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

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus

#include "power_port.h"

#include <zephyr/device.h>

/**
 * Load/USB output states
 */
enum LoadState
{
    LOAD_STATE_OFF = 0, ///< Actively disabled
    LOAD_STATE_ON = 1,  ///< Normal state: On
};

/** Load error flags
 *
 * When adding new flags, please make sure to use only up to 32 errors
 * Each enum must represent a unique power of 2 number
 */
enum LoadErrorFlag
{
    /**
     * Available energy or power too low
     *
     * Switching off the load can be triggered either by a low battery voltage or by low state of
     * charge (SOC) in case of more advanced battery management.
     *
     * Set in LoadOutput::control() and cleared after reconnect delay passed and voltage is above
     * reconnect threshold again.
     */
    ERR_LOAD_SHEDDING = 1U << 0,

    /**
     * Too high voltage for load
     *
     * Set and cleared in LoadOutput::control()
     */
    ERR_LOAD_OVERVOLTAGE = 1U << 1,

    /**
     * Long-term overcurrent at load port
     *
     * Set in LoadOutput::control() and cleared after configurable delay.
     */
    ERR_LOAD_OVERCURRENT = 1U << 2,

    /**
     * Short circuit detected at load port
     *
     * Set after the power switch reported a short circuit detected in hardware, cleared only if
     * load output is manually disabled and enabled again.
     */
    ERR_LOAD_SHORT_CIRCUIT = 1U << 3,

    /**
     * Overcurrent identified via voltage dip (may be caused by too small battery)
     *
     * Set and cleared in LoadOutput::control(). Treated same as load overcurrent.
     */
    ERR_LOAD_VOLTAGE_DIP = 1U << 4,

    /**
     * The bus the load is connected to disabled sourcing current from it
     *
     * Reasons can be that battery temperature limits were exceeded. Voltage limits should be
     * covered by the load directly.
     */
    ERR_LOAD_BUS_SRC_CURRENT = 1U << 5
};

/** Load output type
 *
 * Stores status of load output incl. 5V USB output (if existing on PCB)
 */
class LoadOutput : public PowerPort
{
public:
    /**
     * Initialize LoadOutput struct and switch off the output
     *
     * @param dc_bus DC bus the load is connected to
     * @param power_switch Power switch device of the output
     * @param led LED indicating the switch state or -1 if not existing
     */
    LoadOutput(DcBus *dc_bus, const struct device *power_switch, int led = -1);

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

    /**
     * Update of load voltage limits (typically based on battery configuration)
     *
     * @param lvd Low voltage disconnect setpoint
     * @param lvr Low voltage reconnect setpoint
     * @param ov Overvoltage setpoint
     */
    void set_voltage_limits(float lvd, float lvr, float ov);

    uint32_t state; ///< Current state of load output switch

    uint32_t error_flags = 0; ///< Stores error flags as bits according to LoadErrorFlag enum

    int32_t info; ///< Contains either the state or negative value of error_flags
                  ///< in case of error_flags > 0. This allows to have a single
                  ///< variable for load state diagnosis.

    bool enable = false; ///< Target on state set via communication port (overruled if
                         ///< battery is empty or any errors occured)

    time_t oc_timestamp;        ///< Time when last overcurrent event occured
    uint32_t oc_recovery_delay; ///< Seconds before we attempt to re-enable the load
                                ///< after an overcurrent event

    float disconnect_voltage = 0; ///< Low voltage disconnect (LVD) setpoint
    float reconnect_voltage = 0;  ///< Low voltage reconnect (LVD) setpoint

    time_t lvd_timestamp;        ///< Time when last low voltage disconnect happened
    uint32_t lvd_recovery_delay; ///< Seconds before we re-enable the load after a low voltage
                                 ///< disconnect

    float junction_temperature; ///< calculated using thermal model based on current and ambient
                                ///< temperature measurement (unit: °C)

    float overvoltage = 0; ///< Upper voltage limit
    float ov_hysteresis;   ///< Hysteresis to switch back on after an overvoltage event

private:
    /**
     * Switch the output and the indicator LED (if existing)
     */
    void switch_set(bool on);

    /**
     * Callback for short circuits detected by the power switch hardware
     */
    static void short_circuit_callback(const struct device *dev, uint32_t events, void *user_data);

    /**
     * Power switch device
     */
    const struct device *power_switch;

    /**
     * LED indicating the switch state or -1 if not existing
     */
    int led;

    /**
     * Used to prevent switching of because of very short voltage dip
     */
    int uv_debounce_counter = 0;

    /**
     * Used to prevent switching off because of short voltage spike
     */
    int ov_debounce_counter = 0;
};
#endif

#endif /* LOAD_H */
