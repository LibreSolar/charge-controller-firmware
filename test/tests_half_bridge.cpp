
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
    half_bridge_init(PWM_F_KHZ,PWM_DEADTIME_NS,MIN_PWM_DUTY,MAX_PWM_DUTY);
    half_bridge_stop();
}


void half_brigde_set_duty_cycle_works()
{
    half_bridge_set_duty_cycle(MID_PWM_DUTY);
    TEST_ASSERT_FLOAT_WITHIN(duty_epsilon,MID_PWM_DUTY, half_bridge_get_duty_cycle());
}

void half_brigde_not_steps_over_max()
{
    half_bridge_set_duty_cycle(MAX_PWM_DUTY);
    half_bridge_duty_cycle_step(1);
    TEST_ASSERT_FLOAT_WITHIN(duty_epsilon,MAX_PWM_DUTY, half_bridge_get_duty_cycle());
}

void half_brigde_not_steps_under_min()
{
    half_bridge_set_duty_cycle(MIN_PWM_DUTY);
    half_bridge_duty_cycle_step(-1);
    TEST_ASSERT_FLOAT_WITHIN(duty_epsilon,MIN_PWM_DUTY, half_bridge_get_duty_cycle());
}

void half_brigde_not_exceeds_max()
{
    half_bridge_set_duty_cycle(1.0);
    TEST_ASSERT_FLOAT_WITHIN(duty_epsilon,MAX_PWM_DUTY,half_bridge_get_duty_cycle());
}


void half_brigde_not_exceeds_min()
{
    half_bridge_set_duty_cycle(-1.0);
    TEST_ASSERT_FLOAT_WITHIN(duty_epsilon,MIN_PWM_DUTY,half_bridge_get_duty_cycle());
}


void half_brigde_tests()
{
    init_structs();

    UNITY_BEGIN();

    RUN_TEST(half_brigde_set_duty_cycle_works);
    RUN_TEST(half_brigde_not_exceeds_max);
    RUN_TEST(half_brigde_not_exceeds_min);
    RUN_TEST(half_brigde_not_steps_over_max);
    RUN_TEST(half_brigde_not_steps_under_min);

    UNITY_END();
}