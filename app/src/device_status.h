/*
 * Copyright (c) The Libre Solar Project Contributors
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

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Error Flags
 *
 * When adding new flags, please make sure to use only up to 32 errors
 * Each enum must represent a unique power of 2 number
 */
enum ErrorFlag
{
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

    /** Overvoltage in HS MOSFET
     *
     * Set in DAQ alert and never cleared
     */
    ERR_DCDC_HS_OVERVOLTAGE = 1U << 15,

    /** Overvoltage caused by PWM switch
     *
     * Set and cleared in PwmSwitch::control()
     */
    ERR_PWM_SWITCH_OVERVOLTAGE = 1U << 16,

    /** Mask to catch all error flags (up to 32 errors)
     */
    ERR_ANY_ERROR = UINT32_MAX,
};

/** Device Status data
 *
 * Stores error counters and some maximum ever measured values to EEPROM
 */
struct device_status
{
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
    int16_t bat_temp_max; // °C
    int16_t int_temp_max; // °C
    int16_t mosfet_temp_max;

    uint32_t day_counter;

    // instantaneous device-level data
    uint32_t error_flags; ///< Currently detected errors
    float internal_temp;  ///< Internal temperature (measured in MCU)
};

/** Global device status */
extern struct device_status dev_stat;

/**
 * Updates the total energy counters for solar, battery and load bus
 *
 * Must be called exactly once per second, otherwise energy calculation gets wrong.
 *
 * @param ds Device status
 */
void device_status_update_energy(struct device_status *ds);

/**
 * Updates the logged min/max values for voltages, power, temperatures etc.
 *
 * @param ds Device status
 */
void device_status_update_min_max_values(struct device_status *ds);

/**
 * Sets one or more error flags in device state
 *
 * @param ds Device status
 * @param e a single ErrorFlag or "bitwise ORed" ERR_XXX | ERR_YYY
 */
static inline void device_status_set_error(struct device_status *ds, uint32_t e)
{
    ds->error_flags |= e;
}

/**
 * Clears one or more error flags in device state
 *
 * @param ds Device status
 * @param e a single ErrorFlag or "bitwise ORed" ERR_XXX | ERR_YYY
 */
static inline void device_status_clear_error(struct device_status *ds, uint32_t e)
{
    ds->error_flags &= ~e;
}

/**
 * Queries one or more error flags in device state
 *
 * @param ds Device status
 * @param e a single ErrorFlag or "bitwise ORed" ERR_XXX | ERR_YYY
 *
 * @return true if any of the error flags given in e are set in device state
 */
static inline bool device_status_has_error(const struct device_status *ds, uint32_t e)
{
    return (ds->error_flags & e) != 0;
}

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_STATUS_H */
