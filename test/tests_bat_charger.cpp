
#include "tests.h"

#include "bat_charger.h"
#include "dc_bus.h"

#include <time.h>
#include <stdio.h>

extern BatConf bat_conf;
extern Charger charger;
extern DcBus *bat_terminal;

static void init_structs()
{
    battery_conf_init(&bat_conf, BAT_TYPE_FLOODED, 6, 100);
    charger_init(&charger);
    battery_init_dc_bus(bat_terminal, &bat_conf, 1);
    charger.state = CHG_STATE_IDLE;
}

void no_start_at_high_voltage()
{
    init_structs();
    bat_terminal->voltage = bat_conf.voltage_recharge + 0.1;
    charger_state_machine(bat_terminal, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_IDLE, charger.state);
}

void no_start_after_short_rest()
{
    init_structs();
    charger.time_state_changed = time(NULL) - bat_conf.time_limit_recharge + 1;
    bat_terminal->voltage = bat_conf.voltage_recharge - 0.1;
    charger_state_machine(bat_terminal, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_IDLE, charger.state);
}

void no_start_outside_temperature_limits()
{
    init_structs();
    charger.bat_temperature = bat_conf.charge_temp_max + 1;
    bat_terminal->voltage = bat_conf.voltage_recharge - 0.1;
    charger_state_machine(bat_terminal, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_IDLE, charger.state);

    charger.bat_temperature = bat_conf.charge_temp_min - 1;
    charger_state_machine(bat_terminal, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_IDLE, charger.state);
}

void start_if_everything_just_fine()
{
    init_structs();
    charger.time_state_changed = time(NULL) - bat_conf.time_limit_recharge - 1;
    bat_terminal->voltage = bat_conf.voltage_recharge - 0.1;
    charger_state_machine(bat_terminal, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_BULK, charger.state);
}

void enter_topping_at_voltage_setpoint()
{
    init_structs();
    charger.time_state_changed = time(NULL) - bat_conf.time_limit_recharge - 1;
    bat_terminal->voltage = bat_conf.voltage_recharge - 0.1;
    charger_state_machine(bat_terminal, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_BULK, charger.state);
    bat_terminal->voltage = bat_conf.topping_voltage + 0.1;
    charger_state_machine(bat_terminal, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_TOPPING, charger.state);
}

void stop_topping_after_time_limit()
{
    enter_topping_at_voltage_setpoint();

    charger.time_state_changed = time(NULL) - bat_conf.topping_duration + 1;
    bat_terminal->voltage = bat_conf.topping_voltage + 0.1;
    bat_terminal->current = bat_conf.topping_current_cutoff + 0.1;

    charger_state_machine(bat_terminal, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_TOPPING, charger.state);

    charger.time_state_changed = time(NULL) - bat_conf.topping_duration - 1;
    charger_state_machine(bat_terminal, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_TRICKLE, charger.state);
}

void stop_topping_at_cutoff_current()
{
    enter_topping_at_voltage_setpoint();

    charger.time_state_changed = time(NULL) - 1;
    bat_terminal->voltage = bat_conf.topping_voltage + 0.1;
    bat_terminal->current = bat_conf.topping_current_cutoff - 0.1;
    charger_state_machine(bat_terminal, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_TRICKLE, charger.state);
}

void trickle_to_idle_for_li_ion()
{
    enter_topping_at_voltage_setpoint();
    battery_conf_init(&bat_conf, BAT_TYPE_LFP, 4, 100);

    charger.time_state_changed = time(NULL) - 1;
    bat_terminal->voltage = bat_conf.topping_voltage + 0.1;
    bat_terminal->current = bat_conf.topping_current_cutoff - 0.1;
    charger_state_machine(bat_terminal, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_IDLE, charger.state);
}

void no_trickle_if_low_current_because_of_low_input()
{
    enter_topping_at_voltage_setpoint();

    charger.time_state_changed = time(NULL) - 200;
    charger.time_voltage_limit_reached = time(NULL) - 3;
    bat_terminal->voltage = bat_conf.topping_voltage - 0.1;
    bat_terminal->current = bat_conf.topping_current_cutoff - 0.1;

    charger_state_machine(bat_terminal, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_TOPPING, charger.state);

    charger.time_voltage_limit_reached = time(NULL) - 1;
    charger_state_machine(bat_terminal, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_TRICKLE, charger.state);
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
    bat_terminal->voltage = bat_conf.topping_voltage + 0.1;
    bat_terminal->current = bat_conf.topping_current_cutoff - 0.1;
    charger_state_machine(bat_terminal, &bat_conf, &charger);
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
    bat_terminal->voltage = bat_conf.topping_voltage + 0.1;
    bat_terminal->current = bat_conf.topping_current_cutoff - 0.1;
    charger_state_machine(bat_terminal, &bat_conf, &charger);
    TEST_ASSERT_NOT_EQUAL(CHG_STATE_EQUALIZATION, charger.state);
}

void trickle_to_equalization_if_enabled_and_time_limit_reached()
{
    enter_topping_at_voltage_setpoint();
    bat_conf.equalization_enabled = true;
    charger.time_last_equalization =
        time(NULL) - bat_conf.equalization_trigger_days * 24*60*60;

    charger.time_state_changed = time(NULL) - 1;
    bat_terminal->voltage = bat_conf.topping_voltage + 0.1;
    bat_terminal->current = bat_conf.topping_current_cutoff - 0.1;
    charger_state_machine(bat_terminal, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_EQUALIZATION, charger.state);
}


void trickle_to_equalization_if_enabled_and_deep_dis_limit_reached()
{
    enter_topping_at_voltage_setpoint();
    bat_conf.equalization_enabled = true;
    charger.deep_dis_last_equalization =
        charger.num_deep_discharges - bat_conf.equalization_trigger_deep_cycles;

    charger.time_state_changed = time(NULL) - 1;
    bat_terminal->voltage = bat_conf.topping_voltage + 0.1;
    bat_terminal->current = bat_conf.topping_current_cutoff - 0.1;
    charger_state_machine(bat_terminal, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_EQUALIZATION, charger.state);
}


void stop_equalization_after_time_limit()
{
    trickle_to_equalization_if_enabled_and_time_limit_reached();

    charger.time_state_changed = time(NULL) - bat_conf.equalization_duration + 1;
    bat_terminal->voltage = bat_conf.equalization_voltage + 0.1;

    charger_state_machine(bat_terminal, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_EQUALIZATION, charger.state);

    charger.time_state_changed = time(NULL) - bat_conf.topping_duration - 1;
    charger_state_machine(bat_terminal, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_TRICKLE, charger.state);
}

void restart_bulk_from_trickle_if_voltage_drops()
{
    TEST_ASSERT(0);
}

void bat_charger_tests()
{
    UNITY_BEGIN();

    RUN_TEST(no_start_at_high_voltage);
    RUN_TEST(no_start_after_short_rest);
    RUN_TEST(no_start_outside_temperature_limits);
    RUN_TEST(start_if_everything_just_fine);

    // topping
    RUN_TEST(enter_topping_at_voltage_setpoint);
    RUN_TEST(stop_topping_after_time_limit);
    RUN_TEST(stop_topping_at_cutoff_current);

    // trickle
    RUN_TEST(trickle_to_idle_for_li_ion);
    RUN_TEST(no_trickle_if_low_current_because_of_low_input);

    // equalization
    RUN_TEST(no_equalization_if_disabled);
    RUN_TEST(no_equalization_if_limits_not_reached);
    RUN_TEST(trickle_to_equalization_if_enabled_and_time_limit_reached);
    RUN_TEST(trickle_to_equalization_if_enabled_and_deep_dis_limit_reached);
    RUN_TEST(stop_equalization_after_time_limit);

    //RUN_TEST(restart_bulk_from_trickle_if_voltage_drops);

    // TODO: temperature compensation
    // TODO: current compensation

    UNITY_END();
}