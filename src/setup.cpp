/*
 * Copyright (c) 2020 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *
 * @brief Setup of ports and other essential charge controller objects
 */

#include <zephyr.h>

#include "thingset.h"           // handles access to internal data via communication interfaces

#include "half_bridge.h"        // PWM generation for DC/DC converter
#include "hardware.h"           // hardware-related functions like load switch, LED control, watchdog, etc.
#include "dcdc.h"               // DC/DC converter control (hardware independent)
#include "pwm_switch.h"         // PWM charge controller
#include "bat_charger.h"        // battery settings and charger state machine
#include "daq.h"                // ADC using DMA and conversion to measurement values
#include "eeprom.h"             // external I2C EEPROM
#include "load.h"               // load and USB output management
#include "leds.h"               // LED switching using charlieplexing
#include "device_status.h"      // log data (error memory, min/max measurements, etc.)
#include "data_nodes.h"         // for access to internal data via ThingSet

DcBus lv_bus;
PowerPort lv_terminal(&lv_bus, true);   // low voltage terminal (battery for typical MPPT)

#if DT_NODE_EXISTS(DT_PATH(dcdc))
DcBus hv_bus;
PowerPort hv_terminal(&hv_bus, true);   // high voltage terminal (solar for typical MPPT)
#if CONFIG_HV_TERMINAL_NANOGRID
Dcdc dcdc(&hv_bus, &lv_bus, DCDC_MODE_AUTO);
#elif CONFIG_HV_TERMINAL_BATTERY
Dcdc dcdc(&hv_bus, &lv_bus, DCDC_MODE_BOOST);
#else
Dcdc dcdc(&hv_bus, &lv_bus, DCDC_MODE_BUCK);
#endif // CONFIG_HV_TERMINAL
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), pwm_switch))
PwmSwitch pwm_switch(&lv_bus);
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
LoadOutput load(&lv_bus, &load_out_set, &load_out_init);
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), usb_pwr))
LoadOutput usb_pwr(&lv_bus, &usb_out_set, &usb_out_init);
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

// current unix timestamp (independent of time(NULL), as it is user-configurable)
// uint32_t considered large enough, so we avoid 64-bit math (overflow in year 2106)
uint32_t timestamp;

#ifndef UNIT_TEST

#include <zephyr.h>

static inline void timestamp_inc(struct k_timer *timer_id)
{
    ARG_UNUSED(timer_id);
    timestamp++;
}

void setup()
{
    static struct k_timer timestamp_timer;
    k_timer_init(&timestamp_timer, timestamp_inc, NULL);
    k_timer_start(&timestamp_timer, K_MSEC(1000), K_MSEC(1000));

    /*
     * printf from newlib-nano requires malloc, but Zephyr garbage-collects heap management if it
     * is not used anywhere in the code. Below dummy calls force Zephyr to build with heap support.
     */
    void *temp = k_malloc(4);
    k_free(temp);
}

#endif
