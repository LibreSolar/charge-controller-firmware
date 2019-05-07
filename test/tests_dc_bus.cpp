
#include "tests.h"
#include "adc_dma_stub.h"
#include "adc_dma.h"

#include "battery.h"
#include "dc_bus.h"
#include "charger.h"
#include "log.h"

#include <time.h>
#include <stdio.h>
#include <math.h>

extern dcdc_t dcdc;
extern dc_bus_t hs_bus;
extern dc_bus_t ls_bus;
extern dc_bus_t *bat_port;
extern dc_bus_t load_bus;
extern battery_state_t bat_state;
extern battery_conf_t bat_conf;
extern load_output_t load;
extern log_data_t log_data;

static void init_structs()
{
    battery_conf_init(&bat_conf, BAT_TYPE_FLOODED, 6, 100);
    battery_state_init(&bat_state);
    dc_bus_init_bat(&ls_bus, &bat_conf);
    bat_state.chg_state = CHG_STATE_IDLE;
}

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
    hs_bus.dis_energy_Wh = 0.0;
    load_bus.chg_energy_Wh = 0.0;
    ls_bus.chg_energy_Wh = 0.0;
    ls_bus.dis_energy_Wh = 0.0;

    // set desired measurement values
    adcval.bat_temperature = 25;
    adcval.battery_voltage = 12;
    adcval.dcdc_current = dcdc_current_sun;
    adcval.internal_temperature = 25;
    adcval.load_current = load_current;
    adcval.mcu_temperature = 25;
    adcval.solar_voltage = 30;

    // insert values into ADC functions
    prepare_adc_readings(adcval);
    update_measurements(&dcdc, &bat_state, &load, &hs_bus, &ls_bus, &load_bus);

    for (int i = 0; i < 60*60*sun_hours; i++) {
        dc_bus_energy_balance(&hs_bus);
        dc_bus_energy_balance(&ls_bus);
        dc_bus_energy_balance(&load_bus);
    }

    // disable DC/DC = solar charging
    adcval.dcdc_current = 0;
    prepare_adc_readings(adcval);
    update_measurements(&dcdc, &bat_state, &load, &hs_bus, &ls_bus, &load_bus);

    for (int i = 0; i < 60*60*night_hours; i++) {
        dc_bus_energy_balance(&hs_bus);
        dc_bus_energy_balance(&ls_bus);
        dc_bus_energy_balance(&load_bus);
    }
}

void charging_energy_calculation_valid()
{
    energy_calculation_init();
    // charging only during sun hours
    TEST_ASSERT_EQUAL_FLOAT(round(sun_hours * ls_bus.voltage * (dcdc_current_sun - load_current)), round(ls_bus.chg_energy_Wh));
}

void discharging_energy_calculation_valid()
{
    energy_calculation_init();
    // discharging (sum of current) only during dis hours
    TEST_ASSERT_EQUAL_FLOAT(round(night_hours * ls_bus.voltage * adcval.load_current), round(ls_bus.dis_energy_Wh));
}

void solar_input_energy_calculation_valid()
{
    energy_calculation_init();
    TEST_ASSERT_EQUAL_FLOAT(round(sun_hours * ls_bus.voltage * dcdc_current_sun), round(hs_bus.dis_energy_Wh));
}

void load_output_energy_calculation_valid()
{
    energy_calculation_init();
    TEST_ASSERT_EQUAL_FLOAT(round((sun_hours + night_hours) * ls_bus.voltage * adcval.load_current), round(load_bus.chg_energy_Wh));
}

void dc_bus_tests()
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