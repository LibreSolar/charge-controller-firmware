/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HARDWARE_H
#define HARDWARE_H

/** @file
 *
 * @brief
 * Device reset and watchdog functions
 */

/**
 * Reset device
 */
void reset_device();

/**
 * Callback for task watchdogs used in multiple threads
 */
void task_wdt_callback(int channel_id, void *user_data);

#endif /* HARDWARE_H */