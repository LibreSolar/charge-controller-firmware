/*
 * Copyright (c) 2019 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DEVICE_STATUS_H
#define DEVICE_STATUS_H

/** @file
 *
 * @brief
 * Device-level data storage and functions (like max/min values, error flags, etc.)
 */

#include "bat_charger.h"
#include "power_port.h"
#include "load.h"
#include "dcdc.h"
#include <stdbool.h>
#include <stdint.h>

/** Error Flags
 *
 * When adding new flags, please make sure to use only up to 32 errors
 * Each enum must represent a unique power of 2 number
 */
enum ErrorFlag {

    /** Battery voltage too low
     *
     * Set and cleared in Charger::discharge_control() if the battery voltage dropped to lower than
     * absolute minimum. Load outputs should have switched off earlier.
     */
    ERR_BAT_UNDERVOLTAGE = 1U << 0,

    /** Battery voltage too high
     *
     * Set directly in ISR after ADC conversion finished, cleared in Charger::charge_control() when
     * voltage reached lower level again.
     */
    ERR_BAT_OVERVOLTAGE = 1U << 1,

    /** Battery discharge overcurrent
     *
     * Not used yet, reserved for future.
     */
    ERR_BAT_DIS_OVERCURRENT = 1U << 2,

    /** Battery charge overcurrent
     *
     * Not used yet, reserved for future.
     */
    ERR_BAT_CHG_OVERCURRENT = 1U << 3,

    /** Temperature below discharge minimum limit
     *
     * Set and cleared in Charger::discharge_control (with 2°C hysteresis)
     */
    ERR_BAT_DIS_UNDERTEMP = 1U << 4,

    /** Temperature above discharge maximum limit
     *
     * Set and cleared in Charger::discharge_control (with 2°C hysteresis)
     */
    ERR_BAT_DIS_OVERTEMP = 1U << 5,

    /** Temperature below charge minimum limit
     *
     * Set and cleared in Charger::charge_control (with 2°C hysteresis)
     */
    ERR_BAT_CHG_UNDERTEMP = 1U << 6,

    /** Temperature above charge maximum limit
     *
     * Set and cleared in Charger::charge_control (with 2°C hysteresis)
     */
    ERR_BAT_CHG_OVERTEMP = 1U << 7,

    /** Charge controller internal temperature too high
     *
     * Set and cleared by daq_update()
     */
    ERR_INT_OVERTEMP = 1U << 13,

    /** Short-circuit in HS MOSFET
     *
     * Set in Dcdc::control() and never cleared
     */
    ERR_DCDC_HS_MOSFET_SHORT = 1U << 14,

    /** Short-circuit in HS MOSFET
     *
     * Set in Dcdc::control() and never cleared
     */
    ERR_DCDC_HS_OVERVOLTAGE = 1U << 15,

    /** Mask to catch all error flags (up to 32 errors)
     */
    ERR_ANY_ERROR = UINT32_MAX,
};

/** Device Status data
 *
 * Stores error counters and some maximum ever measured values to EEPROM
 */
class DeviceStatus
{
public:

    /** Updates the total energy counters for solar, battery and load bus
     */
    void update_energy();

    /** Updates the logged min/max values for voltages, power, temperatures etc.
     */
    void update_min_max_values();

    // total energy
    uint32_t bat_chg_total_Wh;
    uint32_t bat_dis_total_Wh;
    uint32_t solar_in_total_Wh;
    uint32_t load_out_total_Wh;
    #if CONFIG_HV_TERMINAL_NANOGRID
    uint32_t grid_import_total_Wh;
    uint32_t grid_export_total_Wh;
    #endif

    // maximum/minimum values
    uint16_t solar_power_max_day;
    uint16_t load_power_max_day;
    uint16_t solar_power_max_total;
    uint16_t load_power_max_total;
    float battery_voltage_max;
    float solar_voltage_max;
    float dcdc_current_max;
    float load_current_max;
    int16_t bat_temp_max;         // °C
    int16_t int_temp_max;         // °C
    int16_t mosfet_temp_max;

    uint32_t day_counter;

    // instantaneous device-level data
    uint32_t error_flags;       ///< Currently detected errors
    float internal_temp;        ///< Internal temperature (measured in MCU)

    /**
     * @brief sets one or more error flags in device state
     * @param e a single ErrorFlag or "bitwise ORed" ERR_XXX | ERR_YYY
     */
    void set_error(uint32_t e) { error_flags |= e; }

    /**
     * @brief clears one or more error flags in device state
     * @param e a single ErrorFlag or "bitwise ORed" ERR_XXX | ERR_YYY
     */
    void clear_error(uint32_t e) { error_flags &= ~e; }

    /**
     * @brief queries one or more error flags in device state
     * @param e a single ErrorFlag or "bitwise ORed" ERR_XXX | ERR_YYY
     * @return true if any of the error flags given in e are set in device state
     */
    bool has_error(uint32_t e) { return (error_flags & e) != 0; }
};

#endif /* DEVICE_STATUS_H */
