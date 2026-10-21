/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBRESOLAR_DRIVERS_HALF_BRIDGE_H_
#define LIBRESOLAR_DRIVERS_HALF_BRIDGE_H_

/**
 * @file
 * @brief Half-bridge driver API
 *
 * A half bridge consists of a high-side and a low-side switch driven by two complementary PWM
 * signals with a dead time in between, e.g. as used in synchronous buck or boost converters.
 *
 * The PWM duty cycle refers to the high-side switch. It is set via a compare value between 0 and
 * the period value of the underlying timer, so that the duty cycle can be changed with the
 * minimum possible step size. Drivers clamp the compare value to configurable limits to protect
 * the hardware from invalid settings.
 */

#include <zephyr/device.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup half_bridge_interface Half-bridge
 * @{
 */

/** @cond INTERNAL_HIDDEN */

__subsystem struct half_bridge_driver_api
{
    int (*start)(const struct device *dev);
    int (*stop)(const struct device *dev);
    bool (*is_enabled)(const struct device *dev);
    int (*set_compare)(const struct device *dev, uint32_t compare);
    uint32_t (*get_compare)(const struct device *dev);
    uint32_t (*get_period)(const struct device *dev);
    int (*set_compare_limits)(const struct device *dev, uint32_t min, uint32_t max);
    int (*force_low_side)(const struct device *dev);
};

/** @endcond */

/**
 * Start the PWM generation
 *
 * A valid compare value has to be set before starting. Drivers may refuse to start if the
 * compare value is zero.
 *
 * @param dev Half-bridge device
 *
 * @retval 0 on success
 * @retval -EINVAL if the output could not be started with the current compare value
 * @retval -EBUSY if the low-side switch is forced on
 */
static inline int half_bridge_start(const struct device *dev)
{
    return DEVICE_API_GET(half_bridge, dev)->start(dev);
}

/**
 * Stop the PWM generation, i.e. switch off both high-side and low-side switches
 *
 * This also ends the state entered with half_bridge_force_low_side().
 *
 * @param dev Half-bridge device
 *
 * @retval 0 on success
 */
static inline int half_bridge_stop(const struct device *dev)
{
    return DEVICE_API_GET(half_bridge, dev)->stop(dev);
}

/**
 * Get status of the PWM output
 *
 * @param dev Half-bridge device
 *
 * @returns true if PWM output is enabled
 */
static inline bool half_bridge_is_enabled(const struct device *dev)
{
    return DEVICE_API_GET(half_bridge, dev)->is_enabled(dev);
}

/**
 * Set raw timer compare value
 *
 * This function allows to change the PWM with minimum step size. The value is clamped to the
 * limits set with half_bridge_set_compare_limits().
 *
 * @param dev Half-bridge device
 * @param compare Timer compare value (between 0 and period)
 *
 * @retval 0 on success
 * @retval -EBUSY if the low-side switch is forced on (setting is ignored)
 */
static inline int half_bridge_set_compare(const struct device *dev, uint32_t compare)
{
    return DEVICE_API_GET(half_bridge, dev)->set_compare(dev, compare);
}

/**
 * Get raw timer compare value
 *
 * @param dev Half-bridge device
 *
 * @returns Timer compare value (between 0 and period)
 */
static inline uint32_t half_bridge_get_compare(const struct device *dev)
{
    return DEVICE_API_GET(half_bridge, dev)->get_compare(dev);
}

/**
 * Get timer period value
 *
 * The duty cycle is calculated as compare / period.
 *
 * @param dev Half-bridge device
 *
 * @returns Timer period value
 */
static inline uint32_t half_bridge_get_period(const struct device *dev)
{
    return DEVICE_API_GET(half_bridge, dev)->get_period(dev);
}

/**
 * Set limits for the raw timer compare value
 *
 * The current compare value is clamped to the new limits.
 *
 * @param dev Half-bridge device
 * @param min Minimum compare value (e.g. for limiting the input voltage)
 * @param max Maximum compare value (e.g. for charge pump of high-side gate driver)
 *
 * @retval 0 on success
 * @retval -EINVAL if min > max or max > period
 * @retval -EBUSY if the low-side switch is forced on (setting is ignored)
 */
static inline int half_bridge_set_compare_limits(const struct device *dev, uint32_t min,
                                                 uint32_t max)
{
    return DEVICE_API_GET(half_bridge, dev)->set_compare_limits(dev, min, max);
}

/**
 * Switch the low-side switch continuously on and the high-side switch off
 *
 * This bypasses the compare value limits and is intended for safety measures, e.g. to blow a
 * fuse at the low-side terminal by shorting it through the inductor. The state is kept until
 * half_bridge_stop() is called. Changes of the compare value or its limits are rejected in the
 * meantime.
 *
 * @param dev Half-bridge device
 *
 * @retval 0 on success
 */
static inline int half_bridge_force_low_side(const struct device *dev)
{
    return DEVICE_API_GET(half_bridge, dev)->force_low_side(dev);
}

/**
 * Set the duty cycle of the PWM signal
 *
 * The resulting compare value is clamped to the configured limits.
 *
 * @param dev Half-bridge device
 * @param duty Duty cycle between 0.0 and 1.0
 *
 * @retval 0 on success
 * @retval -EINVAL if duty cycle is out of range (setting is ignored)
 */
static inline int half_bridge_set_duty_cycle(const struct device *dev, float duty)
{
    if (duty < 0.0F || duty > 1.0F) {
        return -EINVAL;
    }

    return half_bridge_set_compare(dev, half_bridge_get_period(dev) * duty);
}

/**
 * Read the currently set duty cycle
 *
 * @param dev Half-bridge device
 *
 * @returns Duty cycle between 0.0 and 1.0
 */
static inline float half_bridge_get_duty_cycle(const struct device *dev)
{
    return (float)half_bridge_get_compare(dev) / half_bridge_get_period(dev);
}

/**
 * Set duty cycle limits
 *
 * @param dev Half-bridge device
 * @param min_duty Minimum duty cycle (e.g. 0.5 for limiting input voltage)
 * @param max_duty Maximum duty cycle (e.g. 0.97 for charge pump)
 *
 * @retval 0 on success
 * @retval -EINVAL if limits are out of range
 */
static inline int half_bridge_set_duty_limits(const struct device *dev, float min_duty,
                                              float max_duty)
{
    if (min_duty < 0.0F || max_duty > 1.0F || min_duty > max_duty) {
        return -EINVAL;
    }

    uint32_t period = half_bridge_get_period(dev);

    return half_bridge_set_compare_limits(dev, period * min_duty, period * max_duty);
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* LIBRESOLAR_DRIVERS_HALF_BRIDGE_H_ */
