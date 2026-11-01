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

#include <zephyr/device.h>

/**
 * PWM charger type
 *
 * Contains all data belonging to the PWM switching sub-component.
 */
class PwmSwitch : public power_port
{
public:
    PwmSwitch(struct dc_bus *dc_bus, const struct device *power_switch);

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

private:
    /**
     * Increase or decrease the duty cycle by the specified number of steps
     *
     * The duty cycle is not changed if the result would be out of range.
     */
    void duty_cycle_step(int steps);

    /**
     * Callback for the rising edges of the PWM signal
     */
    static void pwm_edge_callback(const struct device *dev, uint32_t events, void *user_data);

    /**
     * Power switch device
     */
    const struct device *power_switch;
};
#endif

#endif /* PWM_SWITCH_H */
