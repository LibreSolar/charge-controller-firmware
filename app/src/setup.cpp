/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *
 * @brief Setup of ports and other essential charge controller objects
 */

#include <zephyr/kernel.h>

#include "thingset.h" // handles access to internal data via communication interfaces

#include "bat_charger.h" // battery settings and charger state machine
#include "board.h"
#include "daq.h"           // ADC using DMA and conversion to measurement values
#include "data_objects.h"  // for access to internal data via ThingSet
#include "dcdc.h"          // DC/DC converter control (hardware independent)
#include "device_status.h" // log data (error memory, min/max measurements, etc.)
#include "hardware.h"   // hardware-related functions like load switch, LED control, watchdog, etc.
#include "leds.h"       // LED switching using charlieplexing
#include "load.h"       // load and USB output management
#include "pwm_switch.h" // PWM charge controller

// low voltage terminal (battery for typical MPPT)
extern struct power_port lv_terminal;
struct dc_bus lv_bus = { .series_multiplier = 1, .ref_current = &lv_terminal.current };
struct power_port lv_terminal = { .bus = &lv_bus };

#if BOARD_HAS_DCDC
// high voltage terminal (solar for typical MPPT)
extern struct power_port hv_terminal;
struct dc_bus hv_bus = { .series_multiplier = 1, .ref_current = &hv_terminal.current };
struct power_port hv_terminal = { .bus = &hv_bus };
#if CONFIG_HV_TERMINAL_NANOGRID
Dcdc dcdc(DEVICE_DT_GET(HALF_BRIDGE_NODE), &hv_bus, &lv_bus, DCDC_MODE_AUTO);
#elif CONFIG_HV_TERMINAL_BATTERY
Dcdc dcdc(DEVICE_DT_GET(HALF_BRIDGE_NODE), &hv_bus, &lv_bus, DCDC_MODE_BOOST);
#else
Dcdc dcdc(DEVICE_DT_GET(HALF_BRIDGE_NODE), &hv_bus, &lv_bus, DCDC_MODE_BUCK);
#endif // CONFIG_HV_TERMINAL
#endif

#if BOARD_HAS_PWM_PORT
PwmSwitch pwm_switch(&lv_bus, DEVICE_DT_GET(PWM_SWITCH_NODE));
#endif

#if BOARD_HAS_LOAD_OUTPUT
#if LED_EXISTS(load)
LoadOutput load(&lv_bus, DEVICE_DT_GET(LOAD_SWITCH_NODE), LED_POS(load));
#else
LoadOutput load(&lv_bus, DEVICE_DT_GET(LOAD_SWITCH_NODE));
#endif
#endif

#if BOARD_HAS_USB_OUTPUT
LoadOutput usb_pwr(&lv_bus, DEVICE_DT_GET(USB_SWITCH_NODE));
#endif

#if CONFIG_HV_TERMINAL_SOLAR
struct power_port &solar_terminal = hv_terminal;
#elif CONFIG_LV_TERMINAL_SOLAR
struct power_port &solar_terminal = lv_terminal;
#elif CONFIG_PWM_TERMINAL_SOLAR
struct power_port &solar_terminal = pwm_switch;
#endif

#if CONFIG_HV_TERMINAL_NANOGRID
struct power_port &grid_terminal = hv_terminal;
#endif

#if CONFIG_LV_TERMINAL_BATTERY
struct power_port &bat_terminal = lv_terminal;
#elif CONFIG_HV_TERMINAL_BATTERY
struct power_port &bat_terminal = hv_terminal;
#endif

Charger charger(&bat_terminal);

BatConf bat_conf;      // actual (used) battery configuration
BatConf bat_conf_user; // temporary storage where the user can write to

DeviceStatus dev_stat;

// current unix timestamp (independent of time(NULL), as it is user-configurable)
// uint32_t considered large enough, so we avoid 64-bit math (overflow in year 2106)
uint32_t timestamp;

#ifndef UNIT_TEST

#include <zephyr/kernel.h>

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
