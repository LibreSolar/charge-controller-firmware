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

void load_init(LoadOutput *l)
{
    l->overvoltage = 14.6;
    l->port->current = 0;
    l->port->pos_current_limit = 10;
    l->port->bus->voltage = 14;
    l->port->bus->sink_voltage_bound = 14.4;
    l->port->bus->src_voltage_bound = 12;
    l->port->bus->sink_current_margin = 10;
    l->port->bus->src_current_margin = -10;
    l->junction_temperature = 25;
}

void control_off_to_pgood_if_everything_fine()
{
    DcBus bus = {};
    PowerPort port(&bus);
    LoadOutput load_out(&port);
    load_init(&load_out);
    dev_stat.clear_error(ERR_ANY_ERROR);

    load_out.control();
    TEST_ASSERT_EQUAL(0, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(true, load_out.pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.state);       // deprecated
}

void control_pgood_to_off_overvoltage()
{
    DcBus bus = {};
    PowerPort port(&bus);
    LoadOutput load_out(&port);
    load_init(&load_out);
    load_out.state = LOAD_STATE_ON;
    dev_stat.clear_error(ERR_ANY_ERROR);
    port.bus->voltage = port.bus->sink_voltage_bound + 0.6;

    // increase debounce counter to 1 before limit
    for (int i = 0; i < CONTROL_FREQUENCY; i++) {
        load_out.control();
    }

    TEST_ASSERT_EQUAL(true, load_out.pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.state);                   // deprecated

    load_out.control();     // once more
    TEST_ASSERT_EQUAL(false, load_out.pgood);
    TEST_ASSERT_EQUAL(ERR_LOAD_OVERVOLTAGE, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_OVERVOLTAGE, load_out.state);      // deprecated
}

void control_pgood_to_off_overcurrent()
{
    DcBus bus = {};
    PowerPort port(&bus);
    LoadOutput load_out(&port);
    load_init(&load_out);
    port.current = LOAD_CURRENT_MAX * 1.9;  // with factor 2 it is switched off immediately
    load_out.state = LOAD_STATE_ON;
    dev_stat.internal_temp = 25;
    dev_stat.clear_error(ERR_ANY_ERROR);

    load_out.control();
    TEST_ASSERT_EQUAL(true, load_out.pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.state);                   // deprecated

    // almost 2x current = 4x heat generation: Should definitely trigger after waiting one time constant
    int trigger_steps = MOSFET_THERMAL_TIME_CONSTANT * CONTROL_FREQUENCY;
    for (int i = 0; i <= trigger_steps; i++) {
        load_out.control();
    }
    TEST_ASSERT_EQUAL(false, load_out.pgood);
    TEST_ASSERT_EQUAL(ERR_LOAD_OVERCURRENT, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_OVERCURRENT, load_out.state);      // deprecated
}

void control_pgood_to_off_voltage_dip()
{
    DcBus bus = {};
    PowerPort port(&bus);
    LoadOutput load_out(&port);
    load_init(&load_out);
    load_out.state = LOAD_STATE_ON;
    dev_stat.internal_temp = 25;
    dev_stat.clear_error(ERR_ANY_ERROR);

    load_out.control();
    TEST_ASSERT_EQUAL(true, load_out.pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.state);                   // deprecated

    load_out.stop(ERR_LOAD_VOLTAGE_DIP);
    load_out.control();
    TEST_ASSERT_EQUAL(false, load_out.pgood);
    TEST_ASSERT_EQUAL(ERR_LOAD_VOLTAGE_DIP, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_OVERCURRENT, load_out.state);      // deprecated
}

void control_pgood_to_off_int_temp()
{
    DcBus bus = {};
    PowerPort port(&bus);
    LoadOutput load_out(&port);
    load_init(&load_out);
    load_out.state = LOAD_STATE_ON;
    dev_stat.internal_temp = 25;
    dev_stat.clear_error(ERR_ANY_ERROR);

    load_out.control();
    TEST_ASSERT_EQUAL(true, load_out.pgood);
    dev_stat.set_error(ERR_INT_OVERTEMP);
    load_out.control();
    TEST_ASSERT_EQUAL(false, load_out.pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_TEMPERATURE, load_out.state);      // deprecated
}

void control_pgood_to_off_bat_temp()
{
    DcBus bus = {};
    PowerPort port(&bus);
    LoadOutput load_out(&port);
    load_init(&load_out);

    // overtemp
    load_out.state = LOAD_STATE_ON;
    load_out.usb_state = LOAD_STATE_ON;
    dev_stat.clear_error(ERR_ANY_ERROR);
    dev_stat.set_error(ERR_BAT_DIS_OVERTEMP);
    load_out.control();
    TEST_ASSERT_EQUAL(false, load_out.pgood);
    TEST_ASSERT_EQUAL(false, load_out.usb_pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_TEMPERATURE, load_out.state);      // deprecated
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_TEMPERATURE, load_out.usb_state);  // deprecated

    // undertemp
    load_out.state = LOAD_STATE_ON;
    load_out.usb_state = LOAD_STATE_ON;
    dev_stat.clear_error(ERR_ANY_ERROR);
    dev_stat.set_error(ERR_BAT_DIS_UNDERTEMP);
    load_out.control();
    TEST_ASSERT_EQUAL(false, load_out.pgood);
    TEST_ASSERT_EQUAL(false, load_out.usb_pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_TEMPERATURE, load_out.state);      // deprecated
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_TEMPERATURE, load_out.usb_state);  // deprecated
}

void control_pgood_to_off_low_soc()
{
    DcBus bus = {};
    PowerPort port(&bus);
    LoadOutput load_out(&port);
    load_init(&load_out);

    dev_stat.clear_error(ERR_ANY_ERROR);
    load_out.state = LOAD_STATE_ON;
    load_out.control();
    TEST_ASSERT_EQUAL(true, load_out.pgood);

    dev_stat.set_error(ERR_BAT_UNDERVOLTAGE);
    load_out.control();
    TEST_ASSERT_EQUAL(false, load_out.pgood);
    TEST_ASSERT_EQUAL(ERR_LOAD_LOW_SOC | ERR_BAT_UNDERVOLTAGE, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_LOW_SOC, load_out.state);          // deprecated
}

void control_pgood_to_off_if_enable_false()
{
    DcBus bus = {};
    PowerPort port(&bus);
    LoadOutput load_out(&port);
    load_init(&load_out);
    dev_stat.clear_error(ERR_ANY_ERROR);

    load_out.state = LOAD_STATE_ON;
    load_out.control();
    TEST_ASSERT_EQUAL(true, load_out.pgood);
    TEST_ASSERT_EQUAL(true, load_out.usb_pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.usb_state);

    load_out.enable = false;
    load_out.control();
    TEST_ASSERT_EQUAL(false, load_out.pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_DISABLED, load_out.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.usb_state);

    load_out.usb_enable = false;
    load_out.control();
    TEST_ASSERT_EQUAL(false, load_out.usb_pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_DISABLED, load_out.usb_state);
}

void control_off_low_soc_to_on_after_delay()
{
    DcBus bus = {};
    PowerPort port(&bus);
    LoadOutput load_out(&port);
    load_init(&load_out);
    load_out.state = LOAD_STATE_OFF_LOW_SOC;
    load_out.usb_state = LOAD_STATE_OFF_LOW_SOC;
    dev_stat.error_flags = ERR_LOAD_LOW_SOC;

    load_out.lvd_timestamp = time(NULL) - load_out.lvd_recovery_delay + 1;
    load_out.control();
    TEST_ASSERT_EQUAL(ERR_LOAD_LOW_SOC, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_LOW_SOC, load_out.state);          // deprecated
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_LOW_SOC, load_out.usb_state);      // deprecated

    load_out.lvd_timestamp = time(NULL) - load_out.lvd_recovery_delay - 1;
    load_out.control();
    TEST_ASSERT_EQUAL(0, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(true, load_out.pgood);
    TEST_ASSERT_EQUAL(true, load_out.usb_pgood);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.state);                   // deprecated
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.usb_state);               // deprecated
}

void control_off_overcurrent_to_on_after_delay()
{
    DcBus bus = {};
    PowerPort port(&bus);
    LoadOutput load_out(&port);
    load_init(&load_out);
    load_out.state = LOAD_STATE_OFF_OVERCURRENT;
    load_out.usb_state = LOAD_STATE_ON;
    dev_stat.error_flags = ERR_LOAD_OVERCURRENT;

    load_out.oc_timestamp = time(NULL) - load_out.oc_recovery_delay + 1;
    load_out.control();
    TEST_ASSERT_EQUAL(ERR_LOAD_OVERCURRENT, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_OVERCURRENT, load_out.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.usb_state);       // not affected by overcurrent

    load_out.oc_timestamp = time(NULL) - load_out.oc_recovery_delay - 1;
    load_out.control();
    TEST_ASSERT_EQUAL(0, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.state);       // deprecated
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.usb_state);   // deprecated
}

void control_off_overvoltage_to_on_at_lower_voltage()
{
    DcBus bus = {};
    PowerPort port(&bus);
    LoadOutput load_out(&port);
    load_init(&load_out);
    load_out.state = LOAD_STATE_OFF_OVERVOLTAGE;
    load_out.usb_state = LOAD_STATE_ON;
    port.bus->voltage = port.bus->sink_voltage_bound + 0.1;
    dev_stat.error_flags = ERR_LOAD_OVERVOLTAGE;

    load_out.control();
    TEST_ASSERT_EQUAL(ERR_LOAD_OVERVOLTAGE, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_OVERVOLTAGE, load_out.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.usb_state);       // not affected by overvoltage

    port.bus->voltage = port.bus->sink_voltage_bound - 0.1;     // test hysteresis
    load_out.control();
    TEST_ASSERT_EQUAL(ERR_LOAD_OVERVOLTAGE, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_OVERVOLTAGE, load_out.state);

    port.bus->voltage = port.bus->sink_voltage_bound - load_out.ov_hysteresis - 0.1;
    load_out.control();
    TEST_ASSERT_EQUAL(0, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.state);       // deprecated
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.usb_state);   // deprecated
}

void control_off_short_circuit_flag_reset()
{
    DcBus bus = {};
    PowerPort port(&bus);
    LoadOutput load_out(&port);
    load_init(&load_out);
    load_out.state = LOAD_STATE_OFF_SHORT_CIRCUIT;
    load_out.usb_state = LOAD_STATE_ON;
    dev_stat.error_flags = ERR_LOAD_SHORT_CIRCUIT;

    load_out.control();
    TEST_ASSERT_EQUAL(ERR_LOAD_SHORT_CIRCUIT, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF_SHORT_CIRCUIT, load_out.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.usb_state);       // not affected by overvoltage

    load_out.enable = false;        // this is like a manual reset
    load_out.control();
    TEST_ASSERT_EQUAL(0, dev_stat.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_DISABLED, load_out.state);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.usb_state);
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
