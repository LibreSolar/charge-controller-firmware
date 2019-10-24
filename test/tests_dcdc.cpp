
#include "tests.h"

#include "half_bridge.h"

#include <time.h>
#include <stdio.h>
#include <math.h>

#include "main.h"

static void init_structs_buck()
{
    hv_terminal.init_solar();
    hv_terminal.voltage = 20;
    hv_terminal.src_voltage_start = 18;
    hv_terminal.current = 0;

    battery_conf_init(&bat_conf, BAT_TYPE_GEL, 6, 100);
    battery_init_dc_bus(&dcdc_lv_port, &bat_conf, 1);
    dcdc_lv_port.voltage = 14;
    dcdc_lv_port.current = 0;

    dcdc.mode = MODE_MPPT_BUCK;
    dcdc.temp_mosfets = 25;
    dcdc.off_timestamp = 0;
    dcdc.power_prev = 0;
    dcdc.pwm_delta = 1;
    dcdc.enabled = true;
}

static void start_buck()
{
    init_structs_buck();
    half_bridge_init(70, 200, 12 / dcdc.hs_voltage_max, 0.97);
    dcdc.control();
    dcdc.control();
    dcdc.control();    // call multiple times because of startup delay
    TEST_ASSERT_EQUAL(DCDC_STATE_MPPT, dcdc.state);
}

static void init_structs_boost()
{
    half_bridge_stop();

    hv_terminal.init_solar();
    dcdc_lv_port.voltage = 20;
    dcdc_lv_port.src_voltage_start = 18;
    dcdc_lv_port.current = 0;
    dcdc_lv_port.power = 0;

    battery_conf_init(&bat_conf, BAT_TYPE_NMC, 10, 9);
    battery_init_dc_bus(&hv_terminal, &bat_conf, 1);
    hv_terminal.voltage = 3.7 * 10;
    hv_terminal.current = 0;
    hv_terminal.power = 0;

    dcdc.mode = MODE_MPPT_BOOST;
    dcdc.temp_mosfets = 25;
    dcdc.off_timestamp = 0;
    dcdc.power_prev = 0;
    dcdc.pwm_delta = 1;
    dcdc.enabled = true;
}

static void start_boost()
{
    init_structs_boost();
    half_bridge_init(70, 200, 12 / dcdc.hs_voltage_max, 0.97);
    dcdc.control();
    dcdc.control();
    dcdc.control();        // call multiple times because of startup delay
    TEST_ASSERT_EQUAL(DCDC_STATE_MPPT, dcdc.state);
}

void start_valid_mppt_buck()
{
    init_structs_buck();
    TEST_ASSERT_EQUAL(1, dcdc.check_start_conditions());
}

void start_valid_mppt_boost()
{
    init_structs_boost();
    TEST_ASSERT_EQUAL(-1, dcdc.check_start_conditions());
}

void no_start_before_restart_delay()
{
    init_structs_buck();
    dcdc.off_timestamp = time(NULL) - dcdc.restart_interval + 1;
    TEST_ASSERT_EQUAL(0, dcdc.check_start_conditions());
    dcdc.off_timestamp = time(NULL) - dcdc.restart_interval;
    TEST_ASSERT_EQUAL(1, dcdc.check_start_conditions());
}

void no_start_if_dcdc_disabled()
{
    init_structs_buck();
    dcdc.enabled = false;
    TEST_ASSERT_EQUAL(0, dcdc.check_start_conditions());
}

void no_start_if_dcdc_lv_voltage_low()
{
    init_structs_buck();
    dcdc_lv_port.voltage = dcdc.ls_voltage_min - 0.5;
    TEST_ASSERT_EQUAL(0, dcdc.check_start_conditions());
}

// buck

void no_buck_start_if_bat_voltage_low()
{
    init_structs_buck();
    dcdc_lv_port.voltage = bat_conf.voltage_absolute_min - 0.1;
    TEST_ASSERT_EQUAL(0, dcdc.check_start_conditions());
}

void no_buck_start_if_bat_voltage_high()
{
    init_structs_buck();
    dcdc_lv_port.voltage = bat_conf.topping_voltage + 0.1;
    TEST_ASSERT_EQUAL(0, dcdc.check_start_conditions());
}

void no_buck_start_if_bat_chg_not_allowed()
{
    init_structs_buck();
    dcdc_lv_port.pos_current_limit = 0;
    TEST_ASSERT_EQUAL(0, dcdc.check_start_conditions());
}

void no_buck_start_if_solar_voltage_high()
{
    init_structs_buck();
    hv_terminal.voltage = dcdc.hs_voltage_max + 1;
    TEST_ASSERT_EQUAL(0, dcdc.check_start_conditions());
}

void no_buck_start_if_solar_voltage_low()
{
    init_structs_buck();
    hv_terminal.voltage = 17;
    TEST_ASSERT_EQUAL(0, dcdc.check_start_conditions());
}

// boost

void no_boost_start_if_bat_voltage_low()
{
    init_structs_boost();
    hv_terminal.voltage = bat_conf.voltage_absolute_min - 0.1;
    TEST_ASSERT_EQUAL(0, dcdc.check_start_conditions());
}

void no_boost_start_if_bat_voltage_high()
{
    init_structs_boost();
    hv_terminal.voltage = bat_conf.topping_voltage + 0.1;
    TEST_ASSERT_EQUAL(0, dcdc.check_start_conditions());
}

void no_boost_start_if_bat_chg_not_allowed()
{
    init_structs_boost();
    hv_terminal.pos_current_limit = 0;
    TEST_ASSERT_EQUAL(0, dcdc.check_start_conditions());
}

void no_boost_start_if_solar_voltage_high()
{
    init_structs_boost();
    dcdc_lv_port.voltage = dcdc.ls_voltage_max + 1;
    TEST_ASSERT_EQUAL(0, dcdc.check_start_conditions());
}

void no_boost_start_if_solar_voltage_low()
{
    init_structs_boost();
    dcdc_lv_port.voltage = 17;
    TEST_ASSERT_EQUAL(0, dcdc.check_start_conditions());
}

// buck operation

void buck_increasing_power()
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle();
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after > pwm_before);
}

void buck_derating_output_voltage_too_high()
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle();
    dcdc_lv_port.voltage = dcdc_lv_port.sink_voltage_max + 0.1;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after < pwm_before);    // less duty cycle = higher voltage
    TEST_ASSERT_EQUAL(DCDC_STATE_CV, dcdc.state);
}

void buck_derating_output_current_too_high()
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle();
    dcdc_lv_port.current = dcdc_lv_port.pos_current_limit + 0.1;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after < pwm_before);
    TEST_ASSERT_EQUAL(DCDC_STATE_CC, dcdc.state);
}

void buck_derating_input_voltage_too_low()
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle();
    hv_terminal.voltage = hv_terminal.src_voltage_stop - 0.1;
    dcdc_lv_port.current = 0.2;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after < pwm_before);
    TEST_ASSERT_EQUAL(DCDC_STATE_DERATING, dcdc.state);
}

void buck_derating_input_current_too_high()
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle();
    hv_terminal.current = hv_terminal.neg_current_limit - 0.1;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after < pwm_before);
    TEST_ASSERT_EQUAL(DCDC_STATE_DERATING, dcdc.state);
}

void buck_derating_temperature_limits_exceeded()
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle();
    dcdc.temp_mosfets = 81;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after < pwm_before);
}

void buck_stop_input_power_too_low()
{
    start_buck();
    dcdc.power_good_timestamp = time(NULL) - 11;
    dcdc.control();
    TEST_ASSERT(half_bridge_enabled() == false);
}

void buck_stop_high_voltage_emergency()
{
    start_buck();
    dcdc_lv_port.voltage = dcdc.ls_voltage_max + 0.1;
    dcdc.control();
    TEST_ASSERT(half_bridge_enabled() == false);
}

void buck_correct_mppt_operation()
{
    start_buck();

    dcdc_lv_port.power = 5;
    dcdc.control();
    float pwm1 = half_bridge_get_duty_cycle();
    dcdc_lv_port.power = 7;
    dcdc.control();
    float pwm2 = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm2 > pwm1);

    dcdc_lv_port.power = 6;     // decrease power to make the direction turn around
    dcdc.control();
    float pwm3 = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm3 < pwm2);
}

// boost operation

void boost_increasing_power()
{
    // make sure that without changing anything, dcdc_control goes in direction of increasing power
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle();
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after < pwm_before);
}

void boost_derating_output_voltage_too_high()
{
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle();
    hv_terminal.voltage = hv_terminal.sink_voltage_max + 0.5;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after > pwm_before);    // higher duty cycle = less power
}

void boost_derating_output_current_too_high()
{
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle();
    hv_terminal.current = hv_terminal.pos_current_limit + 0.1;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after > pwm_before);
}

void boost_derating_input_voltage_too_low()
{
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle();
    dcdc_lv_port.voltage = dcdc_lv_port.src_voltage_stop - 0.1;
    hv_terminal.current = 0.2;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after > pwm_before);
}

void boost_derating_input_current_too_high()
{
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle();
    dcdc_lv_port.current = dcdc_lv_port.neg_current_limit - 0.1;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after > pwm_before);
}

void boost_derating_temperature_limits_exceeded()
{
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle();
    dcdc.temp_mosfets = 81;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after > pwm_before);
}

void boost_stop_input_power_too_low()
{
    start_boost();
    dcdc.power_good_timestamp = time(NULL) - 11;
    dcdc.control();
    TEST_ASSERT(half_bridge_enabled() == false);
}

void boost_stop_high_voltage_emergency()
{
    start_boost();
    hv_terminal.voltage = dcdc.hs_voltage_max + 0.1;
    dcdc.control();
    TEST_ASSERT(half_bridge_enabled() == false);
}

void boost_correct_mppt_operation()
{
    start_boost();

    hv_terminal.power = 5;
    dcdc.control();
    float pwm1 = half_bridge_get_duty_cycle();
    hv_terminal.power = 7;
    dcdc.control();
    float pwm2 = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm2 < pwm1);

    hv_terminal.power = 6;     // decrease power to make the direction turn around
    dcdc.control();
    float pwm3 = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm3 > pwm2);
}

void dcdc_tests()
{
    UNITY_BEGIN();

    // 1. Check of general (re)start conditions

    // check if initialization of test is correct and we start at all
    RUN_TEST(start_valid_mppt_buck);
    RUN_TEST(start_valid_mppt_boost);

    RUN_TEST(no_start_before_restart_delay);
    RUN_TEST(no_start_if_dcdc_disabled);
    RUN_TEST(no_start_if_dcdc_lv_voltage_low);

    // 2. Check startup for MPPT buck converter scenario

    RUN_TEST(no_buck_start_if_bat_voltage_low);
    RUN_TEST(no_buck_start_if_bat_voltage_high);
    RUN_TEST(no_buck_start_if_bat_chg_not_allowed);
    RUN_TEST(no_buck_start_if_solar_voltage_high);
    RUN_TEST(no_buck_start_if_solar_voltage_low);

    // 3. Check startup for MPPT boost converter scenario

    RUN_TEST(no_boost_start_if_bat_voltage_low);
    RUN_TEST(no_boost_start_if_bat_voltage_high);
    RUN_TEST(no_boost_start_if_bat_chg_not_allowed);
    RUN_TEST(no_boost_start_if_solar_voltage_high);
    RUN_TEST(no_boost_start_if_solar_voltage_low);

    // 4. Check startup for nanogrid scenario

    // TODO

    // 5. Check DC/DC control after being started

    // buck mode
    RUN_TEST(buck_increasing_power);
    RUN_TEST(buck_derating_output_voltage_too_high);
    RUN_TEST(buck_derating_output_current_too_high);
    RUN_TEST(buck_derating_input_voltage_too_low);
    RUN_TEST(buck_derating_input_current_too_high);
    RUN_TEST(buck_derating_temperature_limits_exceeded);
    RUN_TEST(buck_stop_input_power_too_low);
    RUN_TEST(buck_stop_high_voltage_emergency);
    RUN_TEST(buck_correct_mppt_operation);

    // boost mode
    RUN_TEST(boost_increasing_power);
    RUN_TEST(boost_derating_output_voltage_too_high);
    RUN_TEST(boost_derating_output_current_too_high);
    RUN_TEST(boost_derating_input_voltage_too_low);
    RUN_TEST(boost_derating_input_current_too_high);
    RUN_TEST(boost_derating_temperature_limits_exceeded);
    RUN_TEST(boost_stop_input_power_too_low);
    RUN_TEST(boost_stop_high_voltage_emergency);
    RUN_TEST(boost_correct_mppt_operation);

    UNITY_END();
}