/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tests.h"
#include "half_bridge.h"

#include <time.h>
#include <stdio.h>
#include <math.h>

#define MAX_PWM_DUTY 0.97
#define MIN_PWM_DUTY 0.1
#define MID_PWM_DUTY ((MIN_PWM_DUTY + MAX_PWM_DUTY)/2)
#define PWM_F_KHZ 70
#define PWM_DEADTIME_NS 300

const float duty_epsilon = 0.006;
// calculated duty cycle float may deviate +/- duty_epsilon from test target value

static void init_structs()
{
    half_bridge_init(PWM_F_KHZ, PWM_DEADTIME_NS, MIN_PWM_DUTY, MAX_PWM_DUTY);
    half_bridge_stop();
}

void half_bridge_set_duty_cycle_works()
{
    half_bridge_set_duty_cycle(MID_PWM_DUTY);
    TEST_ASSERT_FLOAT_WITHIN(duty_epsilon, MID_PWM_DUTY, half_bridge_get_duty_cycle());
}

void half_bridge_starts_up()
{
    half_bridge_set_duty_cycle(MID_PWM_DUTY);
    half_bridge_start();
    TEST_ASSERT_EQUAL(true, half_bridge_enabled());
}

void half_bridge_stops()
{
    half_bridge_set_duty_cycle(MID_PWM_DUTY);
    half_bridge_start();
    half_bridge_stop();
    TEST_ASSERT_EQUAL(false, half_bridge_enabled());
}

void half_bridge_duty_limits_not_violated()
{
    // maximum limit
    half_bridge_set_duty_cycle(1.0);
    TEST_ASSERT_FLOAT_WITHIN(duty_epsilon, MAX_PWM_DUTY, half_bridge_get_duty_cycle());

    // minimum limit
    half_bridge_set_duty_cycle(0.0);
    TEST_ASSERT_FLOAT_WITHIN(duty_epsilon, MIN_PWM_DUTY, half_bridge_get_duty_cycle());
}

void half_bridge_ccr_limits_not_violated()
{
    // maximum limit
    half_bridge_set_duty_cycle(MAX_PWM_DUTY);
    half_bridge_set_ccr(half_bridge_get_ccr() + 1);
    TEST_ASSERT_FLOAT_WITHIN(duty_epsilon, MAX_PWM_DUTY, half_bridge_get_duty_cycle());

    // minimum limit
    half_bridge_set_duty_cycle(MIN_PWM_DUTY);
    half_bridge_set_ccr(half_bridge_get_ccr() - 1);
    TEST_ASSERT_FLOAT_WITHIN(duty_epsilon, MIN_PWM_DUTY, half_bridge_get_duty_cycle());
}

void half_bridge_tests()
{
    init_structs();

    UNITY_BEGIN();

    RUN_TEST(half_bridge_set_duty_cycle_works);
    RUN_TEST(half_bridge_starts_up);
    RUN_TEST(half_bridge_stops);

    RUN_TEST(half_bridge_duty_limits_not_violated);
    RUN_TEST(half_bridge_ccr_limits_not_violated);

    UNITY_END();
}
