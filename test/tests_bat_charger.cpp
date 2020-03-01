/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tests.h"

#include <time.h>
#include <stdio.h>

#include "setup.h"

static void init_structs()
{
    battery_conf_init(&bat_conf, BAT_TYPE_FLOODED, 6, 100);
    charger.init_terminal(&bat_conf);
    charger.state = CHG_STATE_IDLE;
    charger.bat_temperature = 25;
    bat_terminal.bus->voltage = 14.0;
    bat_terminal.current = 0;
}

void no_start_at_high_voltage()
{
    init_structs();
    bat_terminal.bus->voltage = bat_conf.voltage_recharge + 0.1;
    charger.charge_control(&bat_conf);
    TEST_ASSERT_EQUAL(CHG_STATE_IDLE, charger.state);
}

void no_start_after_short_rest()
{
    init_structs();
    charger.time_state_changed = time(NULL) - bat_conf.time_limit_recharge + 1;
    bat_terminal.bus->voltage = bat_conf.voltage_recharge - 0.1;
    charger.charge_control(&bat_conf);
    TEST_ASSERT_EQUAL(CHG_STATE_IDLE, charger.state);
}

void no_start_outside_temperature_limits()
{
    init_structs();
    charger.bat_temperature = bat_conf.charge_temp_max + 1;
    bat_terminal.bus->voltage = bat_conf.voltage_recharge - 0.1;
    charger.charge_control(&bat_conf);
    TEST_ASSERT_EQUAL(CHG_STATE_IDLE, charger.state);

    charger.bat_temperature = bat_conf.charge_temp_min - 1;
    charger.charge_control(&bat_conf);
    TEST_ASSERT_EQUAL(CHG_STATE_IDLE, charger.state);
}

void start_if_everything_just_fine()
{
    init_structs();
    charger.time_state_changed = time(NULL) - bat_conf.time_limit_recharge - 1;
    bat_terminal.bus->voltage = bat_conf.voltage_recharge - 0.1;
    charger.charge_control(&bat_conf);
    TEST_ASSERT_EQUAL(CHG_STATE_BULK, charger.state);
}

void enter_topping_at_voltage_setpoint()
{
    init_structs();
    charger.time_state_changed = time(NULL) - bat_conf.time_limit_recharge - 1;
    bat_terminal.bus->voltage = bat_conf.voltage_recharge - 0.1;
    charger.charge_control(&bat_conf);
    TEST_ASSERT_EQUAL(CHG_STATE_BULK, charger.state);
    bat_terminal.bus->voltage = bat_conf.topping_voltage + 0.1;
    charger.charge_control(&bat_conf);
    TEST_ASSERT_EQUAL(CHG_STATE_TOPPING, charger.state);
}

void topping_to_bulk_after_8h_low_power()
{
    enter_topping_at_voltage_setpoint();

    charger.time_state_changed = time(NULL) - 8 * 60 * 60 + 1;
    bat_terminal.bus->voltage = bat_conf.topping_voltage - 0.1;
    bat_terminal.current = bat_conf.topping_current_cutoff + 0.1;

    charger.charge_control(&bat_conf);
    TEST_ASSERT_EQUAL(CHG_STATE_TOPPING, charger.state);

    charger.time_state_changed = time(NULL) - 8 * 60 * 60 - 1;
    charger.charge_control(&bat_conf);
    TEST_ASSERT_EQUAL(CHG_STATE_BULK, charger.state);
}

void stop_topping_after_time_limit()
{
    enter_topping_at_voltage_setpoint();

    charger.target_voltage_timer = bat_conf.topping_duration - 1;
    bat_terminal.current = bat_conf.topping_current_cutoff + 0.1;
    bat_terminal.bus->voltage = bat_conf.topping_voltage -
        bat_terminal.current * bat_terminal.bus->sink_droop_res + 0.1;

    charger.charge_control(&bat_conf);
    TEST_ASSERT_EQUAL(CHG_STATE_TOPPING, charger.state);

    charger.target_voltage_timer = bat_conf.topping_duration + 1;
    charger.charge_control(&bat_conf);
    TEST_ASSERT_EQUAL(CHG_STATE_TRICKLE, charger.state);
}

void stop_topping_at_cutoff_current()
{
    enter_topping_at_voltage_setpoint();

    charger.target_voltage_timer = 0;
    bat_terminal.current = bat_conf.topping_current_cutoff - 0.1;
    bat_terminal.bus->voltage = bat_conf.topping_voltage -
        bat_terminal.current * bat_terminal.bus->sink_droop_res + 0.1;
    charger.charge_control(&bat_conf);
    TEST_ASSERT_EQUAL(CHG_STATE_TRICKLE, charger.state);
}

void trickle_to_idle_for_li_ion()
{
    enter_topping_at_voltage_setpoint();
    battery_conf_init(&bat_conf, BAT_TYPE_LFP, 4, 100);

    charger.time_state_changed = time(NULL) - 1;
    bat_terminal.current = bat_conf.topping_current_cutoff - 0.1;
    bat_terminal.bus->voltage = bat_conf.topping_voltage -
        bat_terminal.current * bat_terminal.bus->sink_droop_res + 0.1;
    charger.charge_control(&bat_conf);
    TEST_ASSERT_EQUAL(CHG_STATE_IDLE, charger.state);
}

void no_equalization_if_disabled()
{
    enter_topping_at_voltage_setpoint();
    bat_conf.equalization_enabled = false;

    // set triggers such that it would normally start
    charger.deep_dis_last_equalization =
        charger.num_deep_discharges - bat_conf.equalization_trigger_deep_cycles;
    charger.time_last_equalization =
        time(NULL) - bat_conf.equalization_trigger_days * 24*60*60;

    charger.time_state_changed = time(NULL) - 1;
    bat_terminal.bus->voltage = bat_conf.topping_voltage + 0.1;
    bat_terminal.current = bat_conf.topping_current_cutoff - 0.1;
    charger.charge_control(&bat_conf);
    TEST_ASSERT_NOT_EQUAL(CHG_STATE_EQUALIZATION, charger.state);
}

void no_equalization_if_limits_not_reached()
{
    enter_topping_at_voltage_setpoint();
    bat_conf.equalization_enabled = true;

    // set triggers such that they are just NOT reached
    charger.deep_dis_last_equalization =
        charger.num_deep_discharges - bat_conf.equalization_trigger_deep_cycles + 1;
    charger.time_last_equalization =
        time(NULL) - (bat_conf.equalization_trigger_days - 1) * 24*60*60;

    charger.time_state_changed = time(NULL) - 1;
    bat_terminal.bus->voltage = bat_conf.topping_voltage + 0.1;
    bat_terminal.current = bat_conf.topping_current_cutoff - 0.1;
    charger.charge_control(&bat_conf);
    TEST_ASSERT_NOT_EQUAL(CHG_STATE_EQUALIZATION, charger.state);
}

void trickle_to_equalization_if_enabled_and_time_limit_reached()
{
    enter_topping_at_voltage_setpoint();
    bat_conf.equalization_enabled = true;
    charger.time_last_equalization =
        time(NULL) - bat_conf.equalization_trigger_days * 24*60*60;

    charger.time_state_changed = time(NULL) - 1;
    bat_terminal.current = bat_conf.topping_current_cutoff - 0.1;
    bat_terminal.bus->voltage = bat_conf.topping_voltage -
        bat_terminal.current * bat_terminal.bus->sink_droop_res + 0.1;
    charger.charge_control(&bat_conf);
    TEST_ASSERT_EQUAL(CHG_STATE_EQUALIZATION, charger.state);
}


void trickle_to_equalization_if_enabled_and_deep_dis_limit_reached()
{
    enter_topping_at_voltage_setpoint();
    bat_conf.equalization_enabled = true;
    charger.deep_dis_last_equalization =
        charger.num_deep_discharges - bat_conf.equalization_trigger_deep_cycles;

    charger.time_state_changed = time(NULL) - 1;
    bat_terminal.current = bat_conf.topping_current_cutoff - 0.1;
    bat_terminal.bus->voltage = bat_conf.topping_voltage -
        bat_terminal.current * bat_terminal.bus->sink_droop_res + 0.1;
    charger.charge_control(&bat_conf);
    TEST_ASSERT_EQUAL(CHG_STATE_EQUALIZATION, charger.state);
}


void stop_equalization_after_time_limit()
{
    trickle_to_equalization_if_enabled_and_time_limit_reached();

    charger.time_state_changed = time(NULL) - bat_conf.equalization_duration + 1;
    bat_terminal.bus->voltage = bat_conf.equalization_voltage -
        bat_terminal.current * bat_terminal.bus->sink_droop_res + 0.1;

    charger.charge_control(&bat_conf);
    TEST_ASSERT_EQUAL(CHG_STATE_EQUALIZATION, charger.state);

    charger.time_state_changed = time(NULL) - bat_conf.topping_duration - 1;
    charger.charge_control(&bat_conf);
    TEST_ASSERT_EQUAL(CHG_STATE_TRICKLE, charger.state);
}

void restart_bulk_from_trickle_if_voltage_drops()
{
    TEST_ASSERT(0);
}

void stop_discharge_at_low_voltage()
{
    init_structs();

    bat_terminal.bus->voltage = 14;
    charger.discharge_control(&bat_conf);
    TEST_ASSERT_LESS_THAN(0, bat_terminal.neg_current_limit);

    bat_terminal.bus->voltage = bat_conf.voltage_absolute_min - 0.1;
    charger.discharge_control(&bat_conf);
    TEST_ASSERT_EQUAL(0, bat_terminal.neg_current_limit);
}

void stop_discharge_at_overtemp()
{
    init_structs();
    TEST_ASSERT_LESS_THAN(0, bat_terminal.neg_current_limit);

    charger.bat_temperature = bat_conf.discharge_temp_max + 1;
    charger.discharge_control(&bat_conf);
    TEST_ASSERT_EQUAL(0, bat_terminal.neg_current_limit);
}

void stop_discharge_at_undertemp()
{
    init_structs();
    TEST_ASSERT_LESS_THAN(0, bat_terminal.neg_current_limit);

    charger.bat_temperature = bat_conf.discharge_temp_min - 1;
    charger.discharge_control(&bat_conf);
    TEST_ASSERT_EQUAL(0, bat_terminal.neg_current_limit);
}

void restart_discharge_if_allowed()
{
    init_structs();

    // stop because of undervoltage
    bat_terminal.bus->voltage = bat_conf.voltage_absolute_min - 0.1;
    charger.discharge_control(&bat_conf);
    TEST_ASSERT_EQUAL(0, bat_terminal.neg_current_limit);

    // increase voltage slightly above absolute minimum
    bat_terminal.bus->voltage = bat_conf.voltage_absolute_min + 0.05;
    charger.discharge_control(&bat_conf);
    TEST_ASSERT_EQUAL(0, bat_terminal.neg_current_limit);

    // increase voltage above hysteresis voltage
    bat_terminal.bus->voltage = bat_conf.voltage_absolute_min + 0.15;
    charger.discharge_control(&bat_conf);
    TEST_ASSERT_LESS_THAN(0, bat_terminal.neg_current_limit);
}

void battery_values_propagated_to_lv_bus_int()
{
    TEST_ASSERT(0);
}

void no_soc_above_100()
{
    TEST_ASSERT(0);
}

void no_soc_below_0()
{
    TEST_ASSERT(0);
}

void bat_charger_tests()
{
    UNITY_BEGIN();

    // startup of charging
    RUN_TEST(no_start_at_high_voltage);
    RUN_TEST(no_start_after_short_rest);
    RUN_TEST(no_start_outside_temperature_limits);
    RUN_TEST(start_if_everything_just_fine);

    // topping
    RUN_TEST(enter_topping_at_voltage_setpoint);
    RUN_TEST(topping_to_bulk_after_8h_low_power);
    RUN_TEST(stop_topping_after_time_limit);
    RUN_TEST(stop_topping_at_cutoff_current);

    // trickle
    RUN_TEST(trickle_to_idle_for_li_ion);

    // equalization
    RUN_TEST(no_equalization_if_disabled);
    RUN_TEST(no_equalization_if_limits_not_reached);
    RUN_TEST(trickle_to_equalization_if_enabled_and_time_limit_reached);
    RUN_TEST(trickle_to_equalization_if_enabled_and_deep_dis_limit_reached);
    RUN_TEST(stop_equalization_after_time_limit);

    //RUN_TEST(restart_bulk_from_trickle_if_voltage_drops);

    // TODO: temperature compensation
    // TODO: current compensation

    RUN_TEST(stop_discharge_at_low_voltage);
    RUN_TEST(stop_discharge_at_overtemp);
    RUN_TEST(stop_discharge_at_undertemp);
    RUN_TEST(restart_discharge_if_allowed);

    //RUN_TEST(battery_values_propagated_to_lv_bus_int);

    // ToDo: SOC calculation
    //RUN_TEST(no_soc_above_100);
    //RUN_TEST(no_soc_below_0);

    UNITY_END();
}
