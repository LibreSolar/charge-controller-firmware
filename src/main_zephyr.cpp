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
#include "adc_dma.h"            // ADC using DMA and conversion to measurement values
#include "ext/ext.h"           // communication interfaces, displays, etc. in UEXT connector
#include "eeprom.h"             // external I2C EEPROM
#include "load.h"               // load and USB output management
#include "leds.h"               // LED switching using charlieplexing
#include "device_status.h"                // log data (error memory, min/max measurements, etc.)
#include "data_objects.h"       // for access to internal data via ThingSet

PowerPort lv_terminal;          // low voltage terminal (battery for typical MPPT)

#if CONFIG_HAS_DCDC_CONVERTER
PowerPort hv_terminal;          // high voltage terminal (solar for typical MPPT)
PowerPort dcdc_lv_port;         // internal low voltage side of DC/DC converter
#if CONFIG_HV_TERMINAL_NANOGRID
Dcdc dcdc(&hv_terminal, &dcdc_lv_port, MODE_NANOGRID);
#elif CONFIG_HV_TERMINAL_BATTERY
Dcdc dcdc(&hv_terminal, &dcdc_lv_port, MODE_MPPT_BOOST);
#else
Dcdc dcdc(&hv_terminal, &dcdc_lv_port, MODE_MPPT_BUCK);
#endif // CONFIG_HV_TERMINAL
#endif

#if CONFIG_HAS_PWM_SWITCH
PowerPort pwm_terminal;         // external terminal of PWM switch port (normally solar)
PowerPort pwm_port_int;         // internal side of PWM switch
PwmSwitch pwm_switch(&pwm_terminal, &pwm_port_int);
#endif

#if CONFIG_HAS_LOAD_OUTPUT
PowerPort load_terminal;        // load terminal (also connected to lv_bus)
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

Charger charger(&bat_terminal);

BatConf bat_conf;               // actual (used) battery configuration
BatConf bat_conf_user;          // temporary storage where the user can write to

DeviceStatus dev_stat;

extern ThingSet ts;             // defined in data_objects.cpp

time_t timestamp;    // current unix timestamp (independent of time(NULL), as it is user-configurable)

#define USB_PWR_EN_PORT    DT_ALIAS_USB_PWR_EN_GPIOS_CONTROLLER
#define USB_PWR_EN_PIN         DT_ALIAS_USB_PWR_EN_GPIOS_PIN

void main(void)
{
    u32_t cnt = 0;
    struct device *dev_usb_en;

    dev_usb_en = device_get_binding(USB_PWR_EN_PORT);
    gpio_pin_configure(dev_usb_en, USB_PWR_EN_PIN, GPIO_DIR_OUT);
    gpio_pin_write(dev_usb_en, USB_PWR_EN_PIN, 1);

    printf("Booting Libre Solar Charge Controller: %s\n", CONFIG_BOARD);

    // initialize all extensions and external communication interfaces
    ext_mgr.enable_all();

    k_sleep(2000);      // safety feature: be able to re-flash before starting

    while (1) {
        //gpio_pin_write(dev_usb_en, USB_PWR_EN_PIN, cnt % 2);

        leds_update_1s();
        leds_update_soc(charger.soc, dev_stat.has_error(ERR_LOAD_LOW_SOC));

        cnt++;
        k_sleep(1000);
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
            printf("%d\n", now);
        }
        cnt++;
        k_sleep(1);
    }
}

K_THREAD_DEFINE(leds_id, 256, leds_update_thread, NULL, NULL, NULL,	4, 0, K_NO_WAIT);

K_THREAD_DEFINE(ext_thread, 1024, ext_mgr_thread, NULL, NULL, NULL, 6, 0, 1000);

#endif // __ZEPHYR__
