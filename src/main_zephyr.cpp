/*
 * Copyright (c) 2019 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __ZEPHYR__

#include <zephyr.h>
#include <stdio.h>
#include <device.h>
#include <drivers/gpio.h>

#include "thingset.h"           // handles access to internal data via communication interfaces
#include "pcb.h"                // hardware-specific settings
#include "config.h"             // user-specific configuration

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

DcBus lv_bus;
PowerPort lv_terminal(&lv_bus, true);   // low voltage terminal (battery for typical MPPT)

#if CONFIG_HAS_DCDC_CONVERTER
DcBus hv_bus;
PowerPort hv_terminal(&hv_bus, true);   // high voltage terminal (solar for typical MPPT)
PowerPort dcdc_lv_port(&lv_bus);        // internal low voltage side of DC/DC converter
#if CONFIG_HV_TERMINAL_NANOGRID
Dcdc dcdc(&hv_terminal, &dcdc_lv_port, MODE_NANOGRID);
#elif CONFIG_HV_TERMINAL_BATTERY
Dcdc dcdc(&hv_terminal, &dcdc_lv_port, MODE_MPPT_BOOST);
#else
Dcdc dcdc(&hv_terminal, &dcdc_lv_port, MODE_MPPT_BUCK);
#endif // CONFIG_HV_TERMINAL
#endif

#if CONFIG_HAS_PWM_SWITCH
PwmSwitch pwm_switch(&lv_bus);
#endif

#if CONFIG_HAS_LOAD_OUTPUT
PowerPort load_terminal(&lv_bus);        // load terminal (also connected to lv_bus)
LoadOutput load(&load_terminal);
#endif

#if CONFIG_HV_TERMINAL_SOLAR
PowerPort &solar_terminal = hv_terminal;
#elif CONFIG_LV_TERMINAL_SOLAR
PowerPort &solar_terminal = lv_terminal;
#elif CONFIG_PWM_TERMINAL_SOLAR
PowerPort &solar_terminal = pwm_switch;
#endif

#if CONFIG_HV_TERMINAL_NANOGRID
PowerPort &grid_terminal = hv_terminal;
#endif

#if CONFIG_LV_TERMINAL_BATTERY
PowerPort &bat_terminal = lv_terminal;
#elif CONFIG_HV_TERMINAL_BATTERY
PowerPort &bat_terminal = hv_terminal;
#endif

Charger charger(&bat_terminal);

BatConf bat_conf;               // actual (used) battery configuration
BatConf bat_conf_user;          // temporary storage where the user can write to

DeviceStatus dev_stat;

extern ThingSet ts;             // defined in data_objects.cpp

time_t timestamp;    // current unix timestamp (independent of time(NULL), as it is user-configurable)

void main(void)
{
    u32_t cnt = 0;

    printf("Booting Libre Solar Charge Controller: %s\n", CONFIG_BOARD);

    battery_conf_init(&bat_conf,
        BAT_TYPE_GEL,                   // temporary!!
        CONFIG_BAT_DEFAULT_NUM_CELLS,
        CONFIG_BAT_DEFAULT_CAPACITY_AH);
    battery_conf_overwrite(&bat_conf, &bat_conf_user);  // initialize conf_user with same values

    // Configuration from EEPROM
    data_objects_read_eeprom();
    ts.set_conf_callback(data_objects_update_conf);     // after each configuration change, data should be written back to EEPROM
    //ts.set_user_password(THINGSET_USER_PASSWORD);       // passwords defined in config.h (see template)
    //ts.set_maker_password(THINGSET_MAKER_PASSWORD);

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
    battery_init_terminal(&bat_terminal, &bat_conf, charger.num_batteries);
    battery_init_load(&load, &bat_conf, charger.num_batteries);

    k_sleep(2000);      // safety feature: be able to re-flash before starting

    while (1) {
        leds_update_1s();
        leds_update_soc(charger.soc, dev_stat.has_error(ERR_LOAD_LOW_SOC));

        eeprom_update();

        cnt++;
        k_sleep(1000);
    }
}


void control_thread()
{
    int counter = 0;
    while (1) {

        // convert ADC readings to meaningful measurement values
        daq_update();

        // alerts should trigger only for transients, so update based on actual voltage
        daq_set_lv_alerts(lv_terminal.bus->voltage * 1.2, lv_terminal.bus->voltage * 0.8);

        lv_terminal.update_bus_current_margins();

        #if CONFIG_HAS_PWM_SWITCH
        //pwm_switch.control();
        leds_set_charging(pwm_switch.active());
        #endif

        #if CONFIG_HAS_DCDC_CONVERTER
        hv_terminal.update_bus_current_margins();
        dcdc.control();     // control of DC/DC including MPPT algorithm
        leds_set_charging(half_bridge_enabled());
        #endif

        load.control();

        if (counter % CONTROL_FREQUENCY == 0) {
            // called once per second (this timer is much more accurate than time(NULL) based on LSI)
            // see also here: https://github.com/ARMmbed/mbed-os/issues/9065
            timestamp++;
            counter = 0;

            // energy + soc calculation must be called exactly once per second
            #if CONFIG_HAS_DCDC_CONVERTER
            hv_terminal.energy_balance();
            #endif

            #if CONFIG_HAS_PWM_SWITCH
            pwm_switch.energy_balance();
            #endif

            lv_terminal.energy_balance();
            load_terminal.energy_balance();
            dev_stat.update_energy();
            dev_stat.update_min_max_values();
            charger.update_soc(&bat_conf);
        }

        counter++;
        k_sleep(100);
    }
}

void ext_mgr_thread()
{
    u32_t cnt = 0;
    uint32_t last_call = 0;
    while (1) {
        uint32_t now = k_uptime_get() / 1000;
        ext_mgr.process_asap();     // approx. every millisecond
        if (now >= last_call + 1) {
            last_call = now;
            ext_mgr.process_1s();
        }
        cnt++;
        k_sleep(1);
    }
}

K_THREAD_DEFINE(control_thread_id, 1024, control_thread, NULL, NULL, NULL, 2, 0, 2000);

K_THREAD_DEFINE(leds_thread, 256, leds_update_thread, NULL, NULL, NULL,	4, 0, K_NO_WAIT);

K_THREAD_DEFINE(ext_thread, 1024, ext_mgr_thread, NULL, NULL, NULL, 6, 0, 1000);

#endif // __ZEPHYR__
