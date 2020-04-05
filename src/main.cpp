/*
 * Copyright (c) 2019 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UNIT_TEST

#include <zephyr.h>
#include <stdio.h>
#include <device.h>
#include <drivers/gpio.h>

#include "thingset.h"           // handles access to internal data via communication interfaces
#include "board.h"              // hardware-specific settings
#include "setup.h"
#include "helper.h"

#include "half_bridge.h"        // PWM generation for DC/DC converter
#include "hardware.h"           // hardware-related functions like load switch, LED control, watchdog, etc.
#include "dcdc.h"               // DC/DC converter control (hardware independent)
#include "pwm_switch.h"         // PWM charge controller
#include "bat_charger.h"        // battery settings and charger state machine
#include "daq.h"                // ADC using DMA and conversion to measurement values
#include "ext/ext.h"            // communication interfaces, displays, etc. in UEXT connector
#include "eeprom.h"             // external I2C EEPROM
#include "load.h"               // load and USB output management
#include "leds.h"               // LED switching using charlieplexing
#include "device_status.h"      // log data (error memory, min/max measurements, etc.)
#include "data_objects.h"       // for access to internal data via ThingSet

void main(void)
{
    printf("Libre Solar Charge Controller: %s\n", CONFIG_BOARD);

    setup();

    battery_conf_init(&bat_conf, CONFIG_BAT_DEFAULT_TYPE,
        CONFIG_BAT_DEFAULT_NUM_CELLS, CONFIG_BAT_DEFAULT_CAPACITY_AH);
    battery_conf_overwrite(&bat_conf, &bat_conf_user);  // initialize conf_user with same values

    // read custom configuration from EEPROM
    data_objects_read_eeprom();

    // after each configuration change, data should be written back to EEPROM
    ts.set_conf_callback(data_objects_update_conf);
    ts.set_user_password(CONFIG_THINGSET_USER_PASSWORD);
    ts.set_maker_password(CONFIG_THINGSET_MAKER_PASSWORD);

    // Data Acquisition (DAQ) setup
    daq_setup();

    // initialize all extensions and external communication interfaces
    ext_mgr.enable_all();

    #if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    solar_terminal.init_solar();
    #endif

    #if CONFIG_HV_TERMINAL_NANOGRID
    grid_terminal.init_nanogrid();
    #endif

    charger.detect_num_batteries(&bat_conf);     // check if we have 24V instead of 12V system
    charger.init_terminal(&bat_conf);
    load.set_voltage_limits(bat_conf.voltage_load_disconnect, bat_conf.voltage_load_reconnect,
        bat_conf.voltage_absolute_max);

    #if CONFIG_HAS_USB_PWR_OUTPUT
    usb_pwr.set_voltage_limits(bat_conf.voltage_load_disconnect - 0.1, // keep on longer than load
        bat_conf.voltage_load_reconnect, bat_conf.voltage_absolute_max);
    #endif

    k_sleep(2000);      // safety feature: be able to re-flash before starting

    while (1) {
        charger.discharge_control(&bat_conf);
        charger.charge_control(&bat_conf);

        leds_update_1s();
        leds_update_soc(charger.soc, flags_check(&load.error_flags, ERR_LOAD_SHEDDING));

        eeprom_update();

        k_sleep(1000);
    }
}

void control_thread()
{
    uint32_t last_call = 0;
    while (1) {

        // convert ADC readings to meaningful measurement values
        daq_update();

        // alerts should trigger only for transients, so update based on actual voltage
        daq_set_lv_alerts(lv_terminal.bus->voltage * 1.2, lv_terminal.bus->voltage * 0.8);

        lv_terminal.update_bus_current_margins();

        #if DT_COMPAT_PWM_SWITCH
        //pwm_switch.control();
        leds_set_charging(pwm_switch.active());
        #endif

        #if DT_COMPAT_DCDC
        hv_terminal.update_bus_current_margins();
        dcdc.control();     // control of DC/DC including MPPT algorithm
        leds_set_charging(half_bridge_enabled());
        #endif

        #if CONFIG_HAS_LOAD_OUTPUT
        load.control();
        #endif

        #if CONFIG_HAS_USB_PWR_OUTPUT
        usb_pwr.control();
        #endif

        uint32_t now = k_uptime_get() / 1000;
        if (now > last_call) {
            last_call = now;

            // energy + soc calculation must be called exactly once per second
            #if DT_COMPAT_DCDC
            hv_terminal.energy_balance();
            #endif

            #if DT_COMPAT_PWM_SWITCH
            pwm_switch.energy_balance();
            #endif

            lv_terminal.energy_balance();
            load.energy_balance();
            dev_stat.update_energy();
            dev_stat.update_min_max_values();
            charger.update_soc(&bat_conf);

            #if CONFIG_HS_MOSFET_FAIL_SAFE_PROTECTION && DT_COMPAT_DCDC
            if (dev_stat.has_error(ERR_DCDC_HS_MOSFET_SHORT)) {
                dcdc.fuse_destruction();
            }
            #endif
        }
        k_sleep(100);
    }
}

void ext_mgr_thread()
{
    uint32_t last_call = 0;
    while (1) {
        uint32_t now = k_uptime_get() / 1000;
        ext_mgr.process_asap();     // approx. every millisecond
        if (now >= last_call + 1) {
            last_call = now;
            ext_mgr.process_1s();
        }
        k_sleep(1);
    }
}

K_THREAD_DEFINE(control_thread_id, 1024, control_thread, NULL, NULL, NULL, 2, 0, 2000);

K_THREAD_DEFINE(leds_thread, 256, leds_update_thread, NULL, NULL, NULL,	4, 0, K_NO_WAIT);

K_THREAD_DEFINE(ext_thread, 1024, ext_mgr_thread, NULL, NULL, NULL, 6, 0, 1000);

#endif // UNIT_TEST
