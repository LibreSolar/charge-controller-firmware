/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HARDWARE_H
#define HARDWARE_H

/** @file
 *
 * @brief
 * Hardware-specific functions like timers, watchdog, bootloader
 */

#include <stdint.h>

/**
 * Initialization of IWDG
 */
void watchdog_init();

/**
 * Register watchdog for this thread
 *
 * @param timeout_ms Timeout in milliseconds
 *
 * @returns Assigned channel number or -1 in case of error
 */
int watchdog_register(uint32_t timeout_ms);

/**
 * Must be called after all channels have been registered
 */
void watchdog_start();

/**
 * Feed/Check-in watchdog for given channel
 *
 * @param channel Channel number assigned by watchdog_register function
 */
void watchdog_feed(int channel);

/** Timer for system control (main DC/DC or PWM control loop) is started
 *
 * @param freq_Hz Frequency in Hz (10 Hz suggested)
 */
void control_timer_start(int freq_Hz);

/** DC/DC or PWM control loop (implemented in main.cpp)
 */
void system_control();

/** Reset device and start STM32 internal bootloader
 */
void start_stm32_bootloader();

/**
 * Reset device
 */
void reset_device();

#endif /* HARDWARE_H */