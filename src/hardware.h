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

/**
 * Callback for task watchdogs used in multiple threads
 */
void task_wdt_callback(int channel_id, void *user_data);

#endif /* HARDWARE_H */