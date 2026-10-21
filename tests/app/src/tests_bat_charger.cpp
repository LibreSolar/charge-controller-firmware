/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tests.h"

#include <stdio.h>
#include <time.h>

#include "setup.h"

static void init_structs()
{
    battery_conf_init(&bat_conf, BAT_TYPE_FLOODED, 6, 100);
    charger.init_terminal(&bat_conf);
    charger.state = CHG_STATE_IDLE;
    charger.bat_temperature = 25;
    bat_terminal.bus->voltage = 14.0;
    bat_terminal.bus->voltage_filtered = 14.0;
    bat_terminal.current = 0;
}

ZTEST(bat_charger, test_no_start_at_high_voltage)
{
    init_structs();
    bat_terminal.bus->voltage = bat_conf.recharge_voltage + 0.1f;
    charger.charge_control(&bat_conf);
    zassert_equal(charger.state, CHG_STATE_IDLE);
}

ZTEST(bat_charger, test_no_start_after_short_rest)
{
    init_structs();
    charger.time_state_changed = time(NULL) - bat_conf.time_limit_recharge + 1;
    bat_terminal.bus->voltage = bat_conf.recharge_voltage - 0.1f;
    charger.charge_control(&bat_conf);
    zassert_equal(charger.state, CHG_STATE_IDLE);
}

ZTEST(bat_charger, test_no_start_outside_temperature_limits)
{
    init_structs();
    charger.bat_temperature = bat_conf.charge_temp_max + 1;
    bat_terminal.bus->voltage = bat_conf.recharge_voltage - 0.1f;
    charger.charge_control(&bat_conf);
    zassert_equal(charger.state, CHG_STATE_IDLE);

    charger.bat_temperature = bat_conf.charge_temp_min - 1;
    charger.charge_control(&bat_conf);
    zassert_equal(charger.state, CHG_STATE_IDLE);
}

ZTEST(bat_charger, test_start_if_everything_just_fine)
{
    init_structs();
    charger.time_state_changed = time(NULL) - bat_conf.time_limit_recharge - 1;
    bat_terminal.bus->voltage = bat_conf.recharge_voltage - 0.1f;
    charger.charge_control(&bat_conf);
    zassert_equal(charger.state, CHG_STATE_BULK);
}

static void enter_topping_at_voltage_setpoint()
{
    init_structs();
    charger.time_state_changed = time(NULL) - bat_conf.time_limit_recharge - 1;
    bat_terminal.bus->voltage = bat_conf.recharge_voltage - 0.1f;
    bat_terminal.bus->voltage_filtered = bat_terminal.bus->voltage;
    charger.charge_control(&bat_conf);
    zassert_equal(charger.state, CHG_STATE_BULK);
    bat_terminal.bus->voltage = bat_conf.topping_voltage + 0.1f;
    bat_terminal.bus->voltage_filtered = bat_terminal.bus->voltage;
    charger.charge_control(&bat_conf);
    zassert_equal(charger.state, CHG_STATE_TOPPING);
}

ZTEST(bat_charger, test_topping_to_bulk_after_8h_low_power)
{
    enter_topping_at_voltage_setpoint();

    charger.time_state_changed = time(NULL) - 8 * 60 * 60 + 1;
    bat_terminal.bus->voltage = bat_conf.topping_voltage - 0.1f;
    bat_terminal.bus->voltage_filtered = bat_terminal.bus->voltage;
    bat_terminal.current = bat_conf.topping_cutoff_current + 0.1f;
    bat_terminal.current_filtered = bat_terminal.current;

    charger.charge_control(&bat_conf);
    zassert_equal(charger.state, CHG_STATE_TOPPING);

    charger.time_state_changed = time(NULL) - 8 * 60 * 60 - 1;
    charger.charge_control(&bat_conf);
    zassert_equal(charger.state, CHG_STATE_BULK);
}

ZTEST(bat_charger, test_stop_topping_after_time_limit)
{
    enter_topping_at_voltage_setpoint();

    charger.target_voltage_timer = bat_conf.topping_duration - 1;
    bat_terminal.current = bat_conf.topping_cutoff_current + 0.1f;
    bat_terminal.current_filtered = bat_terminal.current;
    bat_terminal.bus->voltage =
        bat_conf.topping_voltage - bat_terminal.current * bat_terminal.bus->sink_droop_res + 0.1f;
    bat_terminal.bus->voltage_filtered = bat_terminal.bus->voltage;

    charger.charge_control(&bat_conf);
    zassert_equal(charger.state, CHG_STATE_TOPPING);

    charger.target_voltage_timer = bat_conf.topping_duration + 1;
    charger.charge_control(&bat_conf);
    zassert_equal(charger.state, CHG_STATE_FLOAT);
}

ZTEST(bat_charger, test_stop_topping_at_cutoff_current)
{
    enter_topping_at_voltage_setpoint();

    charger.target_voltage_timer = 0;
    bat_terminal.current = bat_conf.topping_cutoff_current - 0.1f;
    bat_terminal.current_filtered = bat_terminal.current;
    bat_terminal.bus->voltage =
        bat_conf.topping_voltage - bat_terminal.current * bat_terminal.bus->sink_droop_res + 0.1f;
    bat_terminal.bus->voltage_filtered = bat_terminal.bus->voltage;
    charger.charge_control(&bat_conf);
    zassert_equal(charger.state, CHG_STATE_FLOAT);
}

ZTEST(bat_charger, test_float_to_idle_for_li_ion)
{
    enter_topping_at_voltage_setpoint();
    battery_conf_init(&bat_conf, BAT_TYPE_LFP, 4, 100);

    charger.time_state_changed = time(NULL) - 1;
    bat_terminal.current = bat_conf.topping_cutoff_current - 0.1f;
    bat_terminal.bus->voltage =
        bat_conf.topping_voltage - bat_terminal.current * bat_terminal.bus->sink_droop_res + 0.1f;
    charger.charge_control(&bat_conf);
    zassert_equal(charger.state, CHG_STATE_IDLE);
}

ZTEST(bat_charger, test_no_equalization_if_disabled)
{
    enter_topping_at_voltage_setpoint();
    bat_conf.equalization_enabled = false;

    // set triggers such that it would normally start
    charger.deep_dis_last_equalization =
        charger.num_deep_discharges - bat_conf.equalization_trigger_deep_cycles;
    charger.time_last_equalization = time(NULL) - bat_conf.equalization_trigger_days * 24 * 60 * 60;

    charger.time_state_changed = time(NULL) - 1;
    bat_terminal.bus->voltage = bat_conf.topping_voltage + 0.1f;
    bat_terminal.current = bat_conf.topping_cutoff_current - 0.1f;
    charger.charge_control(&bat_conf);
    zassert_not_equal(charger.state, CHG_STATE_EQUALIZATION);
}

ZTEST(bat_charger, test_no_equalization_if_limits_not_reached)
{
    enter_topping_at_voltage_setpoint();
    bat_conf.equalization_enabled = true;

    // set triggers such that they are just NOT reached
    charger.deep_dis_last_equalization =
        charger.num_deep_discharges - bat_conf.equalization_trigger_deep_cycles + 1;
    charger.time_last_equalization =
        time(NULL) - (bat_conf.equalization_trigger_days - 1) * 24 * 60 * 60;

    charger.time_state_changed = time(NULL) - 1;
    bat_terminal.bus->voltage = bat_conf.topping_voltage + 0.1f;
    bat_terminal.current = bat_conf.topping_cutoff_current - 0.1f;
    charger.charge_control(&bat_conf);
    zassert_not_equal(charger.state, CHG_STATE_EQUALIZATION);
}

static void float_to_equalization_if_enabled_and_time_limit_reached()
{
    enter_topping_at_voltage_setpoint();
    bat_conf.equalization_enabled = true;
    charger.time_last_equalization = time(NULL) - bat_conf.equalization_trigger_days * 24 * 60 * 60;

    charger.time_state_changed = time(NULL) - 1;
    bat_terminal.current = bat_conf.topping_cutoff_current - 0.1f;
    bat_terminal.bus->voltage =
        bat_conf.topping_voltage - bat_terminal.current * bat_terminal.bus->sink_droop_res + 0.1f;
    charger.charge_control(&bat_conf);
    zassert_equal(charger.state, CHG_STATE_EQUALIZATION);
}

ZTEST(bat_charger, test_float_to_equalization_if_enabled_and_deep_dis_limit_reached)
{
    enter_topping_at_voltage_setpoint();
    bat_conf.equalization_enabled = true;
    charger.deep_dis_last_equalization =
        charger.num_deep_discharges - bat_conf.equalization_trigger_deep_cycles;

    charger.time_state_changed = time(NULL) - 1;
    bat_terminal.current = bat_conf.topping_cutoff_current - 0.1f;
    bat_terminal.bus->voltage =
        bat_conf.topping_voltage - bat_terminal.current * bat_terminal.bus->sink_droop_res + 0.1f;
    charger.charge_control(&bat_conf);
    zassert_equal(charger.state, CHG_STATE_EQUALIZATION);
}

ZTEST(bat_charger, test_stop_equalization_after_time_limit)
{
    float_to_equalization_if_enabled_and_time_limit_reached();

    charger.time_state_changed = time(NULL) - bat_conf.equalization_duration + 1;
    bat_terminal.bus->voltage = bat_conf.equalization_voltage
                                - bat_terminal.current * bat_terminal.bus->sink_droop_res + 0.1f;

    charger.charge_control(&bat_conf);
    zassert_equal(charger.state, CHG_STATE_EQUALIZATION);

    charger.time_state_changed = time(NULL) - bat_conf.topping_duration - 1;
    charger.charge_control(&bat_conf);
    zassert_equal(charger.state, CHG_STATE_FLOAT);
}

void restart_bulk_from_float_if_voltage_drops()
{
    zassert_true(0);
}

ZTEST(bat_charger, test_stop_discharge_at_low_voltage)
{
    init_structs();

    bat_terminal.bus->voltage = 14;
    charger.discharge_control(&bat_conf);
    zassert_true(bat_terminal.neg_current_limit < 0);

    bat_terminal.bus->voltage = bat_conf.absolute_min_voltage - 0.1f;
    charger.discharge_control(&bat_conf);
    zassert_equal(bat_terminal.neg_current_limit, 0);
}

ZTEST(bat_charger, test_stop_discharge_at_overtemp)
{
    init_structs();
    zassert_true(bat_terminal.neg_current_limit < 0);

    charger.bat_temperature = bat_conf.discharge_temp_max + 1;
    charger.discharge_control(&bat_conf);
    zassert_equal(bat_terminal.neg_current_limit, 0);
}

ZTEST(bat_charger, test_stop_discharge_at_undertemp)
{
    init_structs();
    zassert_true(bat_terminal.neg_current_limit < 0);

    charger.bat_temperature = bat_conf.discharge_temp_min - 1;
    charger.discharge_control(&bat_conf);
    zassert_equal(bat_terminal.neg_current_limit, 0);
}

ZTEST(bat_charger, test_restart_discharge_if_allowed)
{
    init_structs();

    // stop because of undervoltage
    bat_terminal.bus->voltage = bat_conf.absolute_min_voltage - 0.1f;
    charger.discharge_control(&bat_conf);
    zassert_equal(bat_terminal.neg_current_limit, 0);

    // increase voltage slightly above absolute minimum
    bat_terminal.bus->voltage = bat_conf.absolute_min_voltage + 0.05f;
    charger.discharge_control(&bat_conf);
    zassert_equal(bat_terminal.neg_current_limit, 0);

    // increase voltage above hysteresis voltage
    bat_terminal.bus->voltage = bat_conf.absolute_min_voltage + 0.15f;
    charger.discharge_control(&bat_conf);
    zassert_true(bat_terminal.neg_current_limit < 0);
}

void battery_values_propagated_to_lv_bus_int()
{
    zassert_true(0);
}

void no_soc_above_100()
{
    zassert_true(0);
}

void no_soc_below_0()
{
    zassert_true(0);
}

ZTEST(bat_charger, test_enter_topping_at_voltage_setpoint)
{
    enter_topping_at_voltage_setpoint();
}

ZTEST(bat_charger, test_float_to_equalization_if_enabled_and_time_limit_reached)
{
    float_to_equalization_if_enabled_and_time_limit_reached();
}

ZTEST_SUITE(bat_charger, NULL, NULL, NULL, NULL, NULL);
