/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HALF_BRIDGE_H
#define HALF_BRIDGE_H
#include <stdint.h>
/** @file
 *
 * @brief PWM timer functions for half bridge of DC/DC converter
 */

/** Initiatializes the registers to generate the PWM signal and sets duty
 *  cycle limits
 *
 * @param freq_kHz Switching frequency in kHz
 * @param deadtime_ns Deadtime in ns between switching the two FETs on/off
 * @param min_duty Minimum duty cycle (e.g. 0.5 for limiting input voltage)
 * @param max_duty Maximum duty cycle (e.g. 0.97 for charge pump)
 */
void half_bridge_init(int freq_kHz, int deadtime_ns, float min_duty, float max_duty);

/** Start the PWM generation
 *
 * @param pwm_duty Duty cycle between 0.0 and 1.0
 */
void half_bridge_start(float pwm_duty);

/** Stop the PWM generation
 */
void half_bridge_stop();

/** Get status of the PWM output
 *
 * @returns True if PWM output enabled
 */
bool half_bridge_enabled();

/** Set the duty cycle of the PWM signal
 *
 * @param duty Duty cycle between 0.0 and 1.0
 */
void half_bridge_set_duty_cycle(float duty);

/** Adjust the duty cycle with minimum step size
 *
 * @param delta Number of steps (positive or negative)
 */
void half_bridge_duty_cycle_step(int delta);

/** Read the currently set duty cycle
 *
 * @returns Duty cycle between 0.0 and 1.0
 */
float half_bridge_get_duty_cycle();

#endif /* HALF_BRIDGE_H */
