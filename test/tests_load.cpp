/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tests.h"
#include "adc_dma_stub.h"
#include "adc_dma.h"

#include <time.h>
#include <stdio.h>
#include <math.h>

#include "main.h"

void load_init(LoadOutput *l)
{
    l->port->init_load(14.6);
    l->port->pos_current_limit = 10;
    l->port->voltage = 14;
    l->junction_temperature = 25;
}

void control_off_to_pgood_if_everything_fine()
{
    PowerPort port = {};
    LoadOutput load(&port);
    load_init(&load);
    dev_stat.clear_error(ERR_ANY_ERROR);

    load.control();
    TEST_ASSERT_EQUAL(0, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(true, load.pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.state);       // deprecated
}

void control_pgood_to_off_overvoltage()
{
    PowerPort port = {};
    LoadOutput load(&port);
    load_init(&load);
    load.state = LOAD_STATE_ON;
    dev_stat.clear_error(ERR_ANY_ERROR);
    port.voltage = port.sink_voltage_max + 0.6;

    // increase debounce counter to 1 before limit
    for (int i = 0; i < CONTROL_FREQUENCY; i++) {
        load.control();
    }

    TEST_ASSERT_EQUAL(true, load.pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.state);                   // deprecated

    load.control();     // once more
    TEST_ASSERT_EQUAL(false, load.pgood);
    TEST_ASSERT_EQUAL(ERR_LOAD_OVERVOLTAGE, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_OVERVOLTAGE, load.state);      // deprecated
}

void control_pgood_to_off_overcurrent()
{
    PowerPort port = {};
    LoadOutput load(&port);
    load_init(&load);
    port.current = LOAD_CURRENT_MAX * 1.9;  // with factor 2 it is switched off immediately
    load.state = LOAD_STATE_ON;
    dev_stat.internal_temp = 25;
    dev_stat.clear_error(ERR_ANY_ERROR);

    load.control();
    TEST_ASSERT_EQUAL(true, load.pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.state);                   // deprecated

    // almost 2x current = 4x heat generation: Should definitely trigger after waiting one time constant
    int trigger_steps = MOSFET_THERMAL_TIME_CONSTANT * CONTROL_FREQUENCY;
    for (int i = 0; i <= trigger_steps; i++) {
        load.control();
    }
    TEST_ASSERT_EQUAL(false, load.pgood);
    TEST_ASSERT_EQUAL(ERR_LOAD_OVERCURRENT, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_OVERCURRENT, load.state);      // deprecated
}

void control_pgood_to_off_voltage_dip()
{
    PowerPort port = {};
    LoadOutput load(&port);
    load_init(&load);
    load.state = LOAD_STATE_ON;
    dev_stat.internal_temp = 25;
    dev_stat.clear_error(ERR_ANY_ERROR);

    load.control();
    TEST_ASSERT_EQUAL(true, load.pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.state);                   // deprecated

    load.stop(ERR_LOAD_VOLTAGE_DIP);
    load.control();
    TEST_ASSERT_EQUAL(false, load.pgood);
    TEST_ASSERT_EQUAL(ERR_LOAD_VOLTAGE_DIP, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_OVERCURRENT, load.state);      // deprecated
}

void control_pgood_to_off_int_temp()
{
    PowerPort port = {};
    LoadOutput load(&port);
    load_init(&load);
    load.state = LOAD_STATE_ON;
    dev_stat.internal_temp = 25;
    dev_stat.clear_error(ERR_ANY_ERROR);

    load.control();
    TEST_ASSERT_EQUAL(true, load.pgood);
    dev_stat.set_error(ERR_INT_OVERTEMP);
    load.control();
    TEST_ASSERT_EQUAL(false, load.pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_TEMPERATURE, load.state);      // deprecated
}

void control_pgood_to_off_bat_temp()
{
    PowerPort port = {};
    LoadOutput load(&port);
    load_init(&load);

    // overtemp
    load.state = LOAD_STATE_ON;
    load.usb_state = LOAD_STATE_ON;
    dev_stat.clear_error(ERR_ANY_ERROR);
    dev_stat.set_error(ERR_BAT_DIS_OVERTEMP);
    load.control();
    TEST_ASSERT_EQUAL(false, load.pgood);
    TEST_ASSERT_EQUAL(false, load.usb_pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_TEMPERATURE, load.state);      // deprecated
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_TEMPERATURE, load.usb_state);  // deprecated

    // undertemp
    load.state = LOAD_STATE_ON;
    load.usb_state = LOAD_STATE_ON;
    dev_stat.clear_error(ERR_ANY_ERROR);
    dev_stat.set_error(ERR_BAT_DIS_UNDERTEMP);
    load.control();
    TEST_ASSERT_EQUAL(false, load.pgood);
    TEST_ASSERT_EQUAL(false, load.usb_pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_TEMPERATURE, load.state);      // deprecated
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_TEMPERATURE, load.usb_state);  // deprecated
}

void control_pgood_to_off_low_soc()
{
    PowerPort port = {};
    LoadOutput load(&port);
    load_init(&load);

    dev_stat.clear_error(ERR_ANY_ERROR);
    load.state = LOAD_STATE_ON;
    load.control();
    TEST_ASSERT_EQUAL(true, load.pgood);

    dev_stat.set_error(ERR_BAT_UNDERVOLTAGE);
    load.control();
    TEST_ASSERT_EQUAL(false, load.pgood);
    TEST_ASSERT_EQUAL(ERR_LOAD_LOW_SOC | ERR_BAT_UNDERVOLTAGE, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_LOW_SOC, load.state);          // deprecated
}

void control_pgood_to_off_if_enable_false()
{
    PowerPort port = {};
    LoadOutput load(&port);
    load_init(&load);
    dev_stat.clear_error(ERR_ANY_ERROR);

    load.state = LOAD_STATE_ON;
    load.control();
    TEST_ASSERT_EQUAL(true, load.pgood);
    TEST_ASSERT_EQUAL(true, load.usb_pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.usb_state);

    load.enable = false;
    load.control();
    TEST_ASSERT_EQUAL(false, load.pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_DISABLED, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.usb_state);

    load.usb_enable = false;
    load.control();
    TEST_ASSERT_EQUAL(false, load.usb_pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_DISABLED, load.usb_state);
}

void control_off_low_soc_to_on_after_delay()
{
    PowerPort port = {};
    LoadOutput load(&port);
    load_init(&load);
    load.state = LOAD_STATE_OFF_LOW_SOC;
    load.usb_state = LOAD_STATE_OFF_LOW_SOC;
    dev_stat.error_flags = ERR_LOAD_LOW_SOC;

    load.lvd_timestamp = time(NULL) - load.lvd_recovery_delay + 1;
    load.control();
    TEST_ASSERT_EQUAL(ERR_LOAD_LOW_SOC, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_LOW_SOC, load.state);          // deprecated
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_LOW_SOC, load.usb_state);      // deprecated

    load.lvd_timestamp = time(NULL) - load.lvd_recovery_delay - 1;
    load.control();
    TEST_ASSERT_EQUAL(0, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(true, load.pgood);
    TEST_ASSERT_EQUAL(true, load.usb_pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.state);                   // deprecated
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.usb_state);               // deprecated
}

void control_off_overcurrent_to_on_after_delay()
{
    PowerPort port = {};
    LoadOutput load(&port);
    load_init(&load);
    load.state = LOAD_STATE_OFF_OVERCURRENT;
    load.usb_state = LOAD_STATE_ON;
    dev_stat.error_flags = ERR_LOAD_OVERCURRENT;

    load.oc_timestamp = time(NULL) - load.oc_recovery_delay + 1;
    load.control();
    TEST_ASSERT_EQUAL(ERR_LOAD_OVERCURRENT, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_OVERCURRENT, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.usb_state);       // not affected by overcurrent

    load.oc_timestamp = time(NULL) - load.oc_recovery_delay - 1;
    load.control();
    TEST_ASSERT_EQUAL(0, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.state);       // deprecated
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.usb_state);   // deprecated
}

void control_off_overvoltage_to_on_at_lower_voltage()
{
    PowerPort port = {};
    LoadOutput load(&port);
    load_init(&load);
    load.state = LOAD_STATE_OFF_OVERVOLTAGE;
    load.usb_state = LOAD_STATE_ON;
    port.voltage = port.sink_voltage_max + 0.1;
    dev_stat.error_flags = ERR_LOAD_OVERVOLTAGE;

    load.control();
    TEST_ASSERT_EQUAL(ERR_LOAD_OVERVOLTAGE, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_OVERVOLTAGE, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.usb_state);       // not affected by overvoltage

    port.voltage = port.sink_voltage_max - 0.1;     // test hysteresis
    load.control();
    TEST_ASSERT_EQUAL(ERR_LOAD_OVERVOLTAGE, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_OVERVOLTAGE, load.state);

    port.voltage = port.sink_voltage_max - load.ov_hysteresis - 0.1;
    load.control();
    TEST_ASSERT_EQUAL(0, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.state);       // deprecated
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.usb_state);   // deprecated
}

void control_off_short_circuit_flag_reset()
{
    PowerPort port = {};
    LoadOutput load(&port);
    load_init(&load);
    load.state = LOAD_STATE_OFF_SHORT_CIRCUIT;
    load.usb_state = LOAD_STATE_ON;
    dev_stat.error_flags = ERR_LOAD_SHORT_CIRCUIT;

    load.control();
    TEST_ASSERT_EQUAL(ERR_LOAD_SHORT_CIRCUIT, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_SHORT_CIRCUIT, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.usb_state);       // not affected by overvoltage

    load.enable = false;        // this is like a manual reset
    load.control();
    TEST_ASSERT_EQUAL(0, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_DISABLED, load.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load.usb_state);
}

void load_tests()
{
    UNITY_BEGIN();

    // control tests
    RUN_TEST(control_off_to_pgood_if_everything_fine);
    RUN_TEST(control_pgood_to_off_overvoltage);
    RUN_TEST(control_pgood_to_off_overcurrent);
    RUN_TEST(control_pgood_to_off_voltage_dip);
    RUN_TEST(control_pgood_to_off_int_temp);
    RUN_TEST(control_pgood_to_off_bat_temp);
    RUN_TEST(control_pgood_to_off_low_soc);
    RUN_TEST(control_pgood_to_off_if_enable_false);

    RUN_TEST(control_off_low_soc_to_on_after_delay);
    RUN_TEST(control_off_overcurrent_to_on_after_delay);
    RUN_TEST(control_off_overvoltage_to_on_at_lower_voltage);
    RUN_TEST(control_off_short_circuit_flag_reset);

    // ToDo: What to do if port current is above the limit, but the hardware can still handle it?

    UNITY_END();
}
