/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Loop-back tests for half-bridge drivers
 *
 * The outputs of the half bridge are measured with input capture of a separate timer, so that the
 * timing at the pins (frequency, duty cycle, dead time) can be verified without the power stage.
 */

#include <libresolar/drivers/half_bridge.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "capture.h"

#define LOOPBACK_NODE    DT_COMPAT_GET_ANY_STATUS_OKAY(test_half_bridge_loopback)
#define HALF_BRIDGE_NODE DT_PHANDLE(LOOPBACK_NODE, half_bridge)

#define PERIOD_NS   (1000000000U / DT_PROP(HALF_BRIDGE_NODE, frequency))
#define DEADTIME_NS DT_PROP(HALF_BRIDGE_NODE, deadtime_ns)

/* tolerance for the period (1%) */
#define PERIOD_TOLERANCE_NS (PERIOD_NS / 100)

/* tolerance for the dead time (resolution and synchronization of the capture inputs) */
#define DEADTIME_TOLERANCE_NS 30

/* the on-time of the high side may be reduced by the dead time depending on the timer type */
#define ON_TIME_TOLERANCE_NS (DEADTIME_NS + DEADTIME_TOLERANCE_NS)

#define MIN_DUTY 0.1F
#define MAX_DUTY 0.9F

/* recording time, long enough for CAPTURE_MAX_EDGES at the lowest tested frequency */
#define RECORD_TIMEOUT_US 2000

/* time to wait until a new setting is applied at the outputs (several periods) */
#define SETTLING_TIME_US 200

static const struct device *const dev = DEVICE_DT_GET(HALF_BRIDGE_NODE);

static struct capture_result result;
static struct capture_timing timing;

static void measure(void)
{
    k_busy_wait(SETTLING_TIME_US);
    capture_record(&result, RECORD_TIMEOUT_US);
    capture_analyze(&result, &timing);

    TC_PRINT("edges HS/LS: %zu/%zu, periods: %d, period: %u ns, HS on: %u ns, LS on: %u ns, "
             "dead time: %u..%u ns (%d), overlap: %u ns%s%s\n",
             result.hs.num_edges, result.ls.num_edges, timing.num_periods, timing.period_avg,
             timing.hs_on_avg, timing.ls_on_avg, timing.deadtime_min, timing.deadtime_max,
             timing.num_deadtimes, timing.overlap, result.overrun ? ", OVERRUN" : "",
             timing.invalid ? ", INVALID" : "");

    zassert_false(result.overrun, "capture overrun, edges were missed");
    zassert_false(timing.invalid, "inconsistent edges captured");
}

static void assert_no_switching(bool hs_level, bool ls_level)
{
    measure();

    zassert_equal(result.hs.num_edges, 0, "high side is switching");
    zassert_equal(result.ls.num_edges, 0, "low side is switching");
    zassert_equal(result.hs.level_at_start, hs_level, "wrong high-side level");
    zassert_equal(result.ls.level_at_start, ls_level, "wrong low-side level");
}

static void assert_pwm_timing(float duty)
{
    measure();

    zassert_true(timing.num_periods >= 5, "not enough periods captured");

    zassert_within(timing.period_avg, PERIOD_NS, PERIOD_TOLERANCE_NS, "wrong period: %u ns",
                   timing.period_avg);

    uint32_t hs_on_expected = duty * timing.period_avg;
    zassert_within(timing.hs_on_avg, hs_on_expected, ON_TIME_TOLERANCE_NS,
                   "wrong high-side on-time: %u ns (expected %u ns)", timing.hs_on_avg,
                   hs_on_expected);

    /* outputs must never be on at the same time (shoot-through) */
    zassert_equal(timing.overlap, 0, "outputs overlap for %u ns", timing.overlap);

    /* dead time is inserted at both switching transitions of each period */
    zassert_true(timing.num_deadtimes >= 2 * timing.num_periods, "dead time missing");
    zassert_true(timing.deadtime_min >= DEADTIME_NS - DEADTIME_TOLERANCE_NS,
                 "dead time too short: %u ns", timing.deadtime_min);
    zassert_true(timing.deadtime_max <= DEADTIME_NS + DEADTIME_TOLERANCE_NS,
                 "dead time too long: %u ns", timing.deadtime_max);
}

static void *loopback_setup(void)
{
    zassert_true(device_is_ready(dev), "half bridge not ready");
    zassert_ok(capture_init(), "capture init failed");

    TC_PRINT("expected period: %u ns, dead time: %u ns\n", PERIOD_NS, DEADTIME_NS);

    return NULL;
}

static void loopback_before(void *fixture)
{
    ARG_UNUSED(fixture);

    zassert_ok(half_bridge_stop(dev));
    zassert_ok(half_bridge_set_duty_limits(dev, MIN_DUTY, MAX_DUTY));
    zassert_ok(half_bridge_set_duty_cycle(dev, 0.5F));
}

static void loopback_after(void *fixture)
{
    ARG_UNUSED(fixture);

    half_bridge_stop(dev);
}

ZTEST(half_bridge_loopback, test_outputs_off_before_start)
{
    assert_no_switching(false, false);
}

ZTEST(half_bridge_loopback, test_pwm_timing_mid_duty)
{
    zassert_ok(half_bridge_start(dev));
    assert_pwm_timing(0.5F);
}

ZTEST(half_bridge_loopback, test_pwm_timing_low_duty)
{
    zassert_ok(half_bridge_set_duty_cycle(dev, 0.2F));
    zassert_ok(half_bridge_start(dev));
    assert_pwm_timing(0.2F);
}

ZTEST(half_bridge_loopback, test_pwm_timing_high_duty)
{
    zassert_ok(half_bridge_set_duty_cycle(dev, 0.8F));
    zassert_ok(half_bridge_start(dev));
    assert_pwm_timing(0.8F);
}

ZTEST(half_bridge_loopback, test_duty_change_while_running)
{
    zassert_ok(half_bridge_start(dev));
    zassert_ok(half_bridge_set_duty_cycle(dev, 0.3F));
    assert_pwm_timing(0.3F);
}

ZTEST(half_bridge_loopback, test_duty_limits_at_outputs)
{
    zassert_ok(half_bridge_start(dev));

    zassert_ok(half_bridge_set_duty_cycle(dev, 0.95F));
    assert_pwm_timing(MAX_DUTY);

    zassert_ok(half_bridge_set_duty_cycle(dev, 0.05F));
    assert_pwm_timing(MIN_DUTY);
}

ZTEST(half_bridge_loopback, test_outputs_off_after_stop)
{
    zassert_ok(half_bridge_start(dev));
    zassert_ok(half_bridge_stop(dev));
    assert_no_switching(false, false);
}

ZTEST(half_bridge_loopback, test_force_low_side)
{
    zassert_ok(half_bridge_start(dev));
    zassert_ok(half_bridge_force_low_side(dev));
    assert_no_switching(false, true);
}

ZTEST(half_bridge_loopback, test_outputs_off_after_forced_low_side)
{
    zassert_ok(half_bridge_force_low_side(dev));
    zassert_ok(half_bridge_stop(dev));
    assert_no_switching(false, false);
}

ZTEST_SUITE(half_bridge_loopback, NULL, loopback_setup, loopback_before, loopback_after, NULL);
