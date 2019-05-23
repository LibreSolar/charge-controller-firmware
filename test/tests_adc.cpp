
#include "tests.h"
#include "adc_dma.h"
#include "adc_dma_stub.h"

extern dcdc_t dcdc;
extern dc_bus_t hs_bus;
extern dc_bus_t ls_bus;
extern dc_bus_t *bat_port;
extern dc_bus_t load_bus;
extern charger_t charger;
extern load_output_t load;

static adc_values_t adcval;

void check_solar_bus_readings()
{
    TEST_ASSERT_EQUAL_FLOAT(adcval.solar_voltage, round(hs_bus.voltage * 10) / 10);
    TEST_ASSERT_EQUAL_FLOAT(adcval.dcdc_current / adcval.solar_voltage * adcval.battery_voltage, -round(hs_bus.current * 10) / 10);
}

void check_battery_bus_readings()
{
    TEST_ASSERT_EQUAL_FLOAT(adcval.battery_voltage, round(ls_bus.voltage * 10) / 10);
    TEST_ASSERT_EQUAL_FLOAT(adcval.dcdc_current - adcval.load_current, round(ls_bus.current * 10) / 10);
}

void check_load_bus_readings()
{
    TEST_ASSERT_EQUAL_FLOAT(adcval.battery_voltage, round(load_bus.voltage * 10) / 10);
    TEST_ASSERT_EQUAL_FLOAT(adcval.load_current, round(load_bus.current * 10) / 10);
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
    update_measurements(&dcdc, &charger, &load, &hs_bus, &ls_bus, &load_bus);

    UNITY_BEGIN();

    RUN_TEST(check_solar_bus_readings);
    RUN_TEST(check_battery_bus_readings);
    RUN_TEST(check_load_bus_readings);
    //RUN_TEST(check_temperature_readings);     // TODO

    UNITY_END();
}