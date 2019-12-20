/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PWM_SWITCH_H
#define PWM_SWITCH_H

/** @file
 *
 * @brief PWM charger MOSFET switch control functions
 */

#include <stdbool.h>

#include "power_port.h"

/** PWM charger type
 *
 * Contains all data belonging to the PWM switching sub-component.
 */
class PwmSwitch
{
public:
    /** Initialization of PWM switch struct
     */
    PwmSwitch(PowerPort *pwm_terminal, PowerPort *pwm_port_int);

    /** Main control function for the PWM switching algorithm
     */
    void control();

    /** Fast emergency stop function
     *
     * May be called from an ISR which detected overvoltage / overcurrent conditions
     */
    void emergency_stop();

    /** Read the general on/off status of PWM switching
     *
     * @returns true if on
     */
    bool active();

    /** Read the current high or low state of the PWM signal
     *
     * @returns true if high, false if low
     */
    bool signal_high();

    /** Read the currently set duty cycle
     *
     * @returns Duty cycle between 0.0 and 1.0
     */
    float get_duty_cycle();

    PowerPort *terminal;            ///< Pointer to external power port (terminal)
    PowerPort *port_int;            ///< Pointer to internal power port (junction with load and battery)

    bool enabled;                   ///< Can be used to disable the PWM power stage
    float offset_voltage_start;     ///< Offset voltage of solar panel vs. battery to start charging (V)
    int restart_interval;           ///< Interval to wait before retrying charging after low solar power cut-off (s)
    int off_timestamp;              ///< Time when charger was switched off last time
};

#endif /* PWM_SWITCH_H */
