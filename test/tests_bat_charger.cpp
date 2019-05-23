
#include "tests.h"

#include "bat_charger.h"
#include "dc_bus.h"

#include <time.h>
#include <stdio.h>

extern battery_conf_t bat_conf;
extern charger_t charger;
extern dc_bus_t ls_bus;

static void init_structs()
{
    battery_conf_init(&bat_conf, BAT_TYPE_FLOODED, 6, 100);
    charger_init(&charger);
    battery_init_dc_bus(&ls_bus, &bat_conf, 1);
    charger.state = CHG_STATE_IDLE;
}

void no_start_at_high_voltage()
{
    init_structs();
    ls_bus.voltage = bat_conf.voltage_recharge + 0.1;
    charger_state_machine(&ls_bus, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_IDLE, charger.state);
}

void no_start_after_short_rest()
{
    init_structs();
    charger.time_state_changed = time(NULL) - bat_conf.time_limit_recharge + 1;
    ls_bus.voltage = bat_conf.voltage_recharge - 0.1;
    charger_state_machine(&ls_bus, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_IDLE, charger.state);
}

void no_start_outside_temperature_limits()
{
    init_structs();
    charger.bat_temperature = bat_conf.charge_temp_max + 1;
    ls_bus.voltage = bat_conf.voltage_recharge - 0.1;
    charger_state_machine(&ls_bus, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_IDLE, charger.state);

    charger.bat_temperature = bat_conf.charge_temp_min - 1;
    charger_state_machine(&ls_bus, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_IDLE, charger.state);
}

void start_if_everything_just_fine()
{
    init_structs();
    charger.time_state_changed = time(NULL) - bat_conf.time_limit_recharge - 1;
    ls_bus.voltage = bat_conf.voltage_recharge - 0.1;
    charger_state_machine(&ls_bus, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_BULK, charger.state);
}

void enter_topping_at_voltage_setpoint()
{
    init_structs();
    charger.time_state_changed = time(NULL) - bat_conf.time_limit_recharge - 1;
    ls_bus.voltage = bat_conf.voltage_recharge - 0.1;
    charger_state_machine(&ls_bus, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_BULK, charger.state);
    ls_bus.voltage = bat_conf.voltage_topping + 0.1;
    charger_state_machine(&ls_bus, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_TOPPING, charger.state);
}

void stop_topping_after_time_limit()
{
    enter_topping_at_voltage_setpoint();

    charger.time_state_changed = time(NULL) - bat_conf.time_limit_topping + 1;
    ls_bus.voltage = bat_conf.voltage_topping + 0.1;
    ls_bus.current = bat_conf.current_cutoff_topping + 0.1;

    charger_state_machine(&ls_bus, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_TOPPING, charger.state);

    charger.time_state_changed = time(NULL) - bat_conf.time_limit_topping - 1;
    charger_state_machine(&ls_bus, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_TRICKLE, charger.state);
}

void stop_topping_at_cutoff_current()
{
    enter_topping_at_voltage_setpoint();

    charger.time_state_changed = time(NULL) - 1;
    ls_bus.voltage = bat_conf.voltage_topping + 0.1;
    ls_bus.current = bat_conf.current_cutoff_topping - 0.1;
    charger_state_machine(&ls_bus, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_TRICKLE, charger.state);
}

void trickle_to_idle_for_li_ion()
{
    enter_topping_at_voltage_setpoint();
    battery_conf_init(&bat_conf, BAT_TYPE_LFP, 4, 100);

    charger.time_state_changed = time(NULL) - 1;
    ls_bus.voltage = bat_conf.voltage_topping + 0.1;
    ls_bus.current = bat_conf.current_cutoff_topping - 0.1;
    charger_state_machine(&ls_bus, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_IDLE, charger.state);
}

void trickle_to_equalization_if_enabled()
{
    enter_topping_at_voltage_setpoint();
    bat_conf.equalization_enabled = true;

    charger.time_state_changed = time(NULL) - 1;
    ls_bus.voltage = bat_conf.voltage_topping + 0.1;
    ls_bus.current = bat_conf.current_cutoff_topping - 0.1;
    charger_state_machine(&ls_bus, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_EQUALIZATION, charger.state);
}

void no_trickle_if_low_current_because_of_low_input()
{
    enter_topping_at_voltage_setpoint();

    charger.time_state_changed = time(NULL) - 200;
    charger.time_voltage_limit_reached = time(NULL) - 3;
    ls_bus.voltage = bat_conf.voltage_topping - 0.1;
    ls_bus.current = bat_conf.current_cutoff_topping - 0.1;

    charger_state_machine(&ls_bus, &bat_conf, &charger);
    TEST_ASSERT_EQUAL(CHG_STATE_TOPPING, charger.state);

    charger.time_voltage_limit_reached = time(NULL) - 1;
    charger_state_machine(&ls_bus, &bat_conf, &charger);
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
    RUN_TEST(enter_topping_at_voltage_setpoint);
    RUN_TEST(stop_topping_after_time_limit);
    RUN_TEST(stop_topping_at_cutoff_current);
    RUN_TEST(trickle_to_idle_for_li_ion);
    //RUN_TEST(trickle_to_equalization_if_enabled);
    RUN_TEST(no_trickle_if_low_current_because_of_low_input);
    //RUN_TEST(restart_bulk_from_trickle_if_voltage_drops);

    // TODO: temperature compensation
    // TODO: current compensation

    UNITY_END();
}