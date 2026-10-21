/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tests.h"

#include <libresolar/drivers/half_bridge.h>

#include <math.h>
#include <stdio.h>
#include <time.h>

#include "setup.h"

static void init_structs_buck(int num_batteries = 1)
{
    half_bridge_stop(dcdc.half_bridge);

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
    dcdc.pwm_direction = 1;
    dcdc.enable = true;
}

static void start_buck()
{
    dev_stat.error_flags = 0;
    init_structs_buck();
    half_bridge_set_duty_limits(dcdc.half_bridge, 12 / dcdc.hs_voltage_max, 0.97);
    half_bridge_set_duty_cycle(dcdc.half_bridge, 0.97);
    dcdc.control();
    dcdc.control();
    dcdc.control(); // call multiple times because of startup delay
    zassert_equal(dcdc.state, DCDC_CONTROL_MPPT);
}

static void init_structs_boost(int num_batteries = 1)
{
    half_bridge_stop(dcdc.half_bridge);

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
    dcdc.pwm_direction = 1;
    dcdc.enable = true;
}

static void start_boost()
{
    init_structs_boost();
    half_bridge_set_duty_limits(dcdc.half_bridge, 12 / dcdc.hs_voltage_max, 0.97);
    half_bridge_set_duty_cycle(dcdc.half_bridge, 0.97);
    dcdc.control();
    dcdc.control();
    dcdc.control(); // call multiple times because of startup delay
    zassert_equal(dcdc.state, DCDC_CONTROL_MPPT);
}

ZTEST(dcdc, test_start_valid_mppt_buck)
{
    init_structs_buck();
    zassert_equal(dcdc.check_start_conditions(), DCDC_MODE_BUCK);
}

ZTEST(dcdc, test_start_valid_mppt_buck_dual_battery)
{
    init_structs_buck(2);
    zassert_equal(dcdc.check_start_conditions(), DCDC_MODE_BUCK);
}

ZTEST(dcdc, test_start_valid_mppt_boost)
{
    init_structs_boost();
    zassert_equal(dcdc.check_start_conditions(), DCDC_MODE_BOOST);
}

ZTEST(dcdc, test_start_valid_mppt_boost_dual_battery)
{
    init_structs_boost(2);
    zassert_equal(dcdc.check_start_conditions(), DCDC_MODE_BOOST);
}

ZTEST(dcdc, test_no_start_before_restart_delay)
{
    init_structs_buck();
    dcdc.off_timestamp = time(NULL) - dcdc.restart_interval + 1;
    zassert_equal(dcdc.check_start_conditions(), DCDC_MODE_OFF);
    dcdc.off_timestamp = time(NULL) - dcdc.restart_interval;
    zassert_equal(dcdc.check_start_conditions(), DCDC_MODE_BUCK);
}

ZTEST(dcdc, test_no_start_if_dcdc_disabled)
{
    init_structs_buck();
    dcdc.enable = false;
    zassert_equal(dcdc.check_start_conditions(), DCDC_MODE_OFF);
}

ZTEST(dcdc, test_no_start_if_dcdc_lv_voltage_low)
{
    init_structs_buck();
    lv_terminal.bus->voltage = dcdc.ls_voltage_min - 0.5f;
    zassert_equal(dcdc.check_start_conditions(), DCDC_MODE_OFF);
}

// buck

ZTEST(dcdc, test_no_buck_start_if_bat_voltage_high)
{
    init_structs_buck();
    lv_terminal.bus->voltage = bat_conf.topping_voltage + 0.1f;
    zassert_equal(dcdc.check_start_conditions(), DCDC_MODE_OFF);
}

ZTEST(dcdc, test_no_buck_start_if_bat_chg_not_allowed)
{
    init_structs_buck();
    lv_terminal.bus->sink_current_margin = 0;
    zassert_equal(dcdc.check_start_conditions(), DCDC_MODE_OFF);
}

ZTEST(dcdc, test_no_buck_start_if_solar_voltage_high)
{
    init_structs_buck();
    hv_terminal.bus->voltage = dcdc.hs_voltage_max + 1;
    zassert_equal(dcdc.check_start_conditions(), DCDC_MODE_OFF);
}

ZTEST(dcdc, test_no_buck_start_if_solar_voltage_low)
{
    init_structs_buck();
    hv_terminal.bus->voltage = 17;
    zassert_equal(dcdc.check_start_conditions(), DCDC_MODE_OFF);
}

// boost

ZTEST(dcdc, test_no_boost_start_if_bat_voltage_high)
{
    init_structs_boost();
    hv_terminal.bus->voltage = bat_conf.topping_voltage + 0.1f;
    zassert_equal(dcdc.check_start_conditions(), DCDC_MODE_OFF);
}

ZTEST(dcdc, test_no_boost_start_if_bat_chg_not_allowed)
{
    init_structs_boost();
    hv_terminal.bus->sink_current_margin = 0;
    zassert_equal(dcdc.check_start_conditions(), DCDC_MODE_OFF);
}

ZTEST(dcdc, test_no_boost_start_if_solar_voltage_high)
{
    init_structs_boost();
    lv_terminal.bus->voltage = dcdc.ls_voltage_max + 1;
    zassert_equal(dcdc.check_start_conditions(), DCDC_MODE_OFF);
}

ZTEST(dcdc, test_no_boost_start_if_solar_voltage_low)
{
    init_structs_boost();
    lv_terminal.bus->voltage = 17;
    zassert_equal(dcdc.check_start_conditions(), DCDC_MODE_OFF);
}

// buck operation

ZTEST(dcdc, test_buck_increasing_power)
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle(dcdc.half_bridge);
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle(dcdc.half_bridge);
    zassert_true(pwm_after > pwm_before);
}

ZTEST(dcdc, test_buck_derating_output_voltage_too_high)
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle(dcdc.half_bridge);
    lv_terminal.bus->voltage = lv_terminal.bus->sink_voltage_intercept + 0.1f;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle(dcdc.half_bridge);
    zassert_true(pwm_after < pwm_before); // less duty cycle = higher voltage
    zassert_equal(dcdc.state, DCDC_CONTROL_CV_LS);
}

ZTEST(dcdc, test_buck_derating_output_current_too_high)
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle(dcdc.half_bridge);
    lv_terminal.current = lv_terminal.pos_current_limit + 0.1f;
    lv_terminal.update_bus_current_margins();
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle(dcdc.half_bridge);
    zassert_true(pwm_after < pwm_before);
    zassert_equal(dcdc.state, DCDC_CONTROL_CC_LS);
}

ZTEST(dcdc, test_buck_derating_inductor_current_too_high)
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle(dcdc.half_bridge);
    dcdc.inductor_current = dcdc.inductor_current_max + 0.1f;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle(dcdc.half_bridge);
    zassert_true(pwm_after < pwm_before);
    zassert_equal(dcdc.state, DCDC_CONTROL_CC_LS);
}

ZTEST(dcdc, test_buck_derating_input_voltage_too_low)
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle(dcdc.half_bridge);
    hv_terminal.bus->voltage = hv_terminal.bus->src_voltage_intercept - 0.1f;
    dcdc.power = 1.2f;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle(dcdc.half_bridge);
    zassert_true(pwm_after < pwm_before);
    zassert_equal(dcdc.state, DCDC_CONTROL_CV_HS);
}

ZTEST(dcdc, test_buck_derating_input_current_too_high)
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle(dcdc.half_bridge);
    hv_terminal.current = hv_terminal.neg_current_limit - 0.1f;
    hv_terminal.update_bus_current_margins();
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle(dcdc.half_bridge);
    zassert_true(pwm_after < pwm_before);
    zassert_equal(dcdc.state, DCDC_CONTROL_CC_HS);
}

ZTEST(dcdc, test_buck_derating_temperature_limits_exceeded)
{
    start_buck();
    float pwm_before = half_bridge_get_duty_cycle(dcdc.half_bridge);
    dcdc.temp_mosfets = 81;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle(dcdc.half_bridge);
    zassert_true(pwm_after < pwm_before);
}

ZTEST(dcdc, test_buck_stop_input_power_too_low)
{
    start_buck();
    dcdc.power_good_timestamp = time(NULL) - 11;
    dcdc.control();
    zassert_true(half_bridge_is_enabled(dcdc.half_bridge) == false);
}

ZTEST(dcdc, test_buck_stop_high_voltage_emergency)
{
    start_buck();
    lv_terminal.bus->voltage = dcdc.ls_voltage_max + 0.1f;
    dcdc.control();
    zassert_true(half_bridge_is_enabled(dcdc.half_bridge) == false);
}

ZTEST(dcdc, test_buck_correct_mppt_operation)
{
    start_buck();

    dcdc.power = 5;
    dcdc.control();
    float pwm1 = half_bridge_get_duty_cycle(dcdc.half_bridge);
    dcdc.power = 7;
    dcdc.control();
    float pwm2 = half_bridge_get_duty_cycle(dcdc.half_bridge);
    zassert_true(pwm2 > pwm1);

    dcdc.power = 6; // decrease power to make the direction turn around
    dcdc.control();
    float pwm3 = half_bridge_get_duty_cycle(dcdc.half_bridge);
    zassert_true(pwm3 < pwm2);
}

// boost operation

ZTEST(dcdc, test_boost_increasing_power)
{
    // make sure that without changing anything, dcdc_control goes in direction of increasing power
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle(dcdc.half_bridge);
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle(dcdc.half_bridge);
    zassert_true(pwm_after < pwm_before);
}

ZTEST(dcdc, test_boost_derating_output_voltage_too_high)
{
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle(dcdc.half_bridge);
    hv_terminal.bus->voltage = hv_terminal.bus->sink_voltage_intercept + 0.5f;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle(dcdc.half_bridge);
    zassert_true(pwm_after > pwm_before); // higher duty cycle = less power
}

ZTEST(dcdc, test_boost_derating_output_current_too_high)
{
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle(dcdc.half_bridge);
    hv_terminal.current = hv_terminal.pos_current_limit + 0.1f;
    hv_terminal.update_bus_current_margins();
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle(dcdc.half_bridge);
    zassert_true(pwm_after > pwm_before);
}

ZTEST(dcdc, test_boost_derating_input_voltage_too_low)
{
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle(dcdc.half_bridge);
    lv_terminal.bus->voltage = lv_terminal.bus->src_voltage_intercept - 0.1f;
    dcdc.power = -1.2f;
    hv_terminal.update_bus_current_margins();
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle(dcdc.half_bridge);
    zassert_equal(dcdc.state, DCDC_CONTROL_CV_LS);
    zassert_true(pwm_after > pwm_before);
}

ZTEST(dcdc, test_boost_derating_input_current_too_high)
{
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle(dcdc.half_bridge);
    dcdc.inductor_current = lv_terminal.neg_current_limit - 0.1f;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle(dcdc.half_bridge);
    zassert_true(pwm_after > pwm_before);
}

ZTEST(dcdc, test_boost_derating_temperature_limits_exceeded)
{
    start_boost();
    float pwm_before = half_bridge_get_duty_cycle(dcdc.half_bridge);
    dcdc.temp_mosfets = 81;
    dcdc.control();
    float pwm_after = half_bridge_get_duty_cycle(dcdc.half_bridge);
    zassert_true(pwm_after > pwm_before);
}

ZTEST(dcdc, test_boost_stop_input_power_too_low)
{
    start_boost();
    dcdc.power_good_timestamp = time(NULL) - 11;
    dcdc.control();
    zassert_true(half_bridge_is_enabled(dcdc.half_bridge) == false);
}

ZTEST(dcdc, test_boost_stop_high_voltage_emergency)
{
    start_boost();
    hv_terminal.bus->voltage = dcdc.hs_voltage_max + 0.1f;
    dcdc.control();
    zassert_true(half_bridge_is_enabled(dcdc.half_bridge) == false);
}

ZTEST(dcdc, test_boost_correct_mppt_operation)
{
    start_boost();

    dcdc.power = -5;
    dcdc.control();
    float pwm1 = half_bridge_get_duty_cycle(dcdc.half_bridge);
    dcdc.power = -7;
    dcdc.control();
    float pwm2 = half_bridge_get_duty_cycle(dcdc.half_bridge);
    zassert_true(pwm2 < pwm1);

    dcdc.power = -6; // decrease power to make the direction turn around
    dcdc.control();
    float pwm3 = half_bridge_get_duty_cycle(dcdc.half_bridge);
    zassert_true(pwm3 > pwm2);
}

ZTEST(dcdc, test_fuse_destruction_forces_low_side_on)
{
    start_buck();
    dev_stat.set_error(ERR_DCDC_HS_MOSFET_SHORT);

    // fuse destruction is delayed by 21 calls (seconds) to be able to send out data
    for (int i = 0; i < 21; i++) {
        dcdc.fuse_destruction();
    }
    zassert_not_equal(half_bridge_get_compare(dcdc.half_bridge), 0);

    dcdc.fuse_destruction();
    zassert_true(half_bridge_is_enabled(dcdc.half_bridge));
    zassert_equal(half_bridge_get_compare(dcdc.half_bridge), 0);

    // control loop must not override the forced low side
    dcdc.control();
    zassert_true(half_bridge_is_enabled(dcdc.half_bridge));
    zassert_equal(half_bridge_get_compare(dcdc.half_bridge), 0);

    // forced state is restored after the half bridge was stopped
    dcdc.stop();
    dcdc.fuse_destruction();
    zassert_true(half_bridge_is_enabled(dcdc.half_bridge));
    zassert_equal(half_bridge_get_compare(dcdc.half_bridge), 0);

    dcdc.stop();
    dev_stat.clear_error(ERR_DCDC_HS_MOSFET_SHORT);
}

ZTEST_SUITE(dcdc, NULL, NULL, NULL, NULL, NULL);
