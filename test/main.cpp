/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "thingset.h"           // handles access to internal data via communication interfaces
#include "pcb.h"                // hardware-specific settings
#include "config.h"             // user-specific configuration

#include "half_bridge.h"        // PWM generation for DC/DC converter
#include "hardware.h"           // hardware-related functions like load switch, LED control, watchdog, etc.
#include "dcdc.h"               // DC/DC converter control (hardware independent)
#include "pwm_switch.h"         // PWM charge controller
#include "bat_charger.h"        // battery settings and charger state machine
#include "daq.h"            // ADC using DMA and conversion to measurement values
#include "eeprom.h"             // external I2C EEPROM
#include "load.h"               // load and USB output management
#include "leds.h"               // LED switching using charlieplexing
#include "device_status.h"      // device-level data (error memory, min/max measurements, etc.)
#include "data_objects.h"       // for access to internal data via ThingSet

#include "tests.h"

DcBus lv_bus;
PowerPort lv_terminal(&lv_bus);          // low voltage terminal (battery for typical MPPT)

#if CONFIG_HAS_DCDC_CONVERTER
DcBus hv_bus;
PowerPort hv_terminal(&hv_bus);          // high voltage terminal (solar for typical MPPT)
PowerPort dcdc_lv_port(&lv_bus);         // internal low voltage side of DC/DC converter
#if CONFIG_HV_TERMINAL_NANOGRID
Dcdc dcdc(&hv_terminal, &dcdc_lv_port, MODE_NANOGRID);
#elif CONFIG_HV_TERMINAL_BATTERY
Dcdc dcdc(&hv_terminal, &dcdc_lv_port, MODE_MPPT_BOOST);
#else
Dcdc dcdc(&hv_terminal, &dcdc_lv_port, MODE_MPPT_BUCK);
#endif // CONFIG_HV_TERMINAL
#endif

#if CONFIG_HAS_PWM_SWITCH
PowerPort pwm_terminal(&lv_bus);         // internal side of PWM switch
PwmSwitch pwm_switch(&pwm_terminal);
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
PowerPort &solar_terminal = pwm_terminal;
#endif

#if CONFIG_HV_TERMINAL_NANOGRID
PowerPort &grid_terminal = hv_terminal;
#endif

#if CONFIG_LV_TERMINAL_BATTERY
PowerPort &bat_terminal = lv_terminal;
#elif CONFIG_HV_TERMINAL_BATTERY
PowerPort &bat_terminal = hv_terminal;
#endif

Charger charger(&lv_terminal);

BatConf bat_conf;               // actual (used) battery configuration
BatConf bat_conf_user;          // temporary storage where the user can write to

DeviceStatus dev_stat;

extern ThingSet ts;             // defined in data_objects.cpp

time_t timestamp;    // current unix timestamp (independent of time(NULL), as it is user-configurable)

int main()
{
    daq_tests();
    bat_charger_tests();
    power_port_tests();
    half_brigde_tests();
    dcdc_tests();
    device_status_tests();
    load_tests();
}
