/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libresolar/drivers/power_switch.h>
#include <libresolar/drivers/power_switch_emul.h>

#include <zephyr/device.h>
#include <zephyr/ztest.h>

static const struct device *const emul = DEVICE_DT_GET(DT_NODELABEL(emul_switch));

static uint32_t callback_events;
static int callback_count;

static void callback(const struct device *dev, uint32_t events, void *user_data)
{
    zassert_equal(dev, emul);
    zassert_equal(user_data, &callback_count);

    callback_events = events;
    callback_count++;
}

static void *power_switch_emul_setup(void)
{
    zassert_true(device_is_ready(emul));

    return NULL;
}

static void power_switch_emul_before(void *fixture)
{
    ARG_UNUSED(fixture);

    zassert_ok(power_switch_off(emul));
    zassert_ok(power_switch_set_duty_cycle(emul, 1.0F));
    zassert_ok(power_switch_set_callback(emul, 0, NULL, NULL));
    power_switch_emul_set_power_good(emul, true);

    callback_events = 0;
    callback_count = 0;
}

ZTEST(power_switch_emul, test_duty_cycle)
{
    zassert_ok(power_switch_set_duty_cycle(emul, 0.25F));
    zassert_equal(power_switch_get_duty_cycle(emul), 0.25F);

    zassert_equal(power_switch_set_duty_cycle(emul, 1.1F), -EINVAL);
    zassert_equal(power_switch_set_duty_cycle(emul, -0.1F), -EINVAL);
    zassert_equal(power_switch_get_duty_cycle(emul), 0.25F);
}

ZTEST(power_switch_emul, test_conducting)
{
    zassert_false(power_switch_is_conducting(emul));

    zassert_ok(power_switch_on(emul));
    zassert_true(power_switch_is_conducting(emul));

    zassert_ok(power_switch_set_duty_cycle(emul, 0.0F));
    zassert_false(power_switch_is_conducting(emul));
}

ZTEST(power_switch_emul, test_power_good)
{
    power_switch_emul_set_power_good(emul, false);
    zassert_equal(power_switch_power_good(emul), 1, "off switch must report power good");

    zassert_ok(power_switch_on(emul));
    zassert_equal(power_switch_power_good(emul), 0);
}

ZTEST(power_switch_emul, test_fault_switches_off)
{
    zassert_ok(
        power_switch_set_callback(emul, POWER_SWITCH_EVENT_FAULT, callback, &callback_count));
    zassert_ok(power_switch_on(emul));

    power_switch_emul_trigger_events(emul, POWER_SWITCH_EVENT_FAULT);

    zassert_false(power_switch_is_on(emul));
    zassert_equal(callback_count, 1);
    zassert_equal(callback_events, POWER_SWITCH_EVENT_FAULT);
}

ZTEST(power_switch_emul, test_callback_only_for_requested_events)
{
    zassert_ok(
        power_switch_set_callback(emul, POWER_SWITCH_EVENT_PWM_EDGE, callback, &callback_count));

    power_switch_emul_trigger_events(emul, POWER_SWITCH_EVENT_FAULT);
    zassert_equal(callback_count, 0);

    power_switch_emul_trigger_events(emul, POWER_SWITCH_EVENT_PWM_EDGE);
    zassert_equal(callback_count, 1);
    zassert_equal(callback_events, POWER_SWITCH_EVENT_PWM_EDGE);
}

ZTEST_SUITE(power_switch_emul, NULL, power_switch_emul_setup, power_switch_emul_before, NULL, NULL);
