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
#include "config.h"
#include "data_objects.h"

#include "half_bridge.h"
#include "hardware.h"
#include "charger.h"
#include "adc_dma.h"
#include "output.h"
#include "communication.h"
#include "eeprom.h"

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

//Thread dcdc_thread;
Timer tim;

//----------------------------------------------------------------------------
// function prototypes

void setup();
void energy_counter();


void check_overcurrent()
{
    if (load.current > load.current_max) {
        disable_load();
        load.overcurrent_flag = true;
    }
}

//----------------------------------------------------------------------------
int main()
{
    setup();
    wait(2);    // safety feature: be able to re-flash before starting
    init_watchdog(0.5);

    time_t last_second = time(NULL);

    // configure high-side and low-side port of DC/DC (defines control mode)
    dcdc_port_init_solar(&hs_port);
    //dcdc_port_init_nanogrid(&hs_port);
    dcdc_port_init_bat(&ls_port, &bat);

    while(1) {

        if (new_reading_available) {

            //tim.reset();
            //tim.start();
            update_measurements(&dcdc, &bat, &load, &hs_port, &ls_port);
            dcdc_control(&dcdc, &hs_port, &ls_port);
            //printf("duration: %d us\n", tim.read_us());

            check_overcurrent();
            update_solar_led(half_bridge_enabled());
            new_reading_available = false;
        }

        flash_led_soc(&bat);
        uart_serial_process();
        //usb_serial_process();

        // called once per second
        if (time(NULL) - last_second >= 1) {
            last_second = time(NULL);

            //serial.printf("Still alive... time: %d\n", (int)time(NULL));

            // TEST only!!
            //if (dcdc_enabled()) dcdc_stop();
            //else dcdc_start(0.9);

            charger_state_machine(&ls_port, &bat, ls_port.voltage, ls_port.current);

            bat_calculate_soc(&bat, ls_port.voltage, ls_port.current);

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
            energy_counter();

            output_oled(&dcdc, &hs_port, &ls_port, &bat, &load);
            //output_serial(&meas, &chg);
            if (pub_data_enabled) {
                //usb_serial_pub();
                uart_serial_pub();
            }
            
            //usbSerial.printf("I am a USB serial port\r\n"); // 12 Mbit/s (USB full-speed)

            fflush(stdout);
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

    init_leds();
    disable_load();

    //led_soc_12 = 1;

    serial.baud(57600);
    //printf("\nSerial interface started...\n");
    //freopen("/serial", "w", stdout);  // retarget stdout

    battery_init(&bat, BAT_TYPE_FLOODED, 7);
    //battery_init(&bat, BAT_TYPE_LFP, 20);

    dcdc_init(&dcdc);

    // Caution: initialize battery and dcdc before half-bridge to get correct limits!
    float lower_duty_limit = (float) bat.cell_voltage_load_disconnect * bat.num_cells / dcdc.hs_voltage_max;
    half_bridge_init(PWM_FREQUENCY, 300, lower_duty_limit, 0.97);

    load.current_max = LOAD_CURRENT_MAX;
    load.overcurrent_flag = false;
    load.enabled_target = true;
    load.usb_enabled_target = true;
    //cal.pub_data_enabled = false;

    uart_serial_init(&serial);
    //usb_serial_init(&usbSerial);
    
    eeprom_restore_data(&ts);

    setup_adc_timer();
    setup_adc();
    start_dma();

    wait(0.2);      // wait for ADC to collect some measurement values

    update_measurements(&dcdc, &bat, &load, &hs_port, &ls_port);
    calibrate_current_sensors(&dcdc, &load);
}

//----------------------------------------------------------------------------
// must be called once per second
void energy_counter()
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

    bat.input_Wh_day += dcdc_power / 3600.0;
    bat.output_Wh_day += load.current * ls_port.voltage / 3600.0;
    bat.input_Wh_total += dcdc_power / 3600.0;
    bat.output_Wh_total += dcdc.ls_current * ls_port.voltage/ 3600.0;
}

#endif
