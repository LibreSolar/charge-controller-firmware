
#include "tests.h"

#include <time.h>
#include <stdio.h>
#include <math.h>

#include "main.h"

void reset_counters_at_start_of_day()
{
    solar_terminal.voltage = bat_terminal.voltage - 1;

    log_data.day_counter = 0;

    solar_terminal.neg_energy_Wh = 10.0;
    bat_terminal.neg_energy_Wh = 3.0;
    bat_terminal.pos_energy_Wh = 4.0;
    load_terminal.pos_energy_Wh = 9.0;

    // 5 houurs without sun
    for (int i = 0; i <= 5 * 60 * 60; i++) {
        log_update_energy(&log_data);
    }

    TEST_ASSERT_EQUAL(10, log_data.solar_in_total_Wh);
    TEST_ASSERT_EQUAL(3, log_data.bat_dis_total_Wh);
    TEST_ASSERT_EQUAL(4, log_data.bat_chg_total_Wh);
    TEST_ASSERT_EQUAL(9, log_data.load_out_total_Wh);

    TEST_ASSERT_EQUAL(10, solar_terminal.neg_energy_Wh);
    TEST_ASSERT_EQUAL(3, bat_terminal.neg_energy_Wh);
    TEST_ASSERT_EQUAL(4, bat_terminal.pos_energy_Wh);
    TEST_ASSERT_EQUAL(9, load_terminal.pos_energy_Wh);

    // solar didn't come back yet
    TEST_ASSERT_EQUAL(0, log_data.day_counter);

    // now solar power comes back
    solar_terminal.voltage = bat_terminal.voltage + 1;
    log_update_energy(&log_data);

    // day counter should be increased and daily energy counters reset
    TEST_ASSERT_EQUAL(1, log_data.day_counter);
    TEST_ASSERT_EQUAL(0, solar_terminal.neg_energy_Wh);
    TEST_ASSERT_EQUAL(0, bat_terminal.neg_energy_Wh);
    TEST_ASSERT_EQUAL(0, bat_terminal.pos_energy_Wh);
    TEST_ASSERT_EQUAL(0, load_terminal.pos_energy_Wh);
}

void log_new_solar_voltage_max()
{
    solar_terminal.voltage = 40;
    log_update_min_max_values(&log_data);
    TEST_ASSERT_EQUAL(40, log_data.solar_voltage_max);
}

void log_new_bat_voltage_max()
{
    bat_terminal.voltage = 31;
    log_update_min_max_values(&log_data);
    TEST_ASSERT_EQUAL(31, log_data.battery_voltage_max);
}

void log_new_dcdc_current_max()
{
    dcdc.lvs->current = 21;
    log_update_min_max_values(&log_data);
    TEST_ASSERT_EQUAL(21, log_data.dcdc_current_max);
}

void log_new_load_current_max()
{
    load.port->current = 21;
    log_update_min_max_values(&log_data);
    TEST_ASSERT_EQUAL(21, log_data.load_current_max);
}

void log_solar_power_max()
{
    solar_terminal.power = -50;
    log_update_min_max_values(&log_data);
    TEST_ASSERT_EQUAL(50, log_data.solar_power_max_day);
    TEST_ASSERT_EQUAL(50, log_data.solar_power_max_total);
}

void log_load_power_max()
{
    load.port->power = 50;
    log_update_min_max_values(&log_data);
    TEST_ASSERT_EQUAL(50, log_data.load_power_max_day);
    TEST_ASSERT_EQUAL(50, log_data.load_power_max_total);
}

void log_new_mosfet_temp_max()
{
    dcdc.temp_mosfets = 80;
    log_update_min_max_values(&log_data);
    TEST_ASSERT_EQUAL(80, log_data.mosfet_temp_max);
}

void log_new_bat_temp_max()
{
    charger.bat_temperature = 45;
    log_update_min_max_values(&log_data);
    TEST_ASSERT_EQUAL(45, log_data.bat_temp_max);
}

extern float mcu_temp;

void log_new_int_temp_max()
{
    mcu_temp = 22;
    log_update_min_max_values(&log_data);
    TEST_ASSERT_EQUAL(22, log_data.int_temp_max);
}

void log_tests()
{
    UNITY_BEGIN();

    RUN_TEST(reset_counters_at_start_of_day);

    RUN_TEST(log_new_solar_voltage_max);
    RUN_TEST(log_new_bat_voltage_max);
    RUN_TEST(log_new_dcdc_current_max);
    RUN_TEST(log_new_load_current_max);
    RUN_TEST(log_solar_power_max);
    RUN_TEST(log_load_power_max);
    RUN_TEST(log_new_mosfet_temp_max);
    RUN_TEST(log_new_bat_temp_max);
    RUN_TEST(log_new_int_temp_max);

    UNITY_END();
}
