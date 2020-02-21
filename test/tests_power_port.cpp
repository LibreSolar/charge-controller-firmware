/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tests.h"
#include "daq_stub.h"
#include "daq.h"

#include <time.h>
#include <stdio.h>
#include <math.h>

#include "setup.h"

static AdcValues adcval;

const float dcdc_current_sun = 3;
const float load_current = 1;
const int sun_hours = 1;
const int night_hours = 3;

void energy_calculation_init()
{
    dev_stat.solar_in_total_Wh = 0;
    dev_stat.load_out_total_Wh = 0;
    dev_stat.bat_chg_total_Wh = 0;
    dev_stat.bat_dis_total_Wh = 0;
    hv_terminal.neg_energy_Wh = 0.0;
    load.pos_energy_Wh = 0.0;
    lv_terminal.pos_energy_Wh = 0.0;
    lv_terminal.neg_energy_Wh = 0.0;

    // set desired measurement values
    adcval.bat_temperature = 25;
    adcval.battery_voltage = 12;
    adcval.dcdc_current = dcdc_current_sun;
    adcval.internal_temperature = 25;
    adcval.load_current = load_current;
    adcval.internal_temperature = 25;
    adcval.solar_voltage = 30;

    // insert values into ADC functions
    prepare_adc_readings(adcval);
    prepare_adc_filtered();
    daq_update();

    for (int i = 0; i < 60*60*sun_hours; i++) {
        hv_terminal.energy_balance();
        lv_terminal.energy_balance();
        load.energy_balance();
    }

    // disable DC/DC = solar charging
    adcval.dcdc_current = 0;
    prepare_adc_readings(adcval);
    prepare_adc_filtered();
    daq_update();

    for (int i = 0; i < 60*60*night_hours; i++) {
        hv_terminal.energy_balance();
        lv_terminal.energy_balance();
        load.energy_balance();
    }
}

void charging_energy_calculation_valid()
{
    energy_calculation_init();
    // charging only during sun hours
    TEST_ASSERT_EQUAL_FLOAT(round(sun_hours * lv_terminal.bus->voltage * (dcdc_current_sun - load_current)), round(lv_terminal.pos_energy_Wh));
}

void discharging_energy_calculation_valid()
{
    energy_calculation_init();
    // discharging (sum of current) only during dis hours
    TEST_ASSERT_EQUAL_FLOAT(round(night_hours * lv_terminal.bus->voltage * adcval.load_current), round(lv_terminal.neg_energy_Wh));
}

void solar_input_energy_calculation_valid()
{
    energy_calculation_init();
    TEST_ASSERT_EQUAL_FLOAT(round(sun_hours * lv_terminal.bus->voltage * dcdc_current_sun), round(hv_terminal.neg_energy_Wh));
}

void load_output_energy_calculation_valid()
{
    energy_calculation_init();
    TEST_ASSERT_EQUAL_FLOAT(round((sun_hours + night_hours) * lv_terminal.bus->voltage * adcval.load_current), round(load.pos_energy_Wh));
}

void power_port_tests()
{
    energy_calculation_init();

    UNITY_BEGIN();

    // energy calculation
    RUN_TEST(charging_energy_calculation_valid);
    RUN_TEST(discharging_energy_calculation_valid);
    RUN_TEST(solar_input_energy_calculation_valid);
    RUN_TEST(load_output_energy_calculation_valid);

    UNITY_END();
}
