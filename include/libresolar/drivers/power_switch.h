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
 *
 * Switches supporting PWM (e.g. used in PWM solar charge controllers) are switched on and off
 * periodically with the configured duty cycle while they are on.
 */

#include <zephyr/device.h>
#include <zephyr/sys/util.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup power_switch_interface Power switch
 * @{
 */

/**
 * Power switch events
 */
enum power_switch_event
{
    /**
     * Fault detected and handled by the hardware (e.g. short circuit)
     *
     * The switch is already off when the callback is called.
     */
    POWER_SWITCH_EVENT_FAULT = BIT(0),

    /**
     * Rising edge of the PWM signal, i.e. the switch starts conducting
     *
     * Not generated if the duty cycle is 0% or 100%.
     */
    POWER_SWITCH_EVENT_PWM_EDGE = BIT(1),
};

/**
 * Callback for power switch events
 *
 * The callback may be called from an ISR.
 *
 * @param dev Power switch device
 * @param events Bitmask of events according to enum power_switch_event
 * @param user_data User data provided with power_switch_set_callback()
 */
typedef void (*power_switch_callback_t)(const struct device *dev, uint32_t events, void *user_data);

/** @cond INTERNAL_HIDDEN */

__subsystem struct power_switch_driver_api
{
    int (*set)(const struct device *dev, bool on);
    bool (*is_on)(const struct device *dev);
    bool (*is_conducting)(const struct device *dev);
    int (*power_good)(const struct device *dev);
    int (*set_duty_cycle)(const struct device *dev, float duty);
    float (*get_duty_cycle)(const struct device *dev);
    int (*set_callback)(const struct device *dev, uint32_t events, power_switch_callback_t callback,
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
 * Check if the switch is currently conducting
 *
 * For switches with PWM, this is the momentary state of the PWM signal. For other switches, it is
 * the same as power_switch_is_on().
 *
 * Can be called from an ISR.
 *
 * @param dev Power switch device
 *
 * @returns true if the switch is conducting
 */
static inline bool power_switch_is_conducting(const struct device *dev)
{
    const struct power_switch_driver_api *api = DEVICE_API_GET(power_switch, dev);

    if (api->is_conducting == NULL) {
        return api->is_on(dev);
    }

    return api->is_conducting(dev);
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
 * Set the PWM duty cycle
 *
 * The duty cycle can be set before switching on and is kept while the switch is off.
 *
 * @param dev Power switch device
 * @param duty Duty cycle between 0.0 and 1.0
 *
 * @retval 0 on success
 * @retval -EINVAL if duty cycle is out of range (setting is ignored)
 * @retval -ENOSYS if the switch does not support PWM
 */
static inline int power_switch_set_duty_cycle(const struct device *dev, float duty)
{
    const struct power_switch_driver_api *api = DEVICE_API_GET(power_switch, dev);

    if (api->set_duty_cycle == NULL) {
        return -ENOSYS;
    }

    if (duty < 0.0F || duty > 1.0F) {
        return -EINVAL;
    }

    return api->set_duty_cycle(dev, duty);
}

/**
 * Get the PWM duty cycle
 *
 * For switches without PWM support, the duty cycle is 1.0 while the switch is on.
 *
 * @param dev Power switch device
 *
 * @returns Duty cycle between 0.0 and 1.0
 */
static inline float power_switch_get_duty_cycle(const struct device *dev)
{
    const struct power_switch_driver_api *api = DEVICE_API_GET(power_switch, dev);

    if (api->get_duty_cycle == NULL) {
        return api->is_on(dev) ? 1.0F : 0.0F;
    }

    return api->get_duty_cycle(dev);
}

/**
 * Set callback for power switch events
 *
 * Only a single callback is supported. A new callback replaces the previous one.
 *
 * @param dev Power switch device
 * @param events Bitmask of events (enum power_switch_event) the callback should be called for
 * @param callback Callback function or NULL to remove the callback
 * @param user_data User data passed to the callback
 *
 * @retval 0 on success
 * @retval -ENOTSUP if one of the requested events is not supported by the switch
 * @retval -ENOSYS if the switch does not support any events
 */
static inline int power_switch_set_callback(const struct device *dev, uint32_t events,
                                            power_switch_callback_t callback, void *user_data)
{
    const struct power_switch_driver_api *api = DEVICE_API_GET(power_switch, dev);

    if (api->set_callback == NULL) {
        return -ENOSYS;
    }

    return api->set_callback(dev, events, callback, user_data);
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* LIBRESOLAR_DRIVERS_POWER_SWITCH_H_ */
