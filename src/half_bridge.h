/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 */

#ifndef HALF_BRIDGE_H_
#define HALF_BRIDGE_H_

#include <stdint.h>

/**
 * @file
 *
 * @brief PWM timer functions for half bridge of DC/DC converter
 */

/**
 * Initiatializes the registers to generate the PWM signal and sets duty
 * cycle limits
 *
 * @param freq_kHz Switching frequency in kHz
 * @param deadtime_ns Deadtime in ns between switching the two FETs on/off
 * @param min_duty Minimum duty cycle (e.g. 0.5 for limiting input voltage)
 * @param max_duty Maximum duty cycle (e.g. 0.97 for charge pump)
 */
void half_bridge_init(int freq_kHz, int deadtime_ns, float min_duty, float max_duty);

/**
 * Set raw timer capture/compare register
 *
 * This function allows to change the PWM with minimum step size.
 *
 * @param ccr Timer CCR value (between 0 and ARR)
 */
void half_bridge_set_ccr(uint16_t ccr);

/**
 * Get raw timer capture/compare register
 *
 * @returns Timer CCR value (between 0 and ARR)
 */
uint16_t half_bridge_get_ccr();

/**
 * Get raw timer auto-reload register
 *
 * @returns Timer ARR value
 */
uint16_t half_bridge_get_arr();

/**
 * Set the duty cycle of the PWM signal
 *
 * @param duty Duty cycle between 0.0 and 1.0
 */
void half_bridge_set_duty_cycle(float duty);

/**
 * Read the currently set duty cycle
 *
 * @returns Duty cycle between 0.0 and 1.0
 */
float half_bridge_get_duty_cycle();

/**
 * Start the PWM generation
 *
 * Important: Valid duty cycle / CCR has to be set before starting
 */
void half_bridge_start();

/**
 * Stop the PWM generation
 */
void half_bridge_stop();

/**
 * Get status of the PWM output
 *
 * @returns True if PWM output enabled
 */
bool half_bridge_enabled();

#endif /* HALF_BRIDGE_H_ */
