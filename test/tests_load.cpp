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

static bool output_on;

static void load_drv_set(bool status)
{
    output_on = status;
}

static void load_drv_init()
{
    output_on = false;
}

static void load_init(LoadOutput *l, bool on = false, int num_batteries = 1)
{
    l->overvoltage = 14.6;
    l->current = 0;
    l->pos_current_limit = 10;
    l->bus->series_multiplier = num_batteries;
    l->bus->voltage = 14 * num_batteries;
    l->bus->sink_voltage_intercept = 14.4;
    l->bus->src_voltage_intercept = 12;
    l->bus->sink_current_margin = 10;
    l->bus->src_current_margin = -10;
    l->junction_temperature = 25;
    l->error_flags = 0;
    l->enable = true;

    if (on) {
        load_drv_set(true);
        l->state = LOAD_STATE_ON;

        // check if it stays on
        l->control();
        TEST_ASSERT_EQUAL(LOAD_STATE_ON, l->state);
    }
}

static void control_off_to_on_if_everything_fine()
{
    DcBus bus = {};
    LoadOutput load_out(&bus, &load_drv_set, &load_drv_init);
    load_init(&load_out);

    load_out.enable = true;
    load_out.control();
    TEST_ASSERT_EQUAL(0, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.state);
}

static void control_off_to_on_if_everything_fine_dual_battery()
{
    DcBus bus = {};
    LoadOutput load_out(&bus, &load_drv_set, &load_drv_init);
    load_init(&load_out, false, 2);

    load_out.enable = true;
    load_out.control();
    TEST_ASSERT_EQUAL(0, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.state);
}

static void control_on_to_off_shedding()
{
    DcBus bus = {};
    LoadOutput load_out(&bus, &load_drv_set, &load_drv_init);
    load_init(&load_out, true);

    bus.voltage = load_out.disconnect_voltage - 0.1;
    load_out.control();
    TEST_ASSERT_EQUAL(ERR_LOAD_SHEDDING, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF, load_out.state);
}

void control_on_to_off_overvoltage()
{
    DcBus bus = {};
    LoadOutput load_out(&bus, &load_drv_set, &load_drv_init);
    load_init(&load_out, true);

    bus.voltage = bus.sink_voltage_intercept + 0.6;

    // increase debounce counter to 1 before limit
    for (int i = 0; i < CONFIG_CONTROL_FREQUENCY; i++) {
        load_out.control();
    }

    TEST_ASSERT_EQUAL(0, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.state);

    load_out.control();     // once more
    TEST_ASSERT_EQUAL(ERR_LOAD_OVERVOLTAGE, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF, load_out.state);
}

void control_on_to_off_overvoltage_dual_battery()
{
    DcBus bus = {};
    LoadOutput load_out(&bus, &load_drv_set, &load_drv_init);
    load_init(&load_out, true, 2);

    bus.voltage = (bus.sink_voltage_intercept + 0.6) * bus.series_multiplier;

    // increase debounce counter to 1 before limit
    for (int i = 0; i < CONFIG_CONTROL_FREQUENCY; i++) {
        load_out.control();
    }

    TEST_ASSERT_EQUAL(0, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.state);

    load_out.control();     // once more
    TEST_ASSERT_EQUAL(ERR_LOAD_OVERVOLTAGE, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF, load_out.state);
}

void control_on_to_off_overcurrent()
{
    DcBus bus = {};
    LoadOutput load_out(&bus, &load_drv_set, &load_drv_init);
    load_init(&load_out, true);

    // current slightly below factor 2 so that it is not switched off immediately
    load_out.current = DT_PROP(DT_CHILD(DT_PATH(outputs), load), current_max) * 1.9;
    load_out.control();
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.state);

    // almost 2x current = 4x heat generation: Should definitely trigger after waiting one time constant
    int trigger_steps = DT_PROP(DT_PATH(pcb), mosfets_tau_ja) * CONFIG_CONTROL_FREQUENCY;
    for (int i = 0; i <= trigger_steps; i++) {
        load_out.control();
    }
    TEST_ASSERT_EQUAL(ERR_LOAD_OVERCURRENT, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF, load_out.state);
}

void control_on_to_off_voltage_dip()
{
    DcBus bus = {};
    LoadOutput load_out(&bus, &load_drv_set, &load_drv_init);
    load_init(&load_out, true);

    load_out.stop(ERR_LOAD_VOLTAGE_DIP);
    load_out.control();
    TEST_ASSERT_EQUAL(ERR_LOAD_VOLTAGE_DIP, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF, load_out.state);
}

void control_on_to_off_bus_limit()
{
    DcBus bus = {};
    LoadOutput load_out(&bus, &load_drv_set, &load_drv_init);
    load_init(&load_out, true);

    load_out.bus->src_current_margin = 0;
    load_out.control();
    TEST_ASSERT_EQUAL(ERR_LOAD_BUS_SRC_CURRENT, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF, load_out.state);
}

void control_on_to_off_if_enable_false()
{
    DcBus bus = {};
    LoadOutput load_out(&bus, &load_drv_set, &load_drv_init);
    load_init(&load_out, true);

    load_out.enable = false;
    load_out.control();
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF, load_out.state);
    TEST_ASSERT_EQUAL(0, load_out.error_flags);
}

void control_off_shedding_to_on_after_delay()
{
    DcBus bus = {};
    LoadOutput load_out(&bus, &load_drv_set, &load_drv_init);
    load_init(&load_out);
    load_out.error_flags = ERR_LOAD_SHEDDING;

    load_out.lvd_timestamp = time(NULL) - load_out.lvd_recovery_delay + 1;
    load_out.control();
    TEST_ASSERT_EQUAL(ERR_LOAD_SHEDDING, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF, load_out.state);

    load_out.lvd_timestamp = time(NULL) - load_out.lvd_recovery_delay - 1;
    load_out.control();
    TEST_ASSERT_EQUAL(0, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.state);
}

void control_off_overcurrent_to_on_after_delay()
{
    DcBus bus = {};
    LoadOutput load_out(&bus, &load_drv_set, &load_drv_init);
    load_init(&load_out);
    load_out.error_flags = ERR_LOAD_OVERCURRENT;

    load_out.oc_timestamp = time(NULL) - load_out.oc_recovery_delay + 1;
    load_out.control();
    TEST_ASSERT_EQUAL(ERR_LOAD_OVERCURRENT, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF, load_out.state);

    load_out.oc_timestamp = time(NULL) - load_out.oc_recovery_delay - 1;
    load_out.control();
    TEST_ASSERT_EQUAL(0, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.state);
}

void control_off_overvoltage_to_on_at_lower_voltage()
{
    DcBus bus = {};
    LoadOutput load_out(&bus, &load_drv_set, &load_drv_init);
    load_init(&load_out);
    bus.voltage = load_out.overvoltage + 0.1;
    load_out.error_flags = ERR_LOAD_OVERVOLTAGE;

    load_out.control();
    TEST_ASSERT_EQUAL(ERR_LOAD_OVERVOLTAGE, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF, load_out.state);

    bus.voltage = load_out.overvoltage - 0.1;     // test hysteresis
    load_out.control();
    TEST_ASSERT_EQUAL(ERR_LOAD_OVERVOLTAGE, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF, load_out.state);

    bus.voltage = load_out.overvoltage - load_out.ov_hysteresis - 0.1;
    load_out.control();
    TEST_ASSERT_EQUAL(0, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.state);
}

void control_off_overvoltage_to_on_at_lower_voltage_dual_battery()
{
    DcBus bus = {};
    LoadOutput load_out(&bus, &load_drv_set, &load_drv_init);
    load_init(&load_out, false, 2);
    bus.voltage = (load_out.overvoltage + 0.1) * bus.series_multiplier;
    load_out.error_flags = ERR_LOAD_OVERVOLTAGE;

    load_out.control();
    TEST_ASSERT_EQUAL(ERR_LOAD_OVERVOLTAGE, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF, load_out.state);

    bus.voltage = (load_out.overvoltage - 0.1) * bus.series_multiplier;     // test hysteresis
    load_out.control();
    TEST_ASSERT_EQUAL(ERR_LOAD_OVERVOLTAGE, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF, load_out.state);

    bus.voltage = (load_out.overvoltage - load_out.ov_hysteresis - 0.1) * bus.series_multiplier;
    load_out.control();
    TEST_ASSERT_EQUAL(0, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.state);
}

void control_off_short_circuit_flag_reset()
{
    DcBus bus = {};
    LoadOutput load_out(&bus, &load_drv_set, &load_drv_init);
    load_init(&load_out);
    load_out.error_flags = ERR_LOAD_SHORT_CIRCUIT;

    load_out.control();
    TEST_ASSERT_EQUAL(ERR_LOAD_SHORT_CIRCUIT, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF, load_out.state);

    load_out.enable = false;        // this is like a manual reset
    load_out.control();
    TEST_ASSERT_EQUAL(0, load_out.error_flags);
    TEST_ASSERT_EQUAL(LOAD_STATE_OFF, load_out.state);

    load_out.enable = true;
    load_out.control();
    TEST_ASSERT_EQUAL(LOAD_STATE_ON, load_out.state);
}

void load_tests()
{
    UNITY_BEGIN();

    // control tests
    RUN_TEST(control_off_to_on_if_everything_fine);
    RUN_TEST(control_off_to_on_if_everything_fine_dual_battery);
    RUN_TEST(control_on_to_off_shedding);
    RUN_TEST(control_on_to_off_overvoltage);
    RUN_TEST(control_on_to_off_overvoltage_dual_battery);
    RUN_TEST(control_on_to_off_overcurrent);
    RUN_TEST(control_on_to_off_voltage_dip);
    RUN_TEST(control_on_to_off_bus_limit);
    RUN_TEST(control_on_to_off_if_enable_false);

    RUN_TEST(control_off_shedding_to_on_after_delay);
    RUN_TEST(control_off_overcurrent_to_on_after_delay);
    RUN_TEST(control_off_overvoltage_to_on_at_lower_voltage);
    RUN_TEST(control_off_overvoltage_to_on_at_lower_voltage_dual_battery);
    RUN_TEST(control_off_short_circuit_flag_reset);

    // ToDo: What to do if port current is above the limit, but the hardware can still handle it?

    UNITY_END();
}
