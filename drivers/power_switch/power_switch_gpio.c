/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Power switch driven by a GPIO
 *
 * Optionally, the GPIO is used as a power-good feedback of a latching switch: The pin is driven
 * active for a short time to turn the switch on and afterwards configured as an input. The switch
 * latches its state and pulls the pin low if it turned off because of a fault.
 *
 * An optional PWM signal drives the charge pump of a high-side gate driver.
 */

#define DT_DRV_COMPAT libresolar_gpio_power_switch

#include <libresolar/drivers/power_switch.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/spinlock.h>

LOG_MODULE_REGISTER(power_switch_gpio, CONFIG_POWER_SWITCH_LOG_LEVEL);

/** Time the GPIO is driven active before switching to power-good input */
#define LATCH_TIME_MS 50

struct power_switch_gpio_config
{
    struct gpio_dt_spec gpio;
#ifdef CONFIG_POWER_SWITCH_GPIO_CHARGE_PUMP
    struct pwm_dt_spec charge_pump;
    bool has_charge_pump;
#endif
    bool latching_pgood;
};

struct power_switch_gpio_data
{
    const struct device *dev;
    struct k_work_delayable latch_work;
    struct k_spinlock lock;
    bool on;
};

static void latch_work_handler(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct power_switch_gpio_data *data =
        CONTAINER_OF(dwork, struct power_switch_gpio_data, latch_work);
    const struct power_switch_gpio_config *config = data->dev->config;

    K_SPINLOCK(&data->lock)
    {
        /* the switch might have been turned off in the meantime */
        if (data->on) {
            gpio_pin_configure_dt(&config->gpio, GPIO_INPUT);
        }
    }
}

static int power_switch_gpio_set(const struct device *dev, bool on)
{
    const struct power_switch_gpio_config *config = dev->config;
    struct power_switch_gpio_data *data = dev->data;
    int err;

    if (!on && config->latching_pgood) {
        /* returns immediately, so it can be called from an ISR */
        k_work_cancel_delayable(&data->latch_work);
    }

    K_SPINLOCK(&data->lock)
    {
        /* configure instead of set, as the pin might be an input in case of latching pgood */
        err = gpio_pin_configure_dt(&config->gpio, on ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE);
        data->on = on && err == 0;
    }

    if (err != 0) {
        LOG_ERR("Failed to switch %s: %d", on ? "on" : "off", err);
        return err;
    }

    if (on && config->latching_pgood) {
        k_work_reschedule(&data->latch_work, K_MSEC(LATCH_TIME_MS));
    }

    return 0;
}

static bool power_switch_gpio_is_on(const struct device *dev)
{
    struct power_switch_gpio_data *data = dev->data;

    return data->on;
}

static int power_switch_gpio_power_good(const struct device *dev)
{
    const struct power_switch_gpio_config *config = dev->config;
    struct power_switch_gpio_data *data = dev->data;

    if (!config->latching_pgood) {
        return -ENOSYS;
    }

    if (!data->on || k_work_delayable_is_pending(&data->latch_work)) {
        /* switch is off or still actively driven on */
        return 1;
    }

    int ret = gpio_pin_get_dt(&config->gpio);

    return ret < 0 ? ret : (ret > 0);
}

static DEVICE_API(power_switch, power_switch_gpio_api) = {
    .set = power_switch_gpio_set,
    .is_on = power_switch_gpio_is_on,
    .power_good = power_switch_gpio_power_good,
};

static int power_switch_gpio_init(const struct device *dev)
{
    const struct power_switch_gpio_config *config = dev->config;
    struct power_switch_gpio_data *data = dev->data;
    int err;

    data->dev = dev;
    k_work_init_delayable(&data->latch_work, latch_work_handler);

    if (!gpio_is_ready_dt(&config->gpio)) {
        LOG_ERR("GPIO not ready");
        return -ENODEV;
    }

    err = gpio_pin_configure_dt(&config->gpio, GPIO_OUTPUT_INACTIVE);
    if (err != 0) {
        return err;
    }

#ifdef CONFIG_POWER_SWITCH_GPIO_CHARGE_PUMP
    if (config->has_charge_pump) {
        if (!pwm_is_ready_dt(&config->charge_pump)) {
            LOG_ERR("Charge pump PWM not ready");
            return -ENODEV;
        }

        /* runs continuously with 50% duty cycle */
        err = pwm_set_pulse_dt(&config->charge_pump, config->charge_pump.period / 2);
        if (err != 0) {
            LOG_ERR("Failed to start charge pump: %d", err);
            return err;
        }
    }
#endif

    return 0;
}

#define CHARGE_PUMP_PWM_SPEC(n) \
    { \
        .dev = DEVICE_DT_GET(DT_INST_PHANDLE(n, charge_pump_pwms)), \
        .channel = DT_INST_PHA(n, charge_pump_pwms, channel), \
        .period = DT_INST_PHA(n, charge_pump_pwms, period), \
        .flags = DT_INST_PHA_BY_IDX_OR(n, charge_pump_pwms, 0, flags, 0), \
    }

#ifdef CONFIG_POWER_SWITCH_GPIO_CHARGE_PUMP
#define CHARGE_PUMP_CONFIG(n) \
    IF_ENABLED(DT_INST_NODE_HAS_PROP(n, charge_pump_pwms), \
               (.charge_pump = CHARGE_PUMP_PWM_SPEC(n), .has_charge_pump = true, ))
#else
#define CHARGE_PUMP_CONFIG(n)
#endif

#define POWER_SWITCH_GPIO_INIT(n) \
    static const struct power_switch_gpio_config power_switch_gpio_config_##n = { \
        .gpio = GPIO_DT_SPEC_INST_GET(n, gpios), \
        CHARGE_PUMP_CONFIG(n).latching_pgood = DT_INST_PROP(n, latching_pgood), \
    }; \
\
    static struct power_switch_gpio_data power_switch_gpio_data_##n; \
\
    DEVICE_DT_INST_DEFINE(n, power_switch_gpio_init, NULL, &power_switch_gpio_data_##n, \
                          &power_switch_gpio_config_##n, POST_KERNEL, \
                          CONFIG_POWER_SWITCH_INIT_PRIORITY, &power_switch_gpio_api);

DT_INST_FOREACH_STATUS_OKAY(POWER_SWITCH_GPIO_INIT)
