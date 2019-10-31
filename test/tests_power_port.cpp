
#include "tests.h"
#include "adc_dma_stub.h"
#include "adc_dma.h"

#include <time.h>
#include <stdio.h>
#include <math.h>

#include "main.h"

static adc_values_t adcval;

const float dcdc_current_sun = 3;
const float load_current = 1;
const int sun_hours = 1;
const int night_hours = 3;

void energy_calculation_init()
{
    log_data.solar_in_total_Wh = 0;
    log_data.load_out_total_Wh = 0;
    log_data.bat_chg_total_Wh = 0;
    log_data.bat_dis_total_Wh = 0;
    hv_terminal.neg_energy_Wh = 0.0;
    load_terminal.pos_energy_Wh = 0.0;
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
    update_measurements();

    for (int i = 0; i < 60*60*sun_hours; i++) {
        hv_terminal.energy_balance();
        lv_terminal.energy_balance();
        load_terminal.energy_balance();
    }

    // disable DC/DC = solar charging
    adcval.dcdc_current = 0;
    prepare_adc_readings(adcval);
    update_measurements();

    for (int i = 0; i < 60*60*night_hours; i++) {
        hv_terminal.energy_balance();
        lv_terminal.energy_balance();
        load_terminal.energy_balance();
    }
}

void charging_energy_calculation_valid()
{
    energy_calculation_init();
    // charging only during sun hours
    TEST_ASSERT_EQUAL_FLOAT(round(sun_hours * lv_terminal.voltage * (dcdc_current_sun - load_current)), round(lv_terminal.pos_energy_Wh));
}

void discharging_energy_calculation_valid()
{
    energy_calculation_init();
    // discharging (sum of current) only during dis hours
    TEST_ASSERT_EQUAL_FLOAT(round(night_hours * lv_terminal.voltage * adcval.load_current), round(lv_terminal.neg_energy_Wh));
}

void solar_input_energy_calculation_valid()
{
    energy_calculation_init();
    TEST_ASSERT_EQUAL_FLOAT(round(sun_hours * lv_terminal.voltage * dcdc_current_sun), round(hv_terminal.neg_energy_Wh));
}

void load_output_energy_calculation_valid()
{
    energy_calculation_init();
    TEST_ASSERT_EQUAL_FLOAT(round((sun_hours + night_hours) * lv_terminal.voltage * adcval.load_current), round(load_terminal.pos_energy_Wh));
}

void pass_voltage_targets_to_adjacent_bus()
{
    // without any droop
    bat_terminal.current = 0;
    bat_terminal.pass_voltage_targets(&dcdc_lv_port);
    TEST_ASSERT_EQUAL(bat_terminal.sink_voltage_min, dcdc_lv_port.sink_voltage_min);
    TEST_ASSERT_EQUAL(bat_terminal.sink_voltage_max, dcdc_lv_port.sink_voltage_max);
    TEST_ASSERT_EQUAL(bat_terminal.src_voltage_start, dcdc_lv_port.src_voltage_start);
    TEST_ASSERT_EQUAL(bat_terminal.src_voltage_stop, dcdc_lv_port.src_voltage_stop);

    // now with droop for positive current
    bat_terminal.current = 11;
    bat_terminal.pos_droop_res = 0.1;
    bat_terminal.pass_voltage_targets(&dcdc_lv_port);
    TEST_ASSERT_EQUAL(bat_terminal.sink_voltage_min, dcdc_lv_port.sink_voltage_min);
    TEST_ASSERT_EQUAL(bat_terminal.sink_voltage_max + 11*0.1, dcdc_lv_port.sink_voltage_max);
    TEST_ASSERT_EQUAL(bat_terminal.src_voltage_start + 11*0.1, dcdc_lv_port.src_voltage_start);
    TEST_ASSERT_EQUAL(bat_terminal.src_voltage_stop + 11*0.1, dcdc_lv_port.src_voltage_stop);

    // now with droop for negative current
    bat_terminal.current = -11;
    bat_terminal.pos_droop_res = 0.1;
    bat_terminal.pass_voltage_targets(&dcdc_lv_port);
    TEST_ASSERT_EQUAL(bat_terminal.sink_voltage_min, dcdc_lv_port.sink_voltage_min);
    TEST_ASSERT_EQUAL(bat_terminal.sink_voltage_max - 11*0.1, dcdc_lv_port.sink_voltage_max);
    TEST_ASSERT_EQUAL(bat_terminal.src_voltage_start - 11*0.1, dcdc_lv_port.src_voltage_start);
    TEST_ASSERT_EQUAL(bat_terminal.src_voltage_stop - 11*0.1, dcdc_lv_port.src_voltage_stop);
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

    RUN_TEST(pass_voltage_targets_to_adjacent_bus);

    UNITY_END();
}