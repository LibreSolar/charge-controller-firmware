/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DCDC_H
#define DCDC_H

/**
 * @file
 *
 * @brief DC/DC buck/boost control functions
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus

#include "power_port.h"

/**
 * DC/DC operation mode
 *
 * Defines which type of device is connected to the high side and low side ports
 */
enum DcdcOperationMode
{
    /**
     * DC/DC converter switched off
     */
    DCDC_MODE_OFF,

    /**
     * Buck converter mode
     *
     * Solar panel at high side port, battery / load at low side port (typical MPPT).
     */
    DCDC_MODE_BUCK,

    /**
     * Boost converter mode
     *
     * Battery at high side port, solar panel at low side (e.g. e-bike charging).
     */
    DCDC_MODE_BOOST,

    /**
     * Automatic mode selection
     *
     * Boost or buck mode is automatically chosen based on high-side and low-side port settings.
     *
     * May be used in nanogrid applications: Accept input power (if available and need for charging)
     * or provide output power (if no other power source available on the grid and battery charged)
     */
    DCDC_MODE_AUTO
};

/**
 * DC/DC control state
 *
 * Allows to determine the current control state (off, CC, CV and MPPT)
 */
enum DcdcControlState
{
    DCDC_CONTROL_OFF,           ///< DC/DC switched off (low input power or actively disabled)
    DCDC_CONTROL_MPPT,          ///< Maximum Power Point Tracking
    DCDC_CONTROL_CC_LS,         ///< Constant-Current control at low-side
    DCDC_CONTROL_CV_LS,         ///< Constant-Voltage control at low-side
    DCDC_CONTROL_CC_HS,         ///< Constant-Current control at high-side
    DCDC_CONTROL_CV_HS,         ///< Constant-Voltage control at high-side
    DCDC_CONTROL_DERATING       ///< Hardware-limits (current or temperature) reached
};

/**
 * DC/DC class
 *
 * Contains all data belonging to the DC/DC sub-component of the PCB, incl.
 * actual measurements and calibration parameters.
 */
class Dcdc
{
public:
    /**
     * Initialize DC/DC and DC/DC port structs
     *
     * See http://libre.solar/docs/dcdc_control for detailed information
     *
     * @param high High voltage bus (e.g. solar input for MPPT buck)
     * @param low Low voltage bus (e.g. battery output for MPPT buck)
     * @param mode Operation mode (buck, boost or nanogrid)
     */
    Dcdc(DcBus *high, DcBus* low, DcdcOperationMode mode);

    /**
     * Check for valid start conditions of the DC/DC converter
     *
     * @returns operation mode that is valid for starting up
     */
    DcdcOperationMode check_start_conditions();

    /**
     * Main control function for the DC/DC converter
     *
     * If DC/DC is off, this function checks start conditions and starts conversion if possible.
     */
    void control();

    /**
     * Test mode for DC/DC, ramping up to 50% duty cycle
     */
    void test();

    /**
     * Fast stop function (bypassing control loop)
     *
     * May be called from an ISR which detected overvoltage / overcurrent conditions.
     *
     * DC/DC will be restarted automatically from control function if condtions are valid.
     */
    void stop();

    /**
     * Prevent overcharging of battery in case of shorted HS MOSFET
     *
     * This function switches the LS MOSFET continuously on to blow the battery input fuse. The
     * reason for self destruction should be logged and stored to EEPROM prior to calling this
     * function, as the charge controller power supply will be cut after the fuse is destroyed.
     */
    void fuse_destruction();

    DcdcOperationMode mode;     ///< DC/DC mode (buck, boost or nanogrid)
    bool enable;                ///< Can be used to disable the DC/DC power stage
    uint16_t state;             ///< Control state (off / MPPT / CC / CV)

    // actual measurements
    DcBus *hvb;                 ///< Pointer to DC bus at high voltage side
    DcBus *lvb;                 ///< Pointer to DC bus at low voltage (inductor) side
    float inductor_current;     ///< Inductor current
    float power;                ///< Low-side power
    float temp_mosfets;         ///< MOSFET temperature measurement (if existing)

    // current state
    float power_prev;           ///< Stores previous conversion power (set via dcdc_control)
    int32_t pwm_delta;          ///< Direction of PWM change for MPPT
    int32_t off_timestamp;      ///< Last time the DC/DC was switched off
    int32_t power_good_timestamp;   ///< Last time the DC/DC reached above minimum output power

    // maximum allowed values
    float inductor_current_max = 0;   ///< Maximum low-side (inductor) current
    float hs_voltage_max = 0;   ///< Maximum high-side voltage
    float ls_voltage_max;       ///< Maximum low-side voltage
    float ls_voltage_min;       ///< Minimum low-side voltage, e.g. for driver supply
    float output_power_min;     ///< Minimum output power (if lower, DC/DC is switched off)

    // calibration parameters
    uint32_t restart_interval;  ///< Restart interval (s): When should we retry to start
                                ///< charging after low output power cut-off?

private:
    /**
     * MPPT perturb & observe control
     *
     * Calculates the duty cycle change depending on operating mode and actual measurements and
     * changes the half bridge PWM signal accordingly
     *
     * @returns 0 if everything is fine, error number otherwise
     */
    int perturb_observe_controller();

    /**
     * If manual control of the reverse polarity MOSFET on the high-side is available, this
     * function enables it to use the high voltage side as output.
     */
    void output_hvs_enable();

    /**
     * Disable the reverse polarity MOSFET on the high-sied (if option available)
     */
    void output_hvs_disable();

    /**
     * Check if the high-side MOSFET is shorted
     */
    bool check_hs_mosfet_short();

    /**
     * Check if we need to wait for voltages to settle before starting the DC/DC
     *
     * @param reset Reset the inhibit counter if set to true
     *
     * @returns true if we need to wait
     */
    bool startup_inhibit(bool reset = false);
};

#endif // __cplusplus


#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_CUSTOM_DCDC_CONTROLLER

/**
 * Low-level control function
 *
 * Implement this function e.g. for cycle-by-cylce current limitation.
 *
 * It is called from the DMA after each new current reading, i.e. it runs in ISR context with
 * high frequency and must be VERY fast!
 */
void dcdc_low_level_controller();

#endif // CONFIG_CUSTOM_DCDC_CONTROLLER

#ifdef __cplusplus
}
#endif

#endif /* DCDC_H */
