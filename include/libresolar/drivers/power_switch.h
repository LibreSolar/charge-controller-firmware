/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBRESOLAR_DRIVERS_POWER_SWITCH_H_
#define LIBRESOLAR_DRIVERS_POWER_SWITCH_H_

/**
 * @file
 * @brief Power switch driver API
 *
 * A power switch connects or disconnects a power path, e.g. a load or USB output. Besides the
 * switching itself, drivers may provide a power-good feedback from the switch or its gate driver
 * and report faults detected in hardware, e.g. a short circuit that already switched off the
 * output without any software interaction.
 */

#include <zephyr/device.h>

#include <errno.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup power_switch_interface Power switch
 * @{
 */

/**
 * Callback for faults detected and handled by the hardware
 *
 * The switch is already off when the callback is called. It may be called from an ISR.
 *
 * @param dev Power switch device
 * @param user_data User data provided with power_switch_set_fault_callback()
 */
typedef void (*power_switch_fault_callback_t)(const struct device *dev, void *user_data);

/** @cond INTERNAL_HIDDEN */

__subsystem struct power_switch_driver_api
{
    int (*set)(const struct device *dev, bool on);
    bool (*is_on)(const struct device *dev);
    int (*power_good)(const struct device *dev);
    int (*set_fault_callback)(const struct device *dev, power_switch_fault_callback_t callback,
                              void *user_data);
};

/** @endcond */

/**
 * Switch the output on
 *
 * @param dev Power switch device
 *
 * @retval 0 on success
 * @retval -errno otherwise
 */
static inline int power_switch_on(const struct device *dev)
{
    return DEVICE_API_GET(power_switch, dev)->set(dev, true);
}

/**
 * Switch the output off
 *
 * Can be called from an ISR.
 *
 * @param dev Power switch device
 *
 * @retval 0 on success
 * @retval -errno otherwise
 */
static inline int power_switch_off(const struct device *dev)
{
    return DEVICE_API_GET(power_switch, dev)->set(dev, false);
}

/**
 * Get the commanded state of the switch
 *
 * The state changes to off if the hardware switched off the output because of a fault.
 *
 * @param dev Power switch device
 *
 * @returns true if the switch is on
 */
static inline bool power_switch_is_on(const struct device *dev)
{
    return DEVICE_API_GET(power_switch, dev)->is_on(dev);
}

/**
 * Check the power-good feedback of the switch
 *
 * @param dev Power switch device
 *
 * @retval 1 if the power is good or the switch is off
 * @retval 0 if the switch is on, but the power-good signal indicates a failure
 * @retval -ENOSYS if the switch does not provide a power-good feedback
 */
static inline int power_switch_power_good(const struct device *dev)
{
    const struct power_switch_driver_api *api = DEVICE_API_GET(power_switch, dev);

    if (api->power_good == NULL) {
        return -ENOSYS;
    }

    return api->power_good(dev);
}

/**
 * Set callback for faults detected and handled by the hardware (e.g. short circuit)
 *
 * @param dev Power switch device
 * @param callback Callback function or NULL to remove the callback
 * @param user_data User data passed to the callback
 *
 * @retval 0 on success
 * @retval -ENOSYS if the switch does not detect faults in hardware
 */
static inline int power_switch_set_fault_callback(const struct device *dev,
                                                  power_switch_fault_callback_t callback,
                                                  void *user_data)
{
    const struct power_switch_driver_api *api = DEVICE_API_GET(power_switch, dev);

    if (api->set_fault_callback == NULL) {
        return -ENOSYS;
    }

    return api->set_fault_callback(dev, callback, user_data);
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* LIBRESOLAR_DRIVERS_POWER_SWITCH_H_ */
