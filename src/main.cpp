/* LibreSolar MPPT charge controller firmware
 * Copyright (c) 2016-2018 Martin Jäger (www.libre.solar)
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

#ifndef UNIT_TEST

#include "mbed.h"

#include "thingset.h"
#include "pcb.h"
#include "data_objects.h"
#include "config.h"

#include "half_bridge.h"        // PWM generation for DC/DC converter
#include "hardware.h"           // hardware-related functions like load switch, LED control, watchdog, etc.
#include "dcdc.h"               // DC/DC converter control (hardware independent)
#include "bat_charger.h"        // battery settings and charger state machine
#include "adc_dma.h"            // ADC using DMA and conversion to measurement values
#include "interface.h"          // communication interfaces, displays, etc.
#include "eeprom.h"             // external I2C EEPROM
#include "load.h"               // load and USB output management

//----------------------------------------------------------------------------
// global variables

Serial serial(PIN_SWD_TX, PIN_SWD_RX, "serial");

dcdc_t dcdc = {};
dcdc_port_t hs_port = {};       // high-side (solar for typical MPPT)
dcdc_port_t ls_port = {};       // low-side (battery for typical MPPT)
dcdc_port_t *bat_port = NULL;
battery_t bat;
battery_user_settings_t bat_user;
load_output_t load;
log_data_t log_data;
ts_data_t ts;

//----------------------------------------------------------------------------
// function prototypes

void setup();
void energy_counter(float timespan);

void interfaces_init();
void interfaces_process_asap();     // called each time the main loop is in idle
void interfaces_process_1s();       // called every second


void system_control()       // called by control timer (see hardware.cpp)
{
    // convert ADC readings to meaningful measurement values
    update_measurements(&dcdc, &bat, &load, &hs_port, &ls_port);

    // control PWM of the DC/DC according to hs and ls port settings
    // (this function includes MPPT algorithm)
    dcdc_control(&dcdc, &hs_port, &ls_port);

    load_check_overcurrent(&load);
    update_dcdc_led(half_bridge_enabled());
}

//----------------------------------------------------------------------------
int main()
{
    setup();
    interfaces_init();

    wait(2);    // safety feature: be able to re-flash before starting
    init_watchdog(10);      // 10s should be enough for communication ports

    switch(dcdc.mode)
    {
        case MODE_NANOGRID:
            dcdc_port_init_nanogrid(&hs_port);
            dcdc_port_init_bat(&ls_port, &bat);
            bat_port = &ls_port;
            break;
        case MODE_MPPT_BUCK:     // typical MPPT charge controller operation
            dcdc_port_init_solar(&hs_port);
            dcdc_port_init_bat(&ls_port, &bat);
            bat_port = &ls_port;
            break;
        case MODE_MPPT_BOOST:    // for charging of e-bike battery via solar panel
            dcdc_port_init_solar(&ls_port);
            dcdc_port_init_bat(&hs_port, &bat);
            bat_port = &hs_port;
            break;
    }
    control_timer_start(50);  // 50 Hz

    time_t now = time(NULL);
    time_t last_second = now;

    // the main loop is suitable for slow tasks like communication (even blocking wait allowed)
    while(1) {

        interfaces_process_asap();

        update_soc_led(&bat);

        now = time(NULL);
        if (now >= last_second + 1) {   // called once per second (or slower if blocking wait occured somewhere)

            //printf("Still alive... time: %d, mode: %d\n", (int)time(NULL), dcdc.mode);

            battery_update_energy(&bat, bat_port->voltage, bat_port->current, last_second - now);      // also does energy counting

            charger_state_machine(bat_port, &bat, bat_port->voltage, bat_port->current);

            load_state_machine(&load, ls_port.input_allowed);

            interfaces_process_1s();
            
            last_second = now;
        }
        feed_the_dog();
        sleep();    // wake-up by timer interrupts
    }
}

void interfaces_init()
{
#ifdef UART_SERIAL_ENABLED
    serial.baud(57600);
    //freopen("/serial", "w", stdout);  // retarget stdout
    uart_serial_init(&serial);
    //usb_serial_init(&usbSerial);
#endif

#ifdef GSM_ENABLED
    gsm_init();
#endif

#ifdef LORA_ENABLED
    lora_init();
#endif
}

void interfaces_process_asap()
{
#ifdef UART_SERIAL_ENABLED
    uart_serial_process();
#endif
}

void interfaces_process_1s()
{
#ifdef OLED_ENABLED
    oled_output(&dcdc, &hs_port, &ls_port, &bat, &load);
#endif

#ifdef CAN_ENABLED

#endif

#ifdef USB_SERIAL_ENABLED

#endif

#ifdef GSM_ENABLED
    gsm_process();
#endif

#ifdef LORA_ENABLED
    lora_process();
#endif

#ifdef UART_SERIAL_ENABLED
    uart_serial_pub();
#endif

    fflush(stdout);
}

//----------------------------------------------------------------------------
void setup()
{
    load_init(&load);
    init_leds();

    battery_init(&bat, &bat_user);

    dcdc_init(&dcdc);

    half_bridge_init(PWM_FREQUENCY, 300, 12 / dcdc.hs_voltage_max, 0.97);       // lower duty limit might have to be adjusted dynamically depending on LS voltage

    eeprom_restore_data(&ts);

    adc_setup();
    dma_setup();

    adc_timer_setup(1000);  // 1 kHz

    wait(0.2);      // wait for ADC to collect some measurement values

    update_measurements(&dcdc, &bat, &load, &hs_port, &ls_port);
    calibrate_current_sensors(&dcdc, &load);
}

#endif
