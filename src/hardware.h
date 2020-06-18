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

struct sw_wdt_channel {
	int64_t check_in_time;  ///< most recent check-in/feed-in time of a thread for sw_watchdog
	int32_t timeout;        ///< timeout of corresponding thread for sw_watchdog
};

/**
 * Initialization of IWDG
 */
void watchdog_init();

/**
 * Register watchdog for this thread
 *
 * @param timeout_ms Timeout in milliseconds
 */
int watchdog_register(uint32_t timeout_ms);

/**
 * Must be called after all channels have been registered
 */
void watchdog_start();

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

/**
 * Implements watchdog for multiple threads in software
 */
void sw_watchdog(struct k_timer *timer_id);

/**
 * Configure timer for software watchdog and
 * configure actual watchdog
 */
void sw_watchdog_start();

/**
 * Register sw_watchdog
 *
 * @param timeout_ms Timeout in milliseconds
 */
int sw_watchdog_register(uint32_t timeout_ms);

/**
 * Feed/Check-in sw_watchdog for given thread
 */
void sw_watchdog_feed(int thread_id);

#endif /* HARDWARE_H */