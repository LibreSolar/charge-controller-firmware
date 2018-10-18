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

#include "half_bridge.h"        // PWM generation for DC/DC converter
#include "hardware.h"           // hardware-related functions like load switch, LED control, watchdog, etc.
#include "dcdc.h"               // DC/DC converter control (hardware independent)
#include "charger.h"            // battery charging state machine
#include "adc_dma.h"            // ADC using DMA and conversion to measurement values
#include "interface.h"          // communication interfaces, displays, etc.
#include "eeprom.h"             // external I2C EEPROM

//----------------------------------------------------------------------------
// global variables

Serial serial(PIN_SWD_TX, PIN_SWD_RX, "serial");
I2C i2c(PIN_UEXT_SDA, PIN_UEXT_SCL);

#ifdef STM32F0
//#include "USBSerial.h"
//USBSerial usbSerial(0x1f00, 0x2012, 0x0001,  false);    // connection is not blocked when USB is not plugged in
#endif

dcdc_t dcdc = {};
dcdc_port_t hs_port = {};       // high-side (solar for typical MPPT)
dcdc_port_t ls_port = {};       // low-side (battery for typical MPPT)
dcdc_port_t *bat_port = NULL;
battery_t bat;
load_output_t load;
log_data_t log_data;
ts_data_t ts;

#ifdef PIN_USB_PWR_EN
DigitalOut usb_pwr_en(PIN_USB_PWR_EN);
#endif

#ifdef PIN_UEXT_DIS
DigitalOut uext_dis(PIN_UEXT_DIS);
#endif

#ifdef PIN_REF_I_DCDC
AnalogOut ref_i_dcdc(PIN_REF_I_DCDC);
#endif 

// for daily energy counting
int seconds_zero_solar = 0;
int seconds_day = 0;
int day_counter = 0;
bool nighttime = false;

Timer tim;

// temporary to check timer for control part
extern int num_adc_conversions;
volatile int num_control_calls;
volatile int num_control_calls_prev;

//----------------------------------------------------------------------------
// function prototypes

void setup();
void energy_counter(float timespan);

// static variables (only visible in this file)
static bool pub_data_enabled;

void check_overcurrent()
{
    if (load.current > load.current_max) {
        disable_load();
        load.overcurrent_flag = true;
    }
}

void system_control()       // called by control timer (see hardware.cpp)
{
    num_control_calls++;

    update_measurements(&dcdc, &bat, &load, &hs_port, &ls_port);
    dcdc_control(&dcdc, &hs_port, &ls_port);

    check_overcurrent();

    update_dcdc_led(half_bridge_enabled());
}

//----------------------------------------------------------------------------
int main()
{
    setup();
    wait(1);    // safety feature: be able to re-flash before starting
    output_oled(&dcdc, &hs_port, &ls_port, &bat, &load);
    wait(1);
    init_watchdog(10);      // 10s should be enough for communication ports

    time_t now = time(NULL);
    time_t last_second = now;

    switch(dcdc_mode)
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

    // the main loop is suitable for slow tasks like communication (even blocking wait allowed)
    while(1) {

        update_soc_led(&bat);

        uart_serial_process();
        //usb_serial_process();

        now = time(NULL);

        // called once per second (or slower if blocking wait occured somewhere)
        if (now >= last_second + 1) {

            float control_frequency = (num_control_calls - num_control_calls_prev) / tim.read();
            num_control_calls_prev = num_control_calls;
            tim.reset();

            printf("Still alive... time: %d, contr_freq: %.1f Hz\n", (int)time(NULL), control_frequency);

            charger_state_machine(bat_port, &bat, bat_port->voltage, bat_port->current);

            bat_calculate_soc(&bat, bat_port->voltage, bat_port->current);

            // load management
            if (ls_port.input_allowed == false || load.overcurrent_flag == true) {
                disable_load();
                //usb_pwr_en = 0;
                load.enabled = false;
            }
            else {
                if (load.enabled_target == true) {
                    enable_load();
                    load.enabled = true;
                }
                else {
                    disable_load();
                    load.enabled = false;
                }
                if (load.usb_enabled_target == true) {
                    //usb_pwr_en = 1;
                }
                else {
                    //usb_pwr_en = 0;
                }
            }
            energy_counter(last_second - now);

            output_oled(&dcdc, &hs_port, &ls_port, &bat, &load);
            //output_serial(&meas, &chg);
            if (pub_data_enabled) {
                //usb_serial_pub();
                uart_serial_pub();
            }
            
            fflush(stdout);
            last_second = now;
        }
        feed_the_dog();
        sleep();    // wake-up by timer interrupts
    }
}

//----------------------------------------------------------------------------
void setup()
{
#ifdef PIN_REF_I_DCDC
    ref_i_dcdc = 0.5;    // reference voltage for zero current (0.1 for buck, 0.9 for boost, 0.5 for bi-directional)
#endif

#ifdef PIN_UEXT_DIS
    uext_dis = 0;
#endif

#ifdef PIN_USB_PWR_EN
    usb_pwr_en = 1;
#endif

    disable_load();
    init_leds();

    serial.baud(57600);
    //printf("\nSerial interface started...\n");
    //freopen("/serial", "w", stdout);  // retarget stdout

    battery_init(&bat, battery_config_user);

    dcdc_init(&dcdc);

    half_bridge_init(PWM_FREQUENCY, 300, 12 / dcdc.hs_voltage_max, 0.97);       // lower duty limit might have to be adjusted dynamically depending on LS voltage

    load.current_max = LOAD_CURRENT_MAX;
    load.overcurrent_flag = false;
    load.enabled_target = true;
    load.usb_enabled_target = true;

    uart_serial_init(&serial);
    //usb_serial_init(&usbSerial);
    
    eeprom_restore_data(&ts);

    setup_adc_timer(1000);  // 1 kHz
    setup_adc();
    start_dma();

    wait(0.2);      // wait for ADC to collect some measurement values

    update_measurements(&dcdc, &bat, &load, &hs_port, &ls_port);
    calibrate_current_sensors(&dcdc, &load);
    update_measurements(&dcdc, &bat, &load, &hs_port, &ls_port);            // second time to get proper values of power measurement

    setup_control_timer(50);  // 50 Hz
}

//----------------------------------------------------------------------------
// timespan in seconds since last call
void energy_counter(float timespan)
{
    float dcdc_power = ls_port.voltage * ls_port.current;

    if (dcdc_power < 0.1) {
        seconds_zero_solar++;
    }
    else {
        seconds_zero_solar = 0;
    }

    // detect night time for reset of daily counter in the morning
    if (seconds_zero_solar > 60*60*5) {
        nighttime = true;
    }

    if (nighttime && dcdc_power > 0.1) {
        nighttime = false;
        bat.input_Wh_day = 0.0;
        bat.output_Wh_day = 0.0;
        seconds_day = 0;
        day_counter++;
        eeprom_store_data(&ts);
    }
    else {
        seconds_day++;
    }

    bat.input_Wh_day += dcdc_power * timespan / 3600.0;
    bat.output_Wh_day += load.current * ls_port.voltage * timespan / 3600.0;
    bat.input_Wh_total += dcdc_power * timespan / 3600.0;
    bat.output_Wh_total += dcdc.ls_current * ls_port.voltage * timespan / 3600.0;
}

#endif
