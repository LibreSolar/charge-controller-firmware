/* Libre Solar Battery Management System firmware
 * Copyright (c) 2016-2019 Martin Jäger (www.libre.solar)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef __ZEPHYR__

#include <zephyr.h>
#include <stdio.h>

#include "thingset.h"           // handles access to internal data via communication interfaces
#include "pcb.h"                // hardware-specific settings
#include "config.h"             // user-specific configuration

#include "half_bridge.h"        // PWM generation for DC/DC converter
#include "hardware.h"           // hardware-related functions like load switch, LED control, watchdog, etc.
#include "dcdc.h"               // DC/DC converter control (hardware independent)
#include "pwm_switch.h"         // PWM charge controller
#include "bat_charger.h"        // battery settings and charger state machine
#include "adc_dma.h"            // ADC using DMA and conversion to measurement values
#include "ext/uext.h"           // communication interfaces, displays, etc. in UEXT connector
#include "eeprom.h"             // external I2C EEPROM
#include "load.h"               // load and USB output management
#include "leds.h"               // LED switching using charlieplexing
#include "device_status.h"                // log data (error memory, min/max measurements, etc.)
#include "data_objects.h"       // for access to internal data via ThingSet
//#include "thingset_serial.h"    // UART or USB serial communication

PowerPort lv_terminal;          // low voltage terminal (battery for typical MPPT)

#if FEATURE_DCDC_CONVERTER
PowerPort hv_terminal;          // high voltage terminal (solar for typical MPPT)
PowerPort dcdc_lv_port;         // internal low voltage side of DC/DC converter
Dcdc dcdc(&hv_terminal, &dcdc_lv_port, DCDC_MODE_INIT);
#endif

#if FEATURE_PWM_SWITCH
PowerPort pwm_terminal;         // external terminal of PWM switch port (normally solar)
PowerPort pwm_port_int;         // internal side of PWM switch
PwmSwitch pwm_switch(&pwm_terminal, &pwm_port_int);
PowerPort &solar_terminal = pwm_terminal;
#else
PowerPort &solar_terminal = SOLAR_TERMINAL;     // defined in config.h
#endif

#if FEATURE_LOAD_OUTPUT
PowerPort load_terminal;        // load terminal (also connected to lv_bus)
LoadOutput load(&load_terminal);
#endif

PowerPort &bat_terminal = BATTERY_TERMINAL;     // defined in config.h
#ifdef GRID_TERMINAL
PowerPort &grid_terminal = GRID_TERMINAL;
#endif

Charger charger(&bat_terminal);

BatConf bat_conf;               // actual (used) battery configuration
BatConf bat_conf_user;          // temporary storage where the user can write to

DeviceStatus dev_stat;

extern ThingSet ts;             // defined in data_objects.cpp

time_t timestamp;    // current unix timestamp (independent of time(NULL), as it is user-configurable)

void main(void)
{
    printf("Booting Libre Solar Charge Controller: %s\n", CONFIG_BOARD);

    while (1) {

        k_sleep(1000);
    }
}

//K_THREAD_DEFINE(ts_serial_id, 4096, thingset_serial_thread, NULL, NULL, NULL, 5, 0, K_NO_WAIT);

//K_THREAD_DEFINE(leds_id, 256, leds_update_thread, NULL, NULL, NULL,	4, 0, K_NO_WAIT);

//K_THREAD_DEFINE(uext_thread, 1024, uext_process_1s, NULL, NULL, NULL, 6, 0, K_NO_WAIT);

#endif // __ZEPHYR__
