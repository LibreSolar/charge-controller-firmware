/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tests.h"
#include "daq.h"
#include "daq_stub.h"
#include "helper.h"
#include "setup.h"

#include <stdint.h>

static AdcValues adcval;

void test_adc_voltage_to_raw()
{
    int32_t raw;

    raw = adc_voltage_to_raw(0.0, 3300);
    TEST_ASSERT_EQUAL_INT32(0, raw);

    raw = adc_voltage_to_raw(3.3, 3300);
    TEST_ASSERT_EQUAL_INT32(65535, raw);

    raw = adc_voltage_to_raw(1.65, 3300);
    TEST_ASSERT_EQUAL_INT32(32767, raw);
}

void test_adc_raw_to_voltage()
{
    float voltage;

    voltage = adc_raw_to_voltage(0, 3300);
    TEST_ASSERT_EQUAL_FLOAT(0.0, voltage);

    voltage = adc_raw_to_voltage(65535, 3300);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 3.3, voltage);

    voltage = adc_raw_to_voltage(32767, 3300);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 1.65, voltage);
}

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
    adc_filtered_bak[ADC_POS(vref_mcu)] = get_adc_filtered(ADC_POS(vref_mcu));
    adc_filtered_bak[ADC_POS(v_high)] = get_adc_filtered(ADC_POS(v_high));

    // overwrite filtered values
    prepare_adc_filtered();

    TEST_ASSERT_EQUAL(get_adc_filtered(ADC_POS(vref_mcu)), adc_filtered_bak[ADC_POS(vref_mcu)]);
    TEST_ASSERT_EQUAL(get_adc_filtered(ADC_POS(v_high)), adc_filtered_bak[ADC_POS(v_high)]);
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
    TEST_ASSERT_EQUAL_FLOAT(adcval.battery_voltage, round(load.bus->voltage * 10) / 10);
    TEST_ASSERT_EQUAL_FLOAT(adcval.load_current, round(load.current * 10) / 10);
}

void check_temperature_readings()
{
    TEST_ASSERT_EQUAL_FLOAT(adcval.bat_temperature, round(charger.bat_temperature * 10) / 10);
}

void adc_alert_lv_undervoltage_triggering()
{
    dev_stat.clear_error(ERR_ANY_ERROR);
    battery_conf_init(&bat_conf, BAT_TYPE_LFP, 4, 100);
    daq_set_lv_limits(bat_conf.voltage_absolute_max, bat_conf.voltage_absolute_min);
    prepare_adc_filtered();
    adc_update_value(ADC_POS(v_low));

    // undervoltage test
    adcval.battery_voltage = bat_conf.voltage_absolute_min - 0.1;
    prepare_adc_readings(adcval);
    adc_update_value(ADC_POS(v_low));
    TEST_ASSERT_EQUAL(false, dev_stat.has_error(ERR_LOAD_VOLTAGE_DIP));
    adc_update_value(ADC_POS(v_low));
    load.control();
    TEST_ASSERT_EQUAL(true, flags_check(&load.error_flags, ERR_LOAD_VOLTAGE_DIP));
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF, load.state);

    // reset values
    adcval.battery_voltage = 13;
    prepare_adc_readings(adcval);
    prepare_adc_filtered();
    daq_update();

    charger.discharge_control(&bat_conf);
    TEST_ASSERT_EQUAL(false, dev_stat.has_error(ERR_BAT_UNDERVOLTAGE));  // ToDo
}

void adc_alert_lv_overvoltage_triggering()
{
    dev_stat.clear_error(ERR_ANY_ERROR);
    battery_conf_init(&bat_conf, BAT_TYPE_LFP, 4, 100);
    daq_set_lv_limits(bat_conf.voltage_absolute_max, bat_conf.voltage_absolute_min);
    prepare_adc_filtered();
    adc_update_value(ADC_POS(v_low));

    dcdc.state = DCDC_CONTROL_MPPT;

    // overvoltage test
    adcval.battery_voltage = bat_conf.voltage_absolute_max + 0.1;
    prepare_adc_readings(adcval);
    adc_update_value(ADC_POS(v_low));
    TEST_ASSERT_EQUAL(false, dev_stat.has_error(ERR_BAT_OVERVOLTAGE));
    adc_update_value(ADC_POS(v_low));
    TEST_ASSERT_EQUAL(true, dev_stat.has_error(ERR_BAT_OVERVOLTAGE));
    TEST_ASSERT_EQUAL(false, pwm_switch.active());
    TEST_ASSERT_EQUAL(DCDC_CONTROL_OFF, dcdc.state);

    // reset values
    adcval.battery_voltage = 12;
    prepare_adc_readings(adcval);
    prepare_adc_filtered();
    daq_update();

    charger.time_state_changed = time(NULL) - bat_conf.time_limit_recharge - 1;
    charger.charge_control(&bat_conf);
    TEST_ASSERT_EQUAL(false, dev_stat.has_error(ERR_BAT_OVERVOLTAGE));
}

void adc_alert_hv_overvoltage_triggering()
{
    dev_stat.clear_error(ERR_ANY_ERROR);
    daq_set_hv_limit(DT_PROP(DT_PATH(pcb), hs_voltage_max));
    prepare_adc_filtered();
    adc_update_value(ADC_POS(v_high));

    // overvoltage test
    adcval.solar_voltage = 85.0F;
    prepare_adc_readings(adcval);
    adc_update_value(ADC_POS(v_high));
    TEST_ASSERT_EQUAL(false, dev_stat.has_error(ERR_DCDC_HS_OVERVOLTAGE));
    adc_update_value(ADC_POS(v_high));
    TEST_ASSERT_EQUAL(true, dev_stat.has_error(ERR_DCDC_HS_OVERVOLTAGE));
    TEST_ASSERT_EQUAL(DCDC_CONTROL_OFF, dcdc.state);
}

void adc_alert_overflow_prevention()
{
    // try to set an alert that overflows the 12-bit ADC resolution
    uint16_t limit = adc_raw_clamp(1, UINT16_MAX + 1);
    TEST_ASSERT_EQUAL_HEX(UINT16_MAX, limit);
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

    RUN_TEST(test_adc_voltage_to_raw);
    RUN_TEST(test_adc_raw_to_voltage);

    RUN_TEST(check_filtering);

    // call original daq_update function
    daq_update();

    RUN_TEST(check_solar_terminal_readings);
    RUN_TEST(check_bat_terminal_readings);
    RUN_TEST(check_load_terminal_readings);

    //RUN_TEST(check_temperature_readings);     // TODO

    RUN_TEST(adc_alert_lv_undervoltage_triggering);
    RUN_TEST(adc_alert_lv_overvoltage_triggering);
    RUN_TEST(adc_alert_hv_overvoltage_triggering);
    RUN_TEST(adc_alert_overflow_prevention);

    UNITY_END();
}
