/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBRESOLAR_DRIVERS_POWER_SWITCH_EMUL_H_
#define LIBRESOLAR_DRIVERS_POWER_SWITCH_EMUL_H_

/**
 * @file
 * @brief Backend API of the emulated power switch for tests
 */

#include <zephyr/device.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set the power-good feedback reported while the switch is on
 *
 * @param dev Emulated power switch device
 * @param power_good Power-good state
 */
void power_switch_emul_set_power_good(const struct device *dev, bool power_good);

/**
 * Trigger events as if they were detected by the hardware
 *
 * A fault event switches off the output before the callback is called.
 *
 * @param dev Emulated power switch device
 * @param events Bitmask of events according to enum power_switch_event
 */
void power_switch_emul_trigger_events(const struct device *dev, uint32_t events);

#ifdef __cplusplus
}
#endif

#endif /* LIBRESOLAR_DRIVERS_POWER_SWITCH_EMUL_H_ */
