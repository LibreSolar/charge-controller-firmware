/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tests.h"

#include <time.h>
#include <stdio.h>
#include <math.h>

#include "setup.h"

void reset_counters_at_start_of_day()
{
    solar_terminal.bus->voltage = bat_terminal.bus->voltage - 1;

    dev_stat.day_counter = 0;

    solar_terminal.neg_energy_Wh = 10.0;
    bat_terminal.neg_energy_Wh = 3.0;
    bat_terminal.pos_energy_Wh = 4.0;
    load.pos_energy_Wh = 9.0;

    // 5 houurs without sun
    for (int i = 0; i <= 5 * 60 * 60; i++) {
        dev_stat.update_energy();
    }

    TEST_ASSERT_EQUAL(10, dev_stat.solar_in_total_Wh);
    TEST_ASSERT_EQUAL(3, dev_stat.bat_dis_total_Wh);
    TEST_ASSERT_EQUAL(4, dev_stat.bat_chg_total_Wh);
    TEST_ASSERT_EQUAL(9, dev_stat.load_out_total_Wh);

    TEST_ASSERT_EQUAL(10, solar_terminal.neg_energy_Wh);
    TEST_ASSERT_EQUAL(3, bat_terminal.neg_energy_Wh);
    TEST_ASSERT_EQUAL(4, bat_terminal.pos_energy_Wh);
    TEST_ASSERT_EQUAL(9, load.pos_energy_Wh);

    // solar didn't come back yet
    TEST_ASSERT_EQUAL(0, dev_stat.day_counter);

    // now solar power comes back
    solar_terminal.bus->voltage = bat_terminal.bus->voltage + 1;
    dev_stat.update_energy();

    // day counter should be increased and daily energy counters reset
    TEST_ASSERT_EQUAL(1, dev_stat.day_counter);
    TEST_ASSERT_EQUAL(0, solar_terminal.neg_energy_Wh);
    TEST_ASSERT_EQUAL(0, bat_terminal.neg_energy_Wh);
    TEST_ASSERT_EQUAL(0, bat_terminal.pos_energy_Wh);
    TEST_ASSERT_EQUAL(0, load.pos_energy_Wh);
}

void dev_stat_new_solar_voltage_max()
{
    solar_terminal.bus->voltage = 40;
    dev_stat.update_min_max_values();
    TEST_ASSERT_EQUAL(40, dev_stat.solar_voltage_max);
}

void dev_stat_new_bat_voltage_max()
{
    bat_terminal.bus->voltage = 31;
    dev_stat.update_min_max_values();
    TEST_ASSERT_EQUAL(31, dev_stat.battery_voltage_max);
}

void dev_stat_new_dcdc_current_max()
{
    dcdc.inductor_current = 21;
    dev_stat.update_min_max_values();
    TEST_ASSERT_EQUAL(21, dev_stat.dcdc_current_max);
}

void dev_stat_new_load_current_max()
{
    load.current = 21;
    dev_stat.update_min_max_values();
    TEST_ASSERT_EQUAL(21, dev_stat.load_current_max);
}

void dev_stat_solar_power_max()
{
    solar_terminal.power = -50;
    dev_stat.update_min_max_values();
    TEST_ASSERT_EQUAL(50, dev_stat.solar_power_max_day);
    TEST_ASSERT_EQUAL(50, dev_stat.solar_power_max_total);
}

void dev_stat_load_power_max()
{
    load.power = 50;
    dev_stat.update_min_max_values();
    TEST_ASSERT_EQUAL(50, dev_stat.load_power_max_day);
    TEST_ASSERT_EQUAL(50, dev_stat.load_power_max_total);
}

void dev_stat_new_mosfet_temp_max()
{
    dcdc.temp_mosfets = 80;
    dev_stat.update_min_max_values();
    TEST_ASSERT_EQUAL(80, dev_stat.mosfet_temp_max);
}

void dev_stat_new_bat_temp_max()
{
    charger.bat_temperature = 45;
    dev_stat.update_min_max_values();
    TEST_ASSERT_EQUAL(45, dev_stat.bat_temp_max);
}

void dev_stat_new_int_temp_max()
{
    dev_stat.int_temp_max = 20;
    dev_stat.internal_temp = 22;
    dev_stat.update_min_max_values();
    TEST_ASSERT_EQUAL(22, dev_stat.int_temp_max);
}

void device_status_tests()
{
    UNITY_BEGIN();

    RUN_TEST(reset_counters_at_start_of_day);

    RUN_TEST(dev_stat_new_solar_voltage_max);
    RUN_TEST(dev_stat_new_bat_voltage_max);
    RUN_TEST(dev_stat_new_dcdc_current_max);
    RUN_TEST(dev_stat_new_load_current_max);
    RUN_TEST(dev_stat_solar_power_max);
    RUN_TEST(dev_stat_load_power_max);
    RUN_TEST(dev_stat_new_mosfet_temp_max);
    RUN_TEST(dev_stat_new_bat_temp_max);
    RUN_TEST(dev_stat_new_int_temp_max);

    UNITY_END();
}
