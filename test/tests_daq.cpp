/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tests.h"
#include "daq.h"
#include "daq_stub.h"

#include "main.h"

#include <stdint.h>

static AdcValues adcval;

// testing only for 2 values
void check_filtering()
{
    // reset values
    clear_adc_filtered();

    // collect 1000 samples to update filtered values
    for (int s = 0; s < 1000; s++) {
        for (int i = 0; i < NUM_ADC_CH; i++) {
            adc_update_value(i);
        }
    }

    uint32_t adc_filtered_bak[NUM_ADC_CH];
    adc_filtered_bak[ADC_POS_VREF_MCU] = get_adc_filtered(ADC_POS_VREF_MCU);
    adc_filtered_bak[ADC_POS_V_SOLAR] = get_adc_filtered(ADC_POS_V_SOLAR);

    // overwrite filtered values
    prepare_adc_filtered();

    TEST_ASSERT_EQUAL(get_adc_filtered(ADC_POS_VREF_MCU), adc_filtered_bak[ADC_POS_VREF_MCU]);
    TEST_ASSERT_EQUAL(get_adc_filtered(ADC_POS_V_SOLAR), adc_filtered_bak[ADC_POS_V_SOLAR]);
}

void check_solar_terminal_readings()
{
    TEST_ASSERT_EQUAL_FLOAT(adcval.solar_voltage, round(hv_terminal.bus->voltage * 10) / 10);
    TEST_ASSERT_EQUAL_FLOAT(adcval.dcdc_current / adcval.solar_voltage * adcval.battery_voltage,
        -round(hv_terminal.current * 10) / 10);
}

void check_bat_terminal_readings()
{
    TEST_ASSERT_EQUAL_FLOAT(adcval.battery_voltage, round(lv_terminal.bus->voltage * 10) / 10);
    TEST_ASSERT_EQUAL_FLOAT(adcval.dcdc_current - adcval.load_current,
        round(lv_terminal.current * 10) / 10);
}

void check_load_terminal_readings()
{
    TEST_ASSERT_EQUAL_FLOAT(adcval.battery_voltage, round(load_terminal.bus->voltage * 10) / 10);
    TEST_ASSERT_EQUAL_FLOAT(adcval.load_current, round(load_terminal.current * 10) / 10);
}

void check_lv_bus_int_readings()
{
    TEST_ASSERT_EQUAL_FLOAT(adcval.battery_voltage, round(dcdc_lv_port.bus->voltage * 10) / 10);
    TEST_ASSERT_EQUAL_FLOAT(adcval.dcdc_current, round(dcdc_lv_port.current * 10) / 10);
}

void check_temperature_readings()
{
    TEST_ASSERT_EQUAL_FLOAT(adcval.bat_temperature, round(charger.bat_temperature * 10) / 10);
}

void adc_alert_undervoltage_triggering()
{
    dev_stat.clear_error(ERR_ANY_ERROR);
    battery_conf_init(&bat_conf, BAT_TYPE_LFP, 4, 100);
    daq_set_lv_alerts(bat_conf.voltage_absolute_max, bat_conf.voltage_absolute_min);
    prepare_adc_filtered();
    adc_update_value(ADC_POS_V_BAT);

    // undervoltage test
    adcval.battery_voltage = bat_conf.voltage_absolute_min - 0.1;
    prepare_adc_readings(adcval);
    adc_update_value(ADC_POS_V_BAT);
    TEST_ASSERT_EQUAL(false, dev_stat.has_error(ERR_LOAD_VOLTAGE_DIP));
    adc_update_value(ADC_POS_V_BAT);
    load.control();
    TEST_ASSERT_EQUAL(true, dev_stat.has_error(ERR_LOAD_VOLTAGE_DIP));
    TEST_ASSERT_EQUAL(false, load.pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_OVERCURRENT, load.state);

    // reset values
    adcval.battery_voltage = 13;
    prepare_adc_readings(adcval);
    prepare_adc_filtered();
    daq_update();

    charger.discharge_control(&bat_conf);
    TEST_ASSERT_EQUAL(false, dev_stat.has_error(ERR_BAT_UNDERVOLTAGE));
}

void adc_alert_overvoltage_triggering()
{
    dev_stat.clear_error(ERR_ANY_ERROR);
    battery_conf_init(&bat_conf, BAT_TYPE_LFP, 4, 100);
    daq_set_lv_alerts(bat_conf.voltage_absolute_max, bat_conf.voltage_absolute_min);
    prepare_adc_filtered();
    adc_update_value(ADC_POS_V_BAT);

    dcdc.state = DCDC_STATE_MPPT;

    // overvoltage test
    adcval.battery_voltage = bat_conf.voltage_absolute_max + 0.1;
    prepare_adc_readings(adcval);
    adc_update_value(ADC_POS_V_BAT);
    TEST_ASSERT_EQUAL(false, dev_stat.has_error(ERR_BAT_OVERVOLTAGE));
    adc_update_value(ADC_POS_V_BAT);
    TEST_ASSERT_EQUAL(true, dev_stat.has_error(ERR_BAT_OVERVOLTAGE));
    TEST_ASSERT_EQUAL(false, pwm_switch.active());
    TEST_ASSERT_EQUAL(DCDC_STATE_OFF, dcdc.state);

    // reset values
    adcval.battery_voltage = 12;
    prepare_adc_readings(adcval);
    prepare_adc_filtered();
    daq_update();

    charger.time_state_changed = time(NULL) - bat_conf.time_limit_recharge - 1;
    charger.charge_control(&bat_conf);
    TEST_ASSERT_EQUAL(false, dev_stat.has_error(ERR_BAT_OVERVOLTAGE));
}

void adc_alert_overflow_prevention()
{
    // try to set an alert that overflows the 12-bit ADC resolution
    uint16_t limit = adc_get_alert_limit(1, 4097);
    TEST_ASSERT_EQUAL_HEX((uint16_t)(UINT16_MAX << 4), limit);
}

/** Data acquisition tests
 *
 * Purpose: Check if raw data from 2 voltage and 2 current measurements are converted
 * to calculated voltage/current measurements of different DC buses
 */
void daq_tests()
{
    adcval.bat_temperature = 25;
    adcval.battery_voltage = 12;
    adcval.dcdc_current = 3;
    adcval.internal_temperature = 25;
    adcval.load_current = 1;
    adcval.solar_voltage = 30;
    prepare_adc_readings(adcval);

    UNITY_BEGIN();

    RUN_TEST(check_filtering);

    // call original daq_update function
    daq_update();

    RUN_TEST(check_solar_terminal_readings);
    RUN_TEST(check_bat_terminal_readings);
    RUN_TEST(check_load_terminal_readings);
    RUN_TEST(check_lv_bus_int_readings);

    //RUN_TEST(check_temperature_readings);     // TODO

    RUN_TEST(adc_alert_undervoltage_triggering);
    RUN_TEST(adc_alert_overvoltage_triggering);
    RUN_TEST(adc_alert_overflow_prevention);

    UNITY_END();
}
