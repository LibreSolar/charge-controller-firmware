/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tests.h"

#include <math.h>
#include <stdio.h>
#include <time.h>

#include "setup.h"

ZTEST(device_status, test_reset_counters_at_start_of_day)
{
    solar_terminal.bus->voltage = bat_terminal.bus->voltage - 1;

    dev_stat.day_counter = 0;

    solar_terminal.neg_energy_Wh = 10.0;
    bat_terminal.neg_energy_Wh = 3.0;
    bat_terminal.pos_energy_Wh = 4.0;
    load.pos_energy_Wh = 9.0;

    // 5 hours without sun
    for (int i = 0; i <= 5 * 60 * 60; i++) {
        dev_stat.update_energy();
    }

    zassert_equal(dev_stat.solar_in_total_Wh, 10);
    zassert_equal(dev_stat.bat_dis_total_Wh, 3);
    zassert_equal(dev_stat.bat_chg_total_Wh, 4);
    zassert_equal(dev_stat.load_out_total_Wh, 9);

    zassert_equal(solar_terminal.neg_energy_Wh, 10);
    zassert_equal(bat_terminal.neg_energy_Wh, 3);
    zassert_equal(bat_terminal.pos_energy_Wh, 4);
    zassert_equal(load.pos_energy_Wh, 9);

    // solar didn't come back yet
    zassert_equal(dev_stat.day_counter, 0);

    // now solar power comes back
    solar_terminal.bus->voltage = bat_terminal.bus->voltage + 1;
    dev_stat.update_energy();

    // day counter should be increased and daily energy counters reset
    zassert_equal(dev_stat.day_counter, 1);
    zassert_equal(solar_terminal.neg_energy_Wh, 0);
    zassert_equal(bat_terminal.neg_energy_Wh, 0);
    zassert_equal(bat_terminal.pos_energy_Wh, 0);
    zassert_equal(load.pos_energy_Wh, 0);
}

ZTEST(device_status, test_dev_stat_new_solar_voltage_max)
{
    solar_terminal.bus->voltage = 40;
    dev_stat.update_min_max_values();
    zassert_equal(dev_stat.solar_voltage_max, 40);
}

ZTEST(device_status, test_dev_stat_new_bat_voltage_max)
{
    bat_terminal.bus->voltage = 31;
    dev_stat.update_min_max_values();
    zassert_equal(dev_stat.battery_voltage_max, 31);
}

ZTEST(device_status, test_dev_stat_new_dcdc_current_max)
{
    dcdc.inductor_current = 21;
    dev_stat.update_min_max_values();
    zassert_equal(dev_stat.dcdc_current_max, 21);
}

ZTEST(device_status, test_dev_stat_new_load_current_max)
{
    load.current = 21;
    dev_stat.update_min_max_values();
    zassert_equal(dev_stat.load_current_max, 21);
}

ZTEST(device_status, test_dev_stat_solar_power_max)
{
    solar_terminal.power = -50;
    dev_stat.update_min_max_values();
    zassert_equal(dev_stat.solar_power_max_day, 50);
    zassert_equal(dev_stat.solar_power_max_total, 50);
}

ZTEST(device_status, test_dev_stat_load_power_max)
{
    load.power = 50;
    dev_stat.update_min_max_values();
    zassert_equal(dev_stat.load_power_max_day, 50);
    zassert_equal(dev_stat.load_power_max_total, 50);
}

ZTEST(device_status, test_dev_stat_new_mosfet_temp_max)
{
    dcdc.temp_mosfets = 80;
    dev_stat.update_min_max_values();
    zassert_equal(dev_stat.mosfet_temp_max, 80);
}

ZTEST(device_status, test_dev_stat_new_bat_temp_max)
{
    charger.bat_temperature = 45;
    dev_stat.update_min_max_values();
    zassert_equal(dev_stat.bat_temp_max, 45);
}

ZTEST(device_status, test_dev_stat_new_int_temp_max)
{
    dev_stat.int_temp_max = 20;
    dev_stat.internal_temp = 22;
    dev_stat.update_min_max_values();
    zassert_equal(dev_stat.int_temp_max, 22);
}

ZTEST_SUITE(device_status, NULL, NULL, NULL, NULL, NULL);
