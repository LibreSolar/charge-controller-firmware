/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libresolar/drivers/power_switch.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/drivers/pwm/pwm_fake.h>
#include <zephyr/fff.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

DEFINE_FFF_GLOBALS;

#define BASIC_NODE       DT_NODELABEL(basic_switch)
#define LATCHING_NODE    DT_NODELABEL(latching_switch)
#define CHARGE_PUMP_NODE DT_NODELABEL(charge_pump_switch)

/* must be larger than the latch time of the driver */
#define LATCH_WAIT_MS 100

static const struct device *const basic = DEVICE_DT_GET(BASIC_NODE);
static const struct device *const latching = DEVICE_DT_GET(LATCHING_NODE);

static const struct gpio_dt_spec basic_gpio = GPIO_DT_SPEC_GET(BASIC_NODE, gpios);
static const struct gpio_dt_spec latching_gpio = GPIO_DT_SPEC_GET(LATCHING_NODE, gpios);

/* charge pump calls during driver initialization (fake is reset before each test) */
static unsigned int charge_pump_call_count;
static uint32_t charge_pump_period;
static uint32_t charge_pump_pulse;

static void *power_switch_setup(void)
{
    charge_pump_call_count = fake_pwm_set_cycles_fake.call_count;
    charge_pump_period = fake_pwm_set_cycles_fake.arg2_val;
    charge_pump_pulse = fake_pwm_set_cycles_fake.arg3_val;

    zassert_true(device_is_ready(basic));
    zassert_true(device_is_ready(latching));

    return NULL;
}

static void power_switch_before(void *fixture)
{
    ARG_UNUSED(fixture);

    zassert_ok(power_switch_off(basic));
    zassert_ok(power_switch_off(latching));
}

static bool is_input(const struct gpio_dt_spec *spec)
{
    gpio_flags_t flags;

    zassert_ok(gpio_emul_flags_get(spec->port, spec->pin, &flags));

    return (flags & GPIO_INPUT) != 0 && (flags & GPIO_OUTPUT) == 0;
}

ZTEST(power_switch, test_off_after_init)
{
    /* active low, so the physical level must be high */
    zassert_equal(gpio_emul_output_get(basic_gpio.port, basic_gpio.pin), 1);
    zassert_false(power_switch_is_on(basic));
}

ZTEST(power_switch, test_on_off)
{
    zassert_ok(power_switch_on(basic));
    zassert_true(power_switch_is_on(basic));
    zassert_equal(gpio_emul_output_get(basic_gpio.port, basic_gpio.pin), 0);

    zassert_ok(power_switch_off(basic));
    zassert_false(power_switch_is_on(basic));
    zassert_equal(gpio_emul_output_get(basic_gpio.port, basic_gpio.pin), 1);
}

ZTEST(power_switch, test_optional_functions_not_supported)
{
    zassert_equal(power_switch_power_good(basic), -ENOSYS);
    zassert_equal(power_switch_set_duty_cycle(basic, 0.5F), -ENOSYS);
    zassert_equal(power_switch_set_callback(basic, POWER_SWITCH_EVENT_FAULT, NULL, NULL), -ENOSYS);
}

ZTEST(power_switch, test_default_duty_cycle_and_conducting)
{
    zassert_equal(power_switch_get_duty_cycle(basic), 0.0F);
    zassert_false(power_switch_is_conducting(basic));

    zassert_ok(power_switch_on(basic));
    zassert_equal(power_switch_get_duty_cycle(basic), 1.0F);
    zassert_true(power_switch_is_conducting(basic));
}

ZTEST(power_switch, test_latching_releases_pin)
{
    zassert_ok(power_switch_on(latching));
    zassert_false(is_input(&latching_gpio));
    zassert_equal(gpio_emul_output_get(latching_gpio.port, latching_gpio.pin), 1);
    zassert_equal(power_switch_power_good(latching), 1);

    k_msleep(LATCH_WAIT_MS);

    zassert_true(is_input(&latching_gpio));
    zassert_true(power_switch_is_on(latching));
}

ZTEST(power_switch, test_latching_power_good)
{
    zassert_ok(power_switch_on(latching));
    k_msleep(LATCH_WAIT_MS);

    zassert_ok(gpio_emul_input_set(latching_gpio.port, latching_gpio.pin, 1));
    zassert_equal(power_switch_power_good(latching), 1);

    /* switch latched off because of a fault */
    zassert_ok(gpio_emul_input_set(latching_gpio.port, latching_gpio.pin, 0));
    zassert_equal(power_switch_power_good(latching), 0);

    /* power good is reported for a switch commanded off */
    zassert_ok(power_switch_off(latching));
    zassert_equal(power_switch_power_good(latching), 1);
    zassert_false(is_input(&latching_gpio));
    zassert_equal(gpio_emul_output_get(latching_gpio.port, latching_gpio.pin), 0);
}

ZTEST(power_switch, test_latching_off_before_release)
{
    zassert_ok(power_switch_on(latching));
    zassert_ok(power_switch_off(latching));

    k_msleep(LATCH_WAIT_MS);

    /* pin must stay an output driving the switch off */
    zassert_false(is_input(&latching_gpio));
    zassert_equal(gpio_emul_output_get(latching_gpio.port, latching_gpio.pin), 0);
}

ZTEST(power_switch, test_charge_pump_started_at_init)
{
    zassert_true(device_is_ready(DEVICE_DT_GET(CHARGE_PUMP_NODE)));

    zassert_equal(charge_pump_call_count, 1);
    zassert_true(charge_pump_period > 0);
    zassert_equal(charge_pump_pulse, charge_pump_period / 2);
}

ZTEST_SUITE(power_switch, NULL, power_switch_setup, power_switch_before, NULL, NULL);
