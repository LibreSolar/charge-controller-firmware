
#include "tests.h"

#include "dcdc.h"
#include "dc_bus.h"
#include "log.h"
#include "half_bridge.h"

#include <time.h>
#include <stdio.h>
#include <math.h>

extern Dcdc dcdc;
extern DcBus hv_terminal;
extern DcBus lv_bus_int;
extern BatConf bat_conf;

static void init_structs_buck()
{
    dc_bus_init_solar(&hv_terminal, 10);
    hv_terminal.voltage = 20;
    hv_terminal.dis_voltage_start = 18;
    hv_terminal.current = 0;

    battery_conf_init(&bat_conf, BAT_TYPE_GEL, 6, 100);
    battery_init_dc_bus(&lv_bus_int, &bat_conf, 1);
    lv_bus_int.voltage = 14;
    lv_bus_int.current = 0;

    dcdc.temp_mosfets = 25;
    dcdc.off_timestamp = 0;
    dcdc.power_prev = 0;
    dcdc.pwm_delta = 1;
    dcdc_init(&dcdc, &hv_terminal, &lv_bus_int, MODE_MPPT_BUCK);
}

static void start_buck()
{
    init_structs_buck();
    half_bridge_init(70, 200, 12 / dcdc.hs_voltage_max, 0.97);
    dcdc_control(&dcdc);
    dcdc_control(&dcdc);
    dcdc_control(&dcdc);    // call multiple times because of startup delay
    TEST_ASSERT_EQUAL(DCDC_STATE_MPPT, dcdc.state);
}

static void init_structs_boost()
{
    half_bridge_stop();

    dc_bus_init_solar(&lv_bus_int, 10);
    lv_bus_int.voltage = 20;
    lv_bus_int.dis_voltage_start = 18;
    lv_bus_int.current = 0;
    lv_bus_int.power = 0;

    battery_conf_init(&bat_conf, BAT_TYPE_NMC, 10, 9);
    battery_init_dc_bus(&hv_terminal, &bat_conf, 1);
    hv_terminal.voltage = 3.7 * 10;
    hv_terminal.current = 0;
    hv_terminal.power = 0;

    dcdc.temp_mosfets = 25;
    dcdc.off_timestamp = 0;
    dcdc.power_prev = 0;
    dcdc.pwm_delta = -1;
    dcdc_init(&dcdc, &hv_terminal, &lv_bus_int, MODE_MPPT_BOOST);
}

static void start_boost()
{
    init_structs_boost();
    half_bridge_init(70, 200, 12 / dcdc.hs_voltage_max, 0.97);
    dcdc_control(&dcdc);
    dcdc_control(&dcdc);
    dcdc_control(&dcdc);        // call multiple times because of startup delay
    TEST_ASSERT_EQUAL(DCDC_STATE_MPPT, dcdc.state);
}

void start_valid_mppt_buck()
{
    init_structs_buck();
    TEST_ASSERT_EQUAL(1, dcdc_check_start_conditions(&dcdc));
}

void start_valid_mppt_boost()
{
    init_structs_boost();
    TEST_ASSERT_EQUAL(-1, dcdc_check_start_conditions(&dcdc));
}

void no_start_before_restart_delay()
{
    init_structs_buck();
    dcdc.off_timestamp = time(NULL) - dcdc.restart_interval + 1;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc));
    dcdc.off_timestamp = time(NULL) - dcdc.restart_interval;
    TEST_ASSERT_EQUAL(1, dcdc_check_start_conditions(&dcdc));
}

void no_start_if_dcdc_disabled()
{
    init_structs_buck();
    dcdc.enabled = false;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc));
}

void no_start_if_dcdc_lv_voltage_low()
{
    init_structs_buck();
    lv_bus_int.voltage = dcdc.ls_voltage_min - 0.5;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc));
}

// buck

void no_buck_start_if_bat_voltage_low()
{
    init_structs_buck();
    lv_bus_int.voltage = bat_conf.voltage_absolute_min - 0.1;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc));
}

void no_buck_start_if_bat_voltage_high()
{
    init_structs_buck();
    lv_bus_int.voltage = bat_conf.voltage_topping + 0.1;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc));
}

void no_buck_start_if_bat_chg_not_allowed()
{
    init_structs_buck();
    lv_bus_int.chg_current_max = 0;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc));
}

void no_buck_start_if_solar_voltage_high()
{
    init_structs_buck();
    hv_terminal.voltage = dcdc.hs_voltage_max + 1;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc));
}

void no_buck_start_if_solar_voltage_low()
{
    init_structs_buck();
    hv_terminal.voltage = 17;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc));
}

// boost

void no_boost_start_if_bat_voltage_low()
{
    init_structs_boost();
    hv_terminal.voltage = bat_conf.voltage_absolute_min - 0.1;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc));
}

void no_boost_start_if_bat_voltage_high()
{
    init_structs_boost();
    hv_terminal.voltage = bat_conf.voltage_topping + 0.1;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc));
}

void no_boost_start_if_bat_chg_not_allowed()
{
    init_structs_boost();
    hv_terminal.chg_current_max = 0;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc));
}

void no_boost_start_if_solar_voltage_high()
{
    init_structs_boost();
    lv_bus_int.voltage = dcdc.ls_voltage_max + 1;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc));
}

void no_boost_start_if_solar_voltage_low()
{
    init_structs_boost();
    lv_bus_int.voltage = 17;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc));
}

// buck operation

void buck_increasing_power()
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle();
    dcdc_control(&dcdc);
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after > pwm_before);
}

void buck_derating_output_voltage_too_high()
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle();
    lv_bus_int.voltage = lv_bus_int.chg_voltage_target + 0.1;
    dcdc_control(&dcdc);
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after < pwm_before);    // less duty cycle = higher voltage
    TEST_ASSERT_EQUAL(DCDC_STATE_CV, dcdc.state);
}

void buck_derating_output_current_too_high()
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle();
    lv_bus_int.current = lv_bus_int.chg_current_max + 0.1;
    dcdc_control(&dcdc);
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after < pwm_before);
    TEST_ASSERT_EQUAL(DCDC_STATE_CC, dcdc.state);
}

void buck_derating_input_voltage_too_low()
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle();
    hv_terminal.voltage = hv_terminal.dis_voltage_stop - 0.1;
    lv_bus_int.current = 0.2;
    dcdc_control(&dcdc);
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after < pwm_before);
    TEST_ASSERT_EQUAL(DCDC_STATE_CV, dcdc.state);
}

void buck_derating_input_current_too_high()
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle();
    hv_terminal.current = hv_terminal.dis_current_max - 0.1;
    dcdc_control(&dcdc);
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after < pwm_before);
    TEST_ASSERT_EQUAL(DCDC_STATE_CC, dcdc.state);
}

void buck_derating_temperature_limits_exceeded()
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle();
    dcdc.temp_mosfets = 81;
    dcdc_control(&dcdc);
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after < pwm_before);
}

void buck_stop_input_power_too_low()
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle();
    dcdc.power_good_timestamp = time(NULL) - 11;
    dcdc_control(&dcdc);
    TEST_ASSERT(half_bridge_enabled() == false);
}

void buck_stop_high_voltage_emergency()
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle();
    lv_bus_int.voltage = dcdc.ls_voltage_max + 0.1;
    dcdc_control(&dcdc);
    TEST_ASSERT(half_bridge_enabled() == false);
}

void buck_correct_mppt_operation()
{
    start_buck();

    lv_bus_int.power = 5;
    dcdc_control(&dcdc);
    float pwm1 = half_bridge_get_duty_cycle();
    lv_bus_int.power = 7;
    dcdc_control(&dcdc);
    float pwm2 = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm2 > pwm1);

    lv_bus_int.power = 6;     // decrease power to make the direction turn around
    dcdc_control(&dcdc);
    float pwm3 = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm3 < pwm2);
}

// boost operation

void boost_increasing_power()
{
    // make sure that without changing anything, dcdc_control goes in direction of increasing power
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle();
    dcdc_control(&dcdc);
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after < pwm_before);
}

void boost_derating_output_voltage_too_high()
{
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle();
    hv_terminal.voltage = hv_terminal.chg_voltage_target + 0.5;
    dcdc_control(&dcdc);
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after > pwm_before);    // higher duty cycle = less power
}

void boost_derating_output_current_too_high()
{
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle();
    hv_terminal.current = hv_terminal.chg_current_max + 0.1;
    dcdc_control(&dcdc);
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after > pwm_before);
}

void boost_derating_input_voltage_too_low()
{
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle();
    lv_bus_int.voltage = lv_bus_int.dis_voltage_stop - 0.1;
    hv_terminal.current = 0.2;
    dcdc_control(&dcdc);
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after > pwm_before);
}

void boost_derating_input_current_too_high()
{
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle();
    lv_bus_int.current = lv_bus_int.dis_current_max - 0.1;
    dcdc_control(&dcdc);
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after > pwm_before);
}

void boost_derating_temperature_limits_exceeded()
{
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle();
    dcdc.temp_mosfets = 81;
    dcdc_control(&dcdc);
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after > pwm_before);
}

void boost_stop_input_power_too_low()
{
    start_boost();
    dcdc.power_good_timestamp = time(NULL) - 11;
    dcdc_control(&dcdc);
    TEST_ASSERT(half_bridge_enabled() == false);
}

void boost_stop_high_voltage_emergency()
{
    start_boost();
    hv_terminal.voltage = dcdc.hs_voltage_max + 0.1;
    dcdc_control(&dcdc);
    TEST_ASSERT(half_bridge_enabled() == false);
}

void boost_correct_mppt_operation()
{
    start_boost();

    hv_terminal.power = 5;
    dcdc_control(&dcdc);
    float pwm1 = half_bridge_get_duty_cycle();
    hv_terminal.power = 7;
    dcdc_control(&dcdc);
    float pwm2 = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm2 < pwm1);

    hv_terminal.power = 6;     // decrease power to make the direction turn around
    dcdc_control(&dcdc);
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