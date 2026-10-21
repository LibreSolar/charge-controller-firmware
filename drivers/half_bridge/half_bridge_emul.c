/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Emulated half-bridge driver for tests on native_sim
 *
 * Behaves like an edge-aligned STM32 timer, i.e. the period is clock-frequency / frequency.
 */

#define DT_DRV_COMPAT libresolar_half_bridge_emul

#include <libresolar/drivers/half_bridge.h>

#include <zephyr/device.h>
#include <zephyr/sys/util.h>

struct half_bridge_emul_config
{
    uint32_t clock_frequency;
    uint32_t frequency;
};

struct half_bridge_emul_data
{
    uint32_t period;
    uint32_t compare;
    uint32_t compare_min;
    uint32_t compare_max;
    bool enabled;
    bool low_side_forced;
};

static int half_bridge_emul_start(const struct device *dev)
{
    struct half_bridge_emul_data *data = dev->data;

    if (data->low_side_forced) {
        return -EBUSY;
    }

    data->enabled = true;

    return 0;
}

static int half_bridge_emul_stop(const struct device *dev)
{
    struct half_bridge_emul_data *data = dev->data;

    data->enabled = false;
    data->low_side_forced = false;

    return 0;
}

static bool half_bridge_emul_is_enabled(const struct device *dev)
{
    struct half_bridge_emul_data *data = dev->data;

    return data->enabled;
}

static int half_bridge_emul_set_compare(const struct device *dev, uint32_t compare)
{
    struct half_bridge_emul_data *data = dev->data;

    if (data->low_side_forced) {
        return -EBUSY;
    }

    data->compare = CLAMP(compare, data->compare_min, data->compare_max);

    return 0;
}

static uint32_t half_bridge_emul_get_compare(const struct device *dev)
{
    struct half_bridge_emul_data *data = dev->data;

    return data->compare;
}

static uint32_t half_bridge_emul_get_period(const struct device *dev)
{
    struct half_bridge_emul_data *data = dev->data;

    return data->period;
}

static int half_bridge_emul_set_compare_limits(const struct device *dev, uint32_t min, uint32_t max)
{
    struct half_bridge_emul_data *data = dev->data;

    if (data->low_side_forced) {
        return -EBUSY;
    }

    if (min > max || max > data->period) {
        return -EINVAL;
    }

    data->compare_min = min;
    data->compare_max = max;

    return half_bridge_emul_set_compare(dev, data->compare);
}

static int half_bridge_emul_force_low_side(const struct device *dev)
{
    struct half_bridge_emul_data *data = dev->data;

    data->compare = 0;
    data->enabled = true;
    data->low_side_forced = true;

    return 0;
}

static DEVICE_API(half_bridge, half_bridge_emul_api) = {
    .start = half_bridge_emul_start,
    .stop = half_bridge_emul_stop,
    .is_enabled = half_bridge_emul_is_enabled,
    .set_compare = half_bridge_emul_set_compare,
    .get_compare = half_bridge_emul_get_compare,
    .get_period = half_bridge_emul_get_period,
    .set_compare_limits = half_bridge_emul_set_compare_limits,
    .force_low_side = half_bridge_emul_force_low_side,
};

static int half_bridge_emul_init(const struct device *dev)
{
    const struct half_bridge_emul_config *config = dev->config;
    struct half_bridge_emul_data *data = dev->data;

    data->period = config->clock_frequency / config->frequency;
    data->compare_max = data->period;

    return 0;
}

#define HALF_BRIDGE_EMUL_INIT(n) \
    static const struct half_bridge_emul_config half_bridge_emul_config_##n = { \
        .clock_frequency = DT_INST_PROP(n, clock_frequency), \
        .frequency = DT_INST_PROP(n, frequency), \
    }; \
\
    static struct half_bridge_emul_data half_bridge_emul_data_##n; \
\
    DEVICE_DT_INST_DEFINE(n, half_bridge_emul_init, NULL, &half_bridge_emul_data_##n, \
                          &half_bridge_emul_config_##n, POST_KERNEL, \
                          CONFIG_HALF_BRIDGE_INIT_PRIORITY, &half_bridge_emul_api);

DT_INST_FOREACH_STATUS_OKAY(HALF_BRIDGE_EMUL_INIT)
