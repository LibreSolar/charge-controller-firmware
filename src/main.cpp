/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UNIT_TEST

#include <zephyr.h>
#include <stdio.h>
#include <device.h>
#include <drivers/gpio.h>
#include <task_wdt/task_wdt.h>

#include "thingset.h"           // handles access to internal data via communication interfaces
#include "setup.h"
#include "helper.h"

#include "half_bridge.h"        // PWM generation for DC/DC converter
#include "hardware.h"           // hardware-related functions like load switch, LED control, watchdog, etc.
#include "dcdc.h"               // DC/DC converter control (hardware independent)
#include "pwm_switch.h"         // PWM charge controller
#include "bat_charger.h"        // battery settings and charger state machine
#include "daq.h"                // ADC using DMA and conversion to measurement values
#include "data_storage.h"       // non-volatile data storage (e.g. EEPROM)
#include "load.h"               // load and USB output management
#include "leds.h"               // LED switching using charlieplexing
#include "device_status.h"      // log data (error memory, min/max measurements, etc.)
#include "data_nodes.h"         // for access to internal data via ThingSet

void main(void)
{
    printf("Libre Solar Charge Controller: %s\n", CONFIG_BOARD);

    task_wdt_init(device_get_binding(DT_LABEL(DT_NODELABEL(iwdg))));

    setup();

    battery_conf_init(&bat_conf, CONFIG_BAT_TYPE, CONFIG_BAT_NUM_CELLS, CONFIG_BAT_CAPACITY_AH);
    battery_conf_overwrite(&bat_conf, &bat_conf_user);  // initialize conf_user with same values

    #if BOARD_HAS_DCDC
    daq_set_hv_limit(DT_PROP(DT_PATH(pcb), hs_voltage_max));
    #endif

    #if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    solar_terminal.init_solar();
    #endif

    #if CONFIG_HV_TERMINAL_NANOGRID
    grid_terminal.init_nanogrid();
    #endif

    // read custom configuration from EEPROM
    data_nodes_init();

    // Data Acquisition (DAQ) setup
    daq_setup();

    charger.detect_num_batteries(&bat_conf);     // check if we have 24V instead of 12V system
    charger.init_terminal(&bat_conf);

    #if BOARD_HAS_LOAD_OUTPUT
    load.set_voltage_limits(bat_conf.voltage_load_disconnect, bat_conf.voltage_load_reconnect,
        bat_conf.voltage_absolute_max);
    #endif

    #if BOARD_HAS_USB_OUTPUT
    usb_pwr.set_voltage_limits(bat_conf.voltage_load_disconnect - 0.1, // keep on longer than load
        bat_conf.voltage_load_reconnect, bat_conf.voltage_absolute_max);
    #endif

    // wait until all threads are spawned before activating the watchdog
    k_sleep(K_MSEC(2500));

    int64_t t_start = k_uptime_get();

    while (true) {
        // loop runs exactly once per second and includes slow control tasks and energy calculation

        charger.discharge_control(&bat_conf);
        charger.charge_control(&bat_conf);

        // energy + soc calculation must be called exactly once per second
        #if BOARD_HAS_DCDC
        if  (dcdc.state != DCDC_CONTROL_OFF) {
            hv_terminal.energy_balance();
        }
        #endif

        #if BOARD_HAS_PWM_PORT
        if (pwm_switch.active() == 1) {
            pwm_switch.energy_balance();
        }
        #endif

        lv_terminal.energy_balance();

        #if BOARD_HAS_LOAD_OUTPUT
        if (load.state == 1) {
            load.energy_balance();
        }
        #endif

        dev_stat.update_energy();
        dev_stat.update_min_max_values();
        charger.update_soc(&bat_conf);

        #if CONFIG_HS_MOSFET_FAIL_SAFE_PROTECTION && BOARD_HAS_DCDC
        if (dev_stat.has_error(ERR_DCDC_HS_MOSFET_SHORT)) {
            dcdc.fuse_destruction();
        }
        #endif

        leds_update_1s();

        #if BOARD_HAS_LOAD_OUTPUT
        leds_update_soc(charger.soc, flags_check(&load.error_flags, ERR_LOAD_SHEDDING));
        #else
        leds_update_soc(charger.soc, false);
        #endif

        data_storage_update();

        t_start += 1000;
        k_sleep(K_TIMEOUT_ABS_MS(t_start));
    }
}

void control_thread()
{
    int wdt_channel = task_wdt_add(200, task_wdt_callback, (void *)k_current_get());

    while (true) {
        // control loop runs at approx. 10 Hz

        bool charging = false;

        task_wdt_feed(wdt_channel);

        // convert ADC readings to meaningful measurement values
        daq_update();

        // alerts should trigger only for transients, so update based on actual voltage
        daq_set_lv_limits(lv_terminal.bus->voltage * 1.2F, lv_terminal.bus->voltage * 0.8F);

        lv_terminal.update_bus_current_margins();

        #if BOARD_HAS_PWM_PORT
        pwm_switch.control();
        charging |= pwm_switch.active();
        #endif

        #if BOARD_HAS_DCDC
        hv_terminal.update_bus_current_margins();
        dcdc.control();     // control of DC/DC including MPPT algorithm
        charging |= half_bridge_enabled();
        #endif

        leds_set_charging(charging);

        #if BOARD_HAS_LOAD_OUTPUT
        load.control();
        #endif

        #if BOARD_HAS_USB_OUTPUT
        usb_pwr.control();
        #endif

        k_sleep(K_MSEC(100));
    }
}

// 2s delay for control thread as a safety feature: be able to re-flash before starting
// priority is set to -1 (=cooperative) to have higher priority than other threads incl. main
K_THREAD_DEFINE(control_thread_id, 1024, control_thread, NULL, NULL, NULL, -1, 0, 2000);

#endif // UNIT_TEST
