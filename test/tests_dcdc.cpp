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

#include "setup.h"

static void init_structs_buck(int num_batteries = 1)
{
    dev_stat.error_flags = 0;
    hv_terminal.init_solar();
    hv_terminal.bus->voltage = 20 * num_batteries;
    hv_terminal.bus->src_voltage_intercept = 18 * num_batteries;
    hv_terminal.bus->series_multiplier = 1;
    hv_terminal.current = 0;
    hv_terminal.update_bus_current_margins();

    battery_conf_init(&bat_conf, BAT_TYPE_GEL, 6, 100);
    charger.port = &lv_terminal;
    charger.init_terminal(&bat_conf);
    lv_terminal.bus->voltage = 14 * num_batteries;
    lv_terminal.bus->series_multiplier = num_batteries;
    lv_terminal.current = 0;
    lv_terminal.update_bus_current_margins();

    dcdc.mode = DCDC_MODE_BUCK;
    dcdc.temp_mosfets = 25;
    dcdc.off_timestamp = 0;
    dcdc.inductor_current = 0;
    dcdc.power = 0;
    dcdc.power_prev = 0;
    dcdc.pwm_delta = 1;
    dcdc.enable = true;
}

static void start_buck()
{
    dev_stat.error_flags = 0;
    init_structs_buck();
    half_bridge_init(70, 200, 12 / dcdc.hs_voltage_max, 0.97);
    dcdc.control();
    dcdc.control();
    dcdc.control();    // call multiple times because of startup delay
    TEST_ASSERT_EQUAL(DCDC_CONTROL_MPPT, dcdc.state);
}

static void init_structs_boost(int num_batteries = 1)
{
    half_bridge_stop();

    hv_terminal.init_solar();
    lv_terminal.bus->voltage = 20;
    lv_terminal.bus->src_voltage_intercept = 18;
    lv_terminal.bus->series_multiplier = 1;
    dcdc.inductor_current = 0;
    dcdc.power = 0;
    lv_terminal.update_bus_current_margins();

    int num_cells = (num_batteries == 1) ? 10 : 5;
    battery_conf_init(&bat_conf, BAT_TYPE_NMC, num_cells, 9);
    charger.port = &hv_terminal;
    charger.init_terminal(&bat_conf);
    hv_terminal.bus->voltage = 3.7 * num_cells * num_batteries;
    hv_terminal.bus->series_multiplier = num_batteries;
    hv_terminal.current = 0;
    hv_terminal.power = 0;
    hv_terminal.update_bus_current_margins();

    dcdc.mode = DCDC_MODE_BOOST;
    dcdc.temp_mosfets = 25;
    dcdc.off_timestamp = 0;
    dcdc.power_prev = 0;
    dcdc.pwm_delta = 1;
    dcdc.enable = true;
}

static void start_boost()
{
    init_structs_boost();
    half_bridge_init(70, 200, 12 / dcdc.hs_voltage_max, 0.97);
    dcdc.control();
    dcdc.control();
    dcdc.control();        // call multiple times because of startup delay
    TEST_ASSERT_EQUAL(DCDC_CONTROL_MPPT, dcdc.state);
}

void start_valid_mppt_buck()
{
    init_structs_buck();
    TEST_ASSERT_EQUAL(DCDC_MODE_BUCK, dcdc.check_start_conditions());
}

void start_valid_mppt_buck_dual_battery()
{
    init_structs_buck(2);
    TEST_ASSERT_EQUAL(DCDC_MODE_BUCK, dcdc.check_start_conditions());
}

void start_valid_mppt_boost()
{
    init_structs_boost();
    TEST_ASSERT_EQUAL(DCDC_MODE_BOOST, dcdc.check_start_conditions());
}

void start_valid_mppt_boost_dual_battery()
{
    init_structs_boost(2);
    TEST_ASSERT_EQUAL(DCDC_MODE_BOOST, dcdc.check_start_conditions());
}

void no_start_before_restart_delay()
{
    init_structs_buck();
    dcdc.off_timestamp = time(NULL) - dcdc.restart_interval + 1;
    TEST_ASSERT_EQUAL(DCDC_MODE_OFF, dcdc.check_start_conditions());
    dcdc.off_timestamp = time(NULL) - dcdc.restart_interval;
    TEST_ASSERT_EQUAL(DCDC_MODE_BUCK, dcdc.check_start_conditions());
}

void no_start_if_dcdc_disabled()
{
    init_structs_buck();
    dcdc.enable = false;
    TEST_ASSERT_EQUAL(DCDC_MODE_OFF, dcdc.check_start_conditions());
}

void no_start_if_dcdc_lv_voltage_low()
{
    init_structs_buck();
    lv_terminal.bus->voltage = dcdc.ls_voltage_min - 0.5;
    TEST_ASSERT_EQUAL(DCDC_MODE_OFF, dcdc.check_start_conditions());
}

// buck

void no_buck_start_if_bat_voltage_high()
{
    init_structs_buck();
    lv_terminal.bus->voltage = bat_conf.topping_voltage + 0.1;
    TEST_ASSERT_EQUAL(DCDC_MODE_OFF, dcdc.check_start_conditions());
}

void no_buck_start_if_bat_chg_not_allowed()
{
    init_structs_buck();
    lv_terminal.bus->sink_current_margin = 0;
    TEST_ASSERT_EQUAL(DCDC_MODE_OFF, dcdc.check_start_conditions());
}

void no_buck_start_if_solar_voltage_high()
{
    init_structs_buck();
    hv_terminal.bus->voltage = dcdc.hs_voltage_max + 1;
    TEST_ASSERT_EQUAL(DCDC_MODE_OFF, dcdc.check_start_conditions());
}

void no_buck_start_if_solar_voltage_low()
{
    init_structs_buck();
    hv_terminal.bus->voltage = 17;
    TEST_ASSERT_EQUAL(DCDC_MODE_OFF, dcdc.check_start_conditions());
}

// boost

void no_boost_start_if_bat_voltage_high()
{
    init_structs_boost();
    hv_terminal.bus->voltage = bat_conf.topping_voltage + 0.1;
    TEST_ASSERT_EQUAL(DCDC_MODE_OFF, dcdc.check_start_conditions());
}

void no_boost_start_if_bat_chg_not_allowed()
{
    init_structs_boost();
    hv_terminal.bus->sink_current_margin = 0;
    TEST_ASSERT_EQUAL(DCDC_MODE_OFF, dcdc.check_start_conditions());
}

void no_boost_start_if_solar_voltage_high()
{
    init_structs_boost();
    lv_terminal.bus->voltage = dcdc.ls_voltage_max + 1;
    TEST_ASSERT_EQUAL(DCDC_MODE_OFF, dcdc.check_start_conditions());
}

void no_boost_start_if_solar_voltage_low()
{
    init_structs_boost();
    lv_terminal.bus->voltage = 17;
    TEST_ASSERT_EQUAL(DCDC_MODE_OFF, dcdc.check_start_conditions());
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
    lv_terminal.bus->voltage = lv_terminal.bus->sink_voltage_intercept + 0.1;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after < pwm_before);    // less duty cycle = higher voltage
    TEST_ASSERT_EQUAL(DCDC_CONTROL_CV_LS, dcdc.state);
}

void buck_derating_output_current_too_high()
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle();
    lv_terminal.current = lv_terminal.pos_current_limit + 0.1;
    lv_terminal.update_bus_current_margins();
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after < pwm_before);
    TEST_ASSERT_EQUAL(DCDC_CONTROL_CC_LS, dcdc.state);
}

void buck_derating_inductor_current_too_high()
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle();
    dcdc.inductor_current = dcdc.inductor_current_max + 0.1;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after < pwm_before);
    TEST_ASSERT_EQUAL(DCDC_CONTROL_CC_LS, dcdc.state);
}

void buck_derating_input_voltage_too_low()
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle();
    hv_terminal.bus->voltage = hv_terminal.bus->src_voltage_intercept - 0.1;
    dcdc.power = 1.2;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after < pwm_before);
    TEST_ASSERT_EQUAL(DCDC_CONTROL_CV_HS, dcdc.state);
}

void buck_derating_input_current_too_high()
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle();
    hv_terminal.current = hv_terminal.neg_current_limit - 0.1;
    hv_terminal.update_bus_current_margins();
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after < pwm_before);
    TEST_ASSERT_EQUAL(DCDC_CONTROL_CC_HS, dcdc.state);
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
    lv_terminal.bus->voltage = dcdc.ls_voltage_max + 0.1;
    dcdc.control();
    TEST_ASSERT(half_bridge_enabled() == false);
}

void buck_correct_mppt_operation()
{
    start_buck();

    dcdc.power = 5;
    dcdc.control();
    float pwm1 = half_bridge_get_duty_cycle();
    dcdc.power = 7;
    dcdc.control();
    float pwm2 = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm2 > pwm1);

    dcdc.power = 6;     // decrease power to make the direction turn around
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
    hv_terminal.bus->voltage = hv_terminal.bus->sink_voltage_intercept + 0.5;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after > pwm_before);    // higher duty cycle = less power
}

void boost_derating_output_current_too_high()
{
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle();
    hv_terminal.current = hv_terminal.pos_current_limit + 0.1;
    hv_terminal.update_bus_current_margins();
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm_after > pwm_before);
}

void boost_derating_input_voltage_too_low()
{
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle();
    lv_terminal.bus->voltage = lv_terminal.bus->src_voltage_intercept - 0.1;
    dcdc.power = -1.2;
    hv_terminal.update_bus_current_margins();
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle();
    TEST_ASSERT_EQUAL(DCDC_CONTROL_CV_LS, dcdc.state);
    TEST_ASSERT(pwm_after > pwm_before);
}

void boost_derating_input_current_too_high()
{
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle();
    dcdc.inductor_current = lv_terminal.neg_current_limit - 0.1;
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
    hv_terminal.bus->voltage = dcdc.hs_voltage_max + 0.1;
    dcdc.control();
    TEST_ASSERT(half_bridge_enabled() == false);
}

void boost_correct_mppt_operation()
{
    start_boost();

    dcdc.power = -5;
    dcdc.control();
    float pwm1 = half_bridge_get_duty_cycle();
    dcdc.power = -7;
    dcdc.control();
    float pwm2 = half_bridge_get_duty_cycle();
    TEST_ASSERT(pwm2 < pwm1);

    dcdc.power = -6;     // decrease power to make the direction turn around
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
    RUN_TEST(start_valid_mppt_buck_dual_battery);
    RUN_TEST(start_valid_mppt_boost);
    RUN_TEST(start_valid_mppt_boost_dual_battery);

    RUN_TEST(no_start_before_restart_delay);
    RUN_TEST(no_start_if_dcdc_disabled);
    RUN_TEST(no_start_if_dcdc_lv_voltage_low);

    // 2. Check startup for MPPT buck converter scenario

    RUN_TEST(no_buck_start_if_bat_voltage_high);
    RUN_TEST(no_buck_start_if_bat_chg_not_allowed);
    RUN_TEST(no_buck_start_if_solar_voltage_high);
    RUN_TEST(no_buck_start_if_solar_voltage_low);

    // 3. Check startup for MPPT boost converter scenario

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
    RUN_TEST(buck_derating_inductor_current_too_high);
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
