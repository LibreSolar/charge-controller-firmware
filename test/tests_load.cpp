
#include "tests.h"
#include "adc_dma_stub.h"
#include "adc_dma.h"

#include <time.h>
#include <stdio.h>
#include <math.h>

#include "main.h"

void disabled_to_on_if_everything_fine()
{
    PowerPort port;
    LoadOutput load(&port);
    port.init_load(14.6);
    port.pos_current_limit = 10;
    load.state_machine();
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.state);
}

void disabled_to_off_low_soc_if_error_flag_set()
{
    PowerPort port;
    LoadOutput load(&port);
    port.init_load(14.6);
    port.pos_current_limit = 0;
    dev_stat.clear_error(ERR_ANY_ERROR);
    dev_stat.set_error(ERR_BAT_UNDERVOLTAGE);
    load.state_machine();
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_LOW_SOC, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_LOW_SOC, load.usb_state);
}

void disabled_to_off_bat_temp_if_error_flag_set()
{
    PowerPort port;
    LoadOutput load(&port);
    port.init_load(14.6);
    port.pos_current_limit = 0;

    // overtemp
    dev_stat.clear_error(ERR_ANY_ERROR);
    dev_stat.set_error(ERR_BAT_CHG_OVERTEMP);
    load.state_machine();
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_TEMPERATURE, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_TEMPERATURE, load.usb_state);

    // undertemp
    load.state = LOAD_STATE_DISABLED;
    dev_stat.clear_error(ERR_ANY_ERROR); 
    dev_stat.set_error(ERR_BAT_CHG_UNDERTEMP);
    load.state_machine();
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_TEMPERATURE, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_TEMPERATURE, load.usb_state);
}

void off_low_soc_to_on_after_delay()
{
    PowerPort port;
    LoadOutput load(&port);
    port.init_load(14.6);
    load.state = LOAD_STATE_OFF_LOW_SOC;
    load.usb_state = LOAD_STATE_OFF_LOW_SOC;
    port.pos_current_limit = 10;

    load.lvd_timestamp = time(NULL) - load.lvd_recovery_delay + 1;
    load.state_machine();
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_LOW_SOC, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_LOW_SOC, load.usb_state);

    load.lvd_timestamp = time(NULL) - load.lvd_recovery_delay - 1;
    load.state_machine();
    load.state_machine();   // call twice as it goes through disabled state
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.usb_state);
}

void off_overcurrent_to_on_after_delay()
{
    PowerPort port;
    LoadOutput load(&port);
    port.init_load(14.6);
    load.state = LOAD_STATE_OFF_OVERCURRENT;
    load.usb_state = LOAD_STATE_ON;
    port.pos_current_limit = 10;

    load.overcurrent_timestamp = time(NULL) - load.overcurrent_recovery_delay + 1;
    load.state_machine();
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_OVERCURRENT, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.usb_state);       // not affected by overcurrent

    load.overcurrent_timestamp = time(NULL) - load.overcurrent_recovery_delay - 1;
    load.state_machine();
    load.state_machine();   // call twice as it goes through disabled state
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.usb_state);
}

void off_overvoltage_to_on_at_lower_voltage()
{
    PowerPort port;
    LoadOutput load(&port);
    port.init_load(14.6);
    port.pos_current_limit = 10;
    load.state = LOAD_STATE_OFF_OVERVOLTAGE;
    load.usb_state = LOAD_STATE_ON;
    port.voltage = port.sink_voltage_max + 0.1;

    load.state_machine();
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_OVERVOLTAGE, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.usb_state);       // not affected by overvoltage

    port.voltage = port.sink_voltage_max - 0.1;     // test hysteresis
    load.state_machine();
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_OVERVOLTAGE, load.state);

    port.voltage = port.sink_voltage_max - 0.6;
    load.state_machine();
    load.state_machine();
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.usb_state);
}

void off_short_circuit_to_disabled()
{
    PowerPort port;
    LoadOutput load(&port);
    port.init_load(14.6);
    port.pos_current_limit = 10;
    load.state = LOAD_STATE_OFF_SHORT_CIRCUIT;
    load.usb_state = LOAD_STATE_ON;

    load.state_machine();
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_SHORT_CIRCUIT, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.usb_state);       // not affected by overvoltage

    load.enable = false;        // this is like a manual reset
    load.state_machine();
    TEST_ASSERT_EQUAL(LOAD_STATE_DISABLED, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.usb_state);
}

void on_to_off_low_soc_if_error_flag_set()
{
    PowerPort port;
    LoadOutput load(&port);
    port.init_load(14.6);
    load.state = LOAD_STATE_ON;
    load.usb_state = LOAD_STATE_ON;
    port.pos_current_limit = 0;
    dev_stat.clear_error(ERR_ANY_ERROR); 
    dev_stat.set_error(ERR_BAT_UNDERVOLTAGE);

    load.state_machine();
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_LOW_SOC, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_LOW_SOC, load.usb_state);
}

void on_to_off_bat_temp_if_error_flag_set()
{
    PowerPort port;
    LoadOutput load(&port);
    port.init_load(14.6);
    port.pos_current_limit = 0;

    load.state = LOAD_STATE_ON;
    load.usb_state = LOAD_STATE_ON;
    dev_stat.clear_error(ERR_ANY_ERROR); 
    dev_stat.set_error(ERR_BAT_DIS_OVERTEMP);
    load.state_machine();
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_TEMPERATURE, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_TEMPERATURE, load.usb_state);

    load.state = LOAD_STATE_ON;
    dev_stat.clear_error(ERR_ANY_ERROR);
    dev_stat.set_error(ERR_BAT_DIS_UNDERTEMP);
    load.state_machine();
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_TEMPERATURE, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_TEMPERATURE, load.usb_state);
}

void on_to_disabled_if_enable_false()
{
    PowerPort port;
    LoadOutput load(&port);
    port.init_load(14.6);
    port.pos_current_limit = 10;

    load.state = LOAD_STATE_ON;
    load.usb_state = LOAD_STATE_ON;
    load.state_machine();
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.usb_state);

    load.enable = false;
    load.state_machine();
    TEST_ASSERT_EQUAL(LOAD_STATE_DISABLED, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.usb_state);

    load.usb_enable = false;
    load.state_machine();
    TEST_ASSERT_EQUAL(LOAD_STATE_DISABLED, load.usb_state);
}

void control_off_overvoltage()
{
    PowerPort port;
    LoadOutput load(&port);
    port.init_load(14.6);
    port.pos_current_limit = 10;
    port.voltage = 14.7;
    load.state = LOAD_STATE_ON;
    load.voltage_prev = port.voltage;
    dev_stat.clear_error(ERR_ANY_ERROR);

    // increase debounce counter to 1 before limit
    for (int i = 0; i < CONTROL_FREQUENCY; i++) {
        load.control();
    }

    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.state);
    load.control();     // once more
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_OVERVOLTAGE, load.state);
    TEST_ASSERT_EQUAL(ERR_LOAD_OVERVOLTAGE, dev_stat.error_flags);
}

void control_off_overcurrent()
{
    PowerPort port;
    LoadOutput load(&port);
    port.init_load(14.6);
    port.pos_current_limit = 10;            // this is currently not considered, as it is lower than hardware limit
    port.current = LOAD_CURRENT_MAX * 1.9;  // with factor 2 it is switched off immediately
    port.voltage = 14;
    load.state = LOAD_STATE_ON;
    load.voltage_prev = port.voltage;
    dev_stat.internal_temp = 25;
    load.junction_temperature = 25;
    dev_stat.clear_error(ERR_ANY_ERROR);

    load.control();
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.state);

    // almost 2x current = 4x heat generation: Should definitely trigger after waiting one time constant
    int trigger_steps = MOSFET_THERMAL_TIME_CONSTANT * CONTROL_FREQUENCY;
    for (int i = 0; i <= trigger_steps; i++) {
        load.control();
    }
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_OVERCURRENT, load.state);
    TEST_ASSERT_EQUAL(ERR_LOAD_OVERCURRENT, dev_stat.error_flags);
}

void control_off_voltage_dip()
{
    PowerPort port;
    LoadOutput load(&port);
    port.init_load(14.6);
    port.current = 0;
    load.state = LOAD_STATE_ON;
    load.junction_temperature = 25;
    dev_stat.internal_temp = 25;
    dev_stat.clear_error(ERR_ANY_ERROR);

    port.voltage = 14;
    load.voltage_prev = port.voltage;
    load.control();
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.state);

    port.voltage = 14 * 0.74;
    load.control();
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_OVERCURRENT, load.state);
    TEST_ASSERT_EQUAL(ERR_LOAD_VOLTAGE_DIP, dev_stat.error_flags);
}

void control_off_temperature()
{
    PowerPort port;
    LoadOutput load(&port);
    port.init_load(14.6);
    port.current = 0;
    load.voltage_prev = port.voltage;
    load.state = LOAD_STATE_ON;
    dev_stat.internal_temp = 25;
    load.junction_temperature = 25;
    dev_stat.clear_error(ERR_ANY_ERROR);
    dev_stat.set_error(ERR_INT_OVERTEMP);

    load.control();
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_TEMPERATURE, load.state);
}

void load_tests()
{
    UNITY_BEGIN();

    // state_machine tests
    RUN_TEST(disabled_to_on_if_everything_fine);
    RUN_TEST(disabled_to_off_low_soc_if_error_flag_set);
    RUN_TEST(disabled_to_off_bat_temp_if_error_flag_set);
    RUN_TEST(off_low_soc_to_on_after_delay);
    RUN_TEST(off_overcurrent_to_on_after_delay);
    RUN_TEST(off_overvoltage_to_on_at_lower_voltage);
    RUN_TEST(off_short_circuit_to_disabled);
    RUN_TEST(on_to_off_low_soc_if_error_flag_set);
    RUN_TEST(on_to_off_bat_temp_if_error_flag_set);
    RUN_TEST(on_to_disabled_if_enable_false);

    // control tests
    RUN_TEST(control_off_overvoltage);
    RUN_TEST(control_off_overcurrent);
    RUN_TEST(control_off_voltage_dip);
    RUN_TEST(control_off_temperature);

    // ToDo: What to do if port current is above the limit, but the hardware can still handle it?

    UNITY_END();
}
