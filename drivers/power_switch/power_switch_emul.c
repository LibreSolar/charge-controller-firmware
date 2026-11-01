/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Emulated power switch for tests on native_sim
 *
 * Supports all features of the power switch API. Power-good failures and events can be injected
 * via the functions in power_switch_emul.h.
 */

#define DT_DRV_COMPAT libresolar_power_switch_emul

#include <libresolar/drivers/power_switch.h>
#include <libresolar/drivers/power_switch_emul.h>

#include <zephyr/device.h>
#include <zephyr/irq.h>

struct power_switch_emul_data
{
    power_switch_callback_t callback;
    void *user_data;
    uint32_t callback_events;
    float duty;
    bool on;
    bool power_good;
};

static int power_switch_emul_set(const struct device *dev, bool on)
{
    struct power_switch_emul_data *data = dev->data;

    data->on = on;

    return 0;
}

static bool power_switch_emul_is_on(const struct device *dev)
{
    struct power_switch_emul_data *data = dev->data;

    return data->on;
}

static bool power_switch_emul_is_conducting(const struct device *dev)
{
    struct power_switch_emul_data *data = dev->data;

    /* no actual PWM signal emulated, so the switch is considered conducting if duty > 0 */
    return data->on && data->duty > 0.0F;
}

static int power_switch_emul_power_good(const struct device *dev)
{
    struct power_switch_emul_data *data = dev->data;

    return !data->on || data->power_good;
}

static int power_switch_emul_set_duty_cycle(const struct device *dev, float duty)
{
    struct power_switch_emul_data *data = dev->data;

    data->duty = duty;

    return 0;
}

static float power_switch_emul_get_duty_cycle(const struct device *dev)
{
    struct power_switch_emul_data *data = dev->data;

    return data->duty;
}

static int power_switch_emul_set_callback(const struct device *dev, uint32_t events,
                                          power_switch_callback_t callback, void *user_data)
{
    struct power_switch_emul_data *data = dev->data;
    unsigned int key = irq_lock();

    data->callback = callback;
    data->user_data = user_data;
    data->callback_events = events;

    irq_unlock(key);

    return 0;
}

void power_switch_emul_set_power_good(const struct device *dev, bool power_good)
{
    struct power_switch_emul_data *data = dev->data;

    data->power_good = power_good;
}

void power_switch_emul_trigger_events(const struct device *dev, uint32_t events)
{
    struct power_switch_emul_data *data = dev->data;

    if (events & POWER_SWITCH_EVENT_FAULT) {
        data->on = false;
    }

    events &= data->callback_events;

    if (events != 0 && data->callback != NULL) {
        data->callback(dev, events, data->user_data);
    }
}

static DEVICE_API(power_switch, power_switch_emul_api) = {
    .set = power_switch_emul_set,
    .is_on = power_switch_emul_is_on,
    .is_conducting = power_switch_emul_is_conducting,
    .power_good = power_switch_emul_power_good,
    .set_duty_cycle = power_switch_emul_set_duty_cycle,
    .get_duty_cycle = power_switch_emul_get_duty_cycle,
    .set_callback = power_switch_emul_set_callback,
};

#define POWER_SWITCH_EMUL_INIT(n) \
    static struct power_switch_emul_data power_switch_emul_data_##n = { \
        .duty = 1.0F, \
        .power_good = true, \
    }; \
\
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &power_switch_emul_data_##n, NULL, POST_KERNEL, \
                          CONFIG_POWER_SWITCH_INIT_PRIORITY, &power_switch_emul_api);

DT_INST_FOREACH_STATUS_OKAY(POWER_SWITCH_EMUL_INIT)
