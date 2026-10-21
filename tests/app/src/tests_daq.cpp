/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "daq.h"
#include "daq_stub.h"
#include "helper.h"
#include "setup.h"
#include "tests.h"

#include <stdint.h>

static AdcValues adcval;

ZTEST(daq, test_adc_voltage_to_raw)
{
    int32_t raw;

    raw = adc_voltage_to_raw(0.0, 3300);
    zassert_equal(raw, 0);

    raw = adc_voltage_to_raw(3.3, 3300);
    zassert_equal(raw, 65535);

    raw = adc_voltage_to_raw(1.65, 3300);
    zassert_equal(raw, 32767);
}

ZTEST(daq, test_adc_raw_to_voltage)
{
    float voltage;

    voltage = adc_raw_to_voltage(0, 3300);
    assert_float_equal(voltage, 0.0);

    voltage = adc_raw_to_voltage(65535, 3300);
    zassert_within(voltage, 3.3F, 0.01F);

    voltage = adc_raw_to_voltage(32767, 3300);
    zassert_within(voltage, 1.65F, 0.01F);
}

// testing only for 2 values
ZTEST(daq, test_check_filtering)
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

    zassert_equal(adc_filtered_bak[ADC_POS(vref_mcu)], get_adc_filtered(ADC_POS(vref_mcu)));
    zassert_equal(adc_filtered_bak[ADC_POS(v_high)], get_adc_filtered(ADC_POS(v_high)));
}

ZTEST(daq, test_check_solar_terminal_readings)
{
    assert_float_equal(round(hv_terminal.bus->voltage * 10) / 10, adcval.solar_voltage);
    assert_float_equal(-round(hv_terminal.current * 10) / 10,
                       adcval.dcdc_current / adcval.solar_voltage * adcval.battery_voltage);
}

ZTEST(daq, test_check_bat_terminal_readings)
{
    assert_float_equal(round(lv_terminal.bus->voltage * 10) / 10, adcval.battery_voltage);
    assert_float_equal(round(lv_terminal.current * 10) / 10,
                       adcval.dcdc_current - adcval.load_current);
}

ZTEST(daq, test_check_load_terminal_readings)
{
    assert_float_equal(round(load.bus->voltage * 10) / 10, adcval.battery_voltage);
    assert_float_equal(round(load.current * 10) / 10, adcval.load_current);
}

void check_temperature_readings()
{
    assert_float_equal(round(charger.bat_temperature * 10) / 10, adcval.bat_temperature);
}

ZTEST(daq, test_adc_alert_lv_undervoltage_triggering)
{
    dev_stat.clear_error(ERR_ANY_ERROR);
    battery_conf_init(&bat_conf, BAT_TYPE_LFP, 4, 100);
    daq_set_lv_limits(bat_conf.absolute_max_voltage, bat_conf.absolute_min_voltage);
    prepare_adc_filtered();
    adc_update_value(ADC_POS(v_low));

    // undervoltage test
    adcval.battery_voltage = bat_conf.absolute_min_voltage - 0.1f;
    prepare_adc_readings(adcval);
    adc_update_value(ADC_POS(v_low));
    zassert_equal(dev_stat.has_error(ERR_LOAD_VOLTAGE_DIP), false);
    adc_update_value(ADC_POS(v_low));
    load.control();
    zassert_equal(flags_check(&load.error_flags, ERR_LOAD_VOLTAGE_DIP), true);
    zassert_equal(load.state, LOAD_STATE_OFF);

    // reset values
    adcval.battery_voltage = 13;
    prepare_adc_readings(adcval);
    prepare_adc_filtered();
    daq_update();

    charger.discharge_control(&bat_conf);
    zassert_equal(dev_stat.has_error(ERR_BAT_UNDERVOLTAGE), false); // ToDo
}

ZTEST(daq, test_adc_alert_lv_overvoltage_triggering)
{
    dev_stat.clear_error(ERR_ANY_ERROR);
    battery_conf_init(&bat_conf, BAT_TYPE_LFP, 4, 100);
    daq_set_lv_limits(bat_conf.absolute_max_voltage, bat_conf.absolute_min_voltage);
    prepare_adc_filtered();
    adc_update_value(ADC_POS(v_low));

    dcdc.state = DCDC_CONTROL_MPPT;

    // overvoltage test
    adcval.battery_voltage = bat_conf.absolute_max_voltage + 0.1f;
    prepare_adc_readings(adcval);
    adc_update_value(ADC_POS(v_low));
    zassert_equal(dev_stat.has_error(ERR_BAT_OVERVOLTAGE), false);
    adc_update_value(ADC_POS(v_low));
    zassert_equal(dev_stat.has_error(ERR_BAT_OVERVOLTAGE), true);
    // zassert_equal(pwm_switch.active(), false);
    zassert_equal(dcdc.state, DCDC_CONTROL_OFF);

    // reset values
    adcval.battery_voltage = 12;
    prepare_adc_readings(adcval);
    prepare_adc_filtered();
    daq_update();

    charger.time_state_changed = time(NULL) - bat_conf.time_limit_recharge - 1;
    charger.charge_control(&bat_conf);
    zassert_equal(dev_stat.has_error(ERR_BAT_OVERVOLTAGE), false);
}

ZTEST(daq, test_adc_alert_hv_overvoltage_triggering)
{
    dev_stat.clear_error(ERR_ANY_ERROR);
    daq_set_hv_limit(DT_PROP(DT_PATH(pcb), hs_voltage_max));
    prepare_adc_filtered();
    adc_update_value(ADC_POS(v_high));

    // overvoltage test
    adcval.solar_voltage = 85.0F;
    prepare_adc_readings(adcval);
    adc_update_value(ADC_POS(v_high));
    zassert_equal(dev_stat.has_error(ERR_DCDC_HS_OVERVOLTAGE), false);
    adc_update_value(ADC_POS(v_high));
    zassert_equal(dev_stat.has_error(ERR_DCDC_HS_OVERVOLTAGE), true);
    zassert_equal(dcdc.state, DCDC_CONTROL_OFF);
}

ZTEST(daq, test_adc_alert_overflow_prevention)
{
    // try to set an alert that overflows the 12-bit ADC resolution
    uint16_t limit = adc_raw_clamp(1, UINT16_MAX + 1);
    zassert_equal(limit, UINT16_MAX);
}

/** Data acquisition tests
 *
 * Purpose: Check if raw data from 2 voltage and 2 current measurements are converted
 * to calculated voltage/current measurements of different DC buses
 */
static void daq_before(void *fixture)
{
    ARG_UNUSED(fixture);

    adcval.bat_temperature = 25;
    adcval.battery_voltage = 12;
    adcval.dcdc_current = 3;
    adcval.internal_temperature = 25;
    adcval.load_current = 1;
    adcval.solar_voltage = 30;
    prepare_adc_readings(adcval);
    prepare_adc_filtered();

    // call original daq_update function
    daq_update();
}

ZTEST_SUITE(daq, NULL, NULL, daq_before, NULL, NULL);
