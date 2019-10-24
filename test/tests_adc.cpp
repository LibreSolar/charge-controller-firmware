
#include "tests.h"
#include "adc_dma.h"
#include "adc_dma_stub.h"

#include "main.h"

static adc_values_t adcval;

void check_solar_terminal_readings()
{
    TEST_ASSERT_EQUAL_FLOAT(adcval.solar_voltage, round(hv_terminal.voltage * 10) / 10);
    TEST_ASSERT_EQUAL_FLOAT(adcval.dcdc_current / adcval.solar_voltage * adcval.battery_voltage,
        -round(hv_terminal.current * 10) / 10);
}

void check_bat_terminal_readings()
{
    TEST_ASSERT_EQUAL_FLOAT(adcval.battery_voltage, round(lv_terminal.voltage * 10) / 10);
    TEST_ASSERT_EQUAL_FLOAT(adcval.dcdc_current - adcval.load_current,
        round(lv_terminal.current * 10) / 10);
}

void check_load_terminal_readings()
{
    TEST_ASSERT_EQUAL_FLOAT(adcval.battery_voltage, round(load_terminal.voltage * 10) / 10);
    TEST_ASSERT_EQUAL_FLOAT(adcval.load_current, round(load_terminal.current * 10) / 10);
}

void check_lv_bus_int_readings()
{
    TEST_ASSERT_EQUAL_FLOAT(adcval.battery_voltage, round(dcdc_lv_port.voltage * 10) / 10);
    TEST_ASSERT_EQUAL_FLOAT(adcval.dcdc_current, round(dcdc_lv_port.current * 10) / 10);
}

void check_temperature_readings()
{
    TEST_ASSERT_EQUAL_FLOAT(adcval.bat_temperature, round(charger.bat_temperature * 10) / 10);
}

/** ADC conversion test
 *
 * Purpose: Check if raw data from 2 voltage and 2 current measurements are converted
 * to calculated voltage/current measurements of different DC buses
 */
void adc_tests()
{
    adcval.bat_temperature = 25;
    adcval.battery_voltage = 12;
    adcval.dcdc_current = 3;
    adcval.internal_temperature = 25;
    adcval.load_current = 1;
    adcval.mcu_temperature = 25;
    adcval.solar_voltage = 30;
    prepare_adc_readings(adcval);

    // call original update_measurements function
    update_measurements();

    UNITY_BEGIN();

    RUN_TEST(check_solar_terminal_readings);
    RUN_TEST(check_bat_terminal_readings);
    RUN_TEST(check_load_terminal_readings);
    RUN_TEST(check_lv_bus_int_readings);

    //RUN_TEST(check_temperature_readings);     // TODO

    UNITY_END();
}