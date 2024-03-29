/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PWM_SWITCH_H
#define PWM_SWITCH_H

/**
 * @file
 *
 * @brief PWM charger MOSFET switch control functions
 *
 * Only used for PWM solar charge controllers.
 */

#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus

#include "power_port.h"

/**
 * PWM charger type
 *
 * Contains all data belonging to the PWM switching sub-component.
 */
class PwmSwitch : public PowerPort
{
public:
    PwmSwitch(DcBus *dc_bus);

    /**
     * Main control function for the PWM switching algorithm
     */
    void control();

    /**
     * Test mode for PWM switch
     *
     * Sets duty cycle to 90% and listens to enable/disable signal
     */
    void test();

    /**
     * Fast stop function (bypassing control loop)
     *
     * May be called from an ISR which detected overvoltage / overcurrent conditions.
     *
     * PWM port will be restarted automatically from control function if condtions are valid.
     */
    void stop();

    /**
     * Read the general on/off status of PWM switching
     *
     * @returns true if on
     */
    bool active();

    /**
     * Read the current high or low state of the PWM signal
     *
     * @returns true if high, false if low
     */
    bool signal_high();

    /**
     * Read the currently set duty cycle
     *
     * @returns Duty cycle between 0.0 and 1.0
     */
    float get_duty_cycle();

    /**
     * Voltage measurement at terminal (external, usually solar panel voltage)
     */
    float ext_voltage;

    /**
     * Enable switch, true by default. Can be used to completely disable the PWM power stage.
     */
    bool enable = true;

    /**
     * Offset voltage of solar panel vs. battery to start charging (V)
     */
    float offset_voltage_start = 2.0F;

    /**
     * Interval to wait before retrying charging after low solar power cut-off or overvoltage
     * event (s)
     */
    uint32_t restart_interval = 60;

    /**
     * Time when charger was switched off last time
     *
     * Initialized with large negative value to start immediately after reset.
     */
    time_t off_timestamp = -10000;

    /**
     * Last time the current through the switch was above minimum
     */
    time_t power_good_timestamp;
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Read the currently set duty cycle
 *
 * @returns Duty cycle between 0.0 and 1.0
 */
float pwm_signal_get_duty_cycle();

/**
 * Set the duty cycle of the PWM signal
 *
 * @param duty Duty cycle between 0.0 and 1.0
 */
void pwm_signal_set_duty_cycle(float duty);

/**
 * Change raw timer capture/compare register by defined step
 *
 * @param delta Steps to be added/substracted from current CCR value
 */
void pwm_signal_duty_cycle_step(int delta);

/**
 * Initiatializes the registers to generate the PWM signal and sets duty
 * cycle limits
 *
 * @param freq_Hz Switching frequency in Hz
 */
void pwm_signal_init_registers(int freq_Hz);

/**
 * Start the PWM generation
 *
 * @param duty Duty cycle between 0.0 and 1.0
 */
void pwm_signal_start(float pwm_duty);

/**
 * Stop the PWM generation
 */
void pwm_signal_stop();

/**
 * Check if the PWM pin voltage level is high (on-state)
 *
 * @returns true if pin is high, false if pin is low
 */
bool pwm_signal_high();

/**
 * Check if the PWM generation is active
 *
 * @returns true if PWM signal generation is active
 */
bool pwm_active();

#ifdef __cplusplus
}
#endif

#endif /* PWM_SWITCH_H */
