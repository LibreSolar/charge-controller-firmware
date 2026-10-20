/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libresolar/drivers/half_bridge.h>

#include <zephyr/device.h>
#include <zephyr/ztest.h>

#define HALF_BRIDGE_NODE DT_NODELABEL(halfbridge)

#define MAX_PWM_DUTY 0.97F
#define MIN_PWM_DUTY 0.1F
#define MID_PWM_DUTY ((MIN_PWM_DUTY + MAX_PWM_DUTY) / 2)

/* calculated duty cycle may deviate +/- DUTY_EPSILON from test target value */
#define DUTY_EPSILON 0.006F

static const struct device *const dev = DEVICE_DT_GET(HALF_BRIDGE_NODE);

static void *half_bridge_setup(void)
{
    zassert_true(device_is_ready(dev), "half-bridge device not ready");

    return NULL;
}

static void half_bridge_before(void *fixture)
{
    ARG_UNUSED(fixture);

    zassert_ok(half_bridge_set_duty_limits(dev, MIN_PWM_DUTY, MAX_PWM_DUTY));
    zassert_ok(half_bridge_set_duty_cycle(dev, MAX_PWM_DUTY));
    zassert_ok(half_bridge_stop(dev));
}

ZTEST(half_bridge, test_period_from_devicetree)
{
    uint32_t expected =
        DT_PROP(HALF_BRIDGE_NODE, clock_frequency) / DT_PROP(HALF_BRIDGE_NODE, frequency);

    zassert_equal(half_bridge_get_period(dev), expected);
}

ZTEST(half_bridge, test_set_duty_cycle)
{
    zassert_ok(half_bridge_set_duty_cycle(dev, MID_PWM_DUTY));
    zassert_within(half_bridge_get_duty_cycle(dev), MID_PWM_DUTY, DUTY_EPSILON);
}

ZTEST(half_bridge, test_invalid_duty_cycle_ignored)
{
    zassert_ok(half_bridge_set_duty_cycle(dev, MID_PWM_DUTY));
    zassert_equal(half_bridge_set_duty_cycle(dev, 1.5F), -EINVAL);
    zassert_equal(half_bridge_set_duty_cycle(dev, -0.1F), -EINVAL);
    zassert_within(half_bridge_get_duty_cycle(dev), MID_PWM_DUTY, DUTY_EPSILON);
}

ZTEST(half_bridge, test_start)
{
    zassert_ok(half_bridge_set_duty_cycle(dev, MID_PWM_DUTY));
    zassert_ok(half_bridge_start(dev));
    zassert_true(half_bridge_is_enabled(dev));
}

ZTEST(half_bridge, test_stop)
{
    zassert_ok(half_bridge_set_duty_cycle(dev, MID_PWM_DUTY));
    zassert_ok(half_bridge_start(dev));
    zassert_ok(half_bridge_stop(dev));
    zassert_false(half_bridge_is_enabled(dev));
}

ZTEST(half_bridge, test_duty_limits_not_violated)
{
    /* maximum limit */
    zassert_ok(half_bridge_set_duty_cycle(dev, 1.0F));
    zassert_within(half_bridge_get_duty_cycle(dev), MAX_PWM_DUTY, DUTY_EPSILON);

    /* minimum limit */
    zassert_ok(half_bridge_set_duty_cycle(dev, 0.0F));
    zassert_within(half_bridge_get_duty_cycle(dev), MIN_PWM_DUTY, DUTY_EPSILON);
}

ZTEST(half_bridge, test_compare_limits_not_violated)
{
    /* maximum limit */
    zassert_ok(half_bridge_set_duty_cycle(dev, MAX_PWM_DUTY));
    zassert_ok(half_bridge_set_compare(dev, half_bridge_get_compare(dev) + 1));
    zassert_within(half_bridge_get_duty_cycle(dev), MAX_PWM_DUTY, DUTY_EPSILON);

    /* minimum limit */
    zassert_ok(half_bridge_set_duty_cycle(dev, MIN_PWM_DUTY));
    zassert_ok(half_bridge_set_compare(dev, half_bridge_get_compare(dev) - 1));
    zassert_within(half_bridge_get_duty_cycle(dev), MIN_PWM_DUTY, DUTY_EPSILON);
}

ZTEST(half_bridge, test_new_limits_clamp_compare)
{
    zassert_ok(half_bridge_set_duty_cycle(dev, MAX_PWM_DUTY));
    zassert_ok(half_bridge_set_duty_limits(dev, MIN_PWM_DUTY, MID_PWM_DUTY));
    zassert_within(half_bridge_get_duty_cycle(dev), MID_PWM_DUTY, DUTY_EPSILON);
}

ZTEST(half_bridge, test_invalid_limits_rejected)
{
    uint32_t period = half_bridge_get_period(dev);

    zassert_equal(half_bridge_set_duty_limits(dev, MAX_PWM_DUTY, MIN_PWM_DUTY), -EINVAL);
    zassert_equal(half_bridge_set_duty_limits(dev, MIN_PWM_DUTY, 1.1F), -EINVAL);
    zassert_equal(half_bridge_set_compare_limits(dev, 0, period + 1), -EINVAL);
}

ZTEST_SUITE(half_bridge, NULL, half_bridge_setup, half_bridge_before, NULL, NULL);
