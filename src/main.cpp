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

#include "half_bridge.h"        // PWM generation for DC/DC converter
#include "hardware.h"           // hardware-related functions like load switch, LED control, watchdog, etc.
#include "dcdc.h"               // DC/DC converter control (hardware independent)
#include "charger.h"            // battery charging state machine
#include "adc_dma.h"            // ADC using DMA and conversion to measurement values
#include "output.h"
#include "communication.h"
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

//----------------------------------------------------------------------------
// function prototypes

void setup();
void energy_counter();
void setup_control_timer();

// static variables (only visible in this file)
static bool pub_data_enabled;

void check_overcurrent()
{
    if (load.current > load.current_max) {
        disable_load();
        load.overcurrent_flag = true;
    }
}


// will be called by timer interrupt in the future
void system_control()
{
    num_control_calls++;
    if (new_reading_available) {

        update_measurements(&dcdc, &bat, &load, &hs_port, &ls_port);
        dcdc_control(&dcdc, &hs_port, &ls_port);
        //printf("duration: %d us\n", tim.read_us());

        check_overcurrent();
        update_solar_led(half_bridge_enabled());
        new_reading_available = false;
        //printf("SysControl %d s / %d us\n", (int)time(NULL), tim.read_us());
    }
}

//----------------------------------------------------------------------------
int main()
{
    setup();
    wait(1);    // safety feature: be able to re-flash before starting
    output_oled(&dcdc, &hs_port, &ls_port, &bat, &load);
    wait(1);
    init_watchdog(10);      // 10s should be enough for communication ports

    time_t last_second = time(NULL);

    switch(system_mode)
    {
        case NANOGRID:
            dcdc_port_init_nanogrid(&hs_port);
            dcdc_port_init_bat(&ls_port, &bat);
            break;
        case MPPT_BUCK:     // typical MPPT charge controller operation
            dcdc_port_init_solar(&hs_port);
            dcdc_port_init_bat(&ls_port, &bat);
            break;
        case MPPT_BOOST:    // for charging of e-bike battery via solar panel
            dcdc_port_init_solar(&ls_port);
            dcdc_port_init_bat(&hs_port, &bat);
            break;
    }

    bat_port = &ls_port;            // to be adjusted (depending on battery)

    while(1) {

        system_control();       // call to be moved to timer interrupt

        flash_led_soc(&bat);
        uart_serial_process();
        //usb_serial_process();

        // called once per second
        if (time(NULL) - last_second >= 1) {
            last_second = time(NULL);

            //printf("Still alive... time: %d, num_adc: %d, num_control: %d\n", (int)time(NULL), num_adc_conversions, num_control_calls);

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
            energy_counter();   // TODO: include delta time if >1 sec

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

    disable_load();
    init_leds();

    serial.baud(57600);
    //printf("\nSerial interface started...\n");
    //freopen("/serial", "w", stdout);  // retarget stdout

    battery_init(&bat, battery_config_user);
    //battery_init(&bat, BAT_TYPE_LFP, 20, 4);

    dcdc_init(&dcdc);

    half_bridge_init(PWM_FREQUENCY, 300, 12 / dcdc.hs_voltage_max, 0.97);       // lower duty limit might have to be adjusted dynamically depending on LS voltage

    load.current_max = LOAD_CURRENT_MAX;
    load.overcurrent_flag = false;
    load.enabled_target = true;
    load.usb_enabled_target = true;

    uart_serial_init(&serial);
    //usb_serial_init(&usbSerial);
    
    eeprom_restore_data(&ts);

    setup_adc_timer();
    setup_adc();
    start_dma();

    wait(0.2);      // wait for ADC to collect some measurement values

    update_measurements(&dcdc, &bat, &load, &hs_port, &ls_port);
    calibrate_current_sensors(&dcdc, &load);
    update_measurements(&dcdc, &bat, &load, &hs_port, &ls_port);            // second time to get proper values of power measurement

    //setup_control_timer();
    //output_sdcard();
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

#if defined(STM32F0)
/*
void setup_dcdc_timer()
{
    // Enable peripheral clock of GPIOA and GPIOB
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;

    // Enable TIM1 clock
    RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;

    // timer clock: 48 MHz / 4800 = 10 kHz
    TIM15->PSC = 4799;

    // interrupt on timer update
    TIM15->DIER |= TIM_DIER_UIE;

    // Auto Reload Register
    TIM15->ARR = 10;        // 1 kHz sample frequency at 10 kHz clock

    //NVIC_SetPriority(DMA1_Channel1_IRQn, 16);
    NVIC_EnableIRQ(TIM15_IRQn);

    // Control Register 1
    // TIM_CR1_CEN =  1: Counter enable
    TIM15->CR1 |= TIM_CR1_CEN;
}

extern "C" void TIM15_IRQHandler(void)
{
    TIM15->SR &= ~(1 << 0);
    ADC1->CR |= ADC_CR_ADSTART;
}
*/
#elif defined(STM32L0)

void setup_control_timer()
{
    // Enable peripheral clock of GPIOA and GPIOB
    //RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;

    // Enable TIM7 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM7EN;

    // timer clock: 48 MHz / 4800 = 1 kHz
    TIM7->PSC = 47999;

    // interrupt on timer update
    TIM7->DIER |= TIM_DIER_UIE;

    // Auto Reload Register
    TIM7->ARR = 10;        // 1 kHz sample frequency at 10 kHz clock

    //NVIC_SetPriority(DMA1_Channel1_IRQn, 16);
    NVIC_EnableIRQ(TIM7_IRQn);

    // Control Register 1
    // TIM_CR1_CEN =  1: Counter enable
    TIM7->CR1 |= TIM_CR1_CEN;
}

extern "C" void TIM7_IRQHandler(void)
{
    TIM7->SR &= ~(1 << 0);
    //system_control();
}

#endif
