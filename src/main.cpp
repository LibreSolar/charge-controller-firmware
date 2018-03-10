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
#include "config.h"
#include "data_objects.h"
#include "dcdc.h"
#include "charger.h"
#include "adc_dma.h"

//#include "24LCxx_I2C.h"
#include <math.h>     // log for thermistor calculation

#include "output.h"
#include "output_can.h"

//----------------------------------------------------------------------------
// global variables

Serial serial(PIN_SWD_TX, PIN_SWD_RX, "serial");
CAN can(PIN_CAN_RX, PIN_CAN_TX, CAN_SPEED);
I2C i2c(PIN_UEXT_SDA, PIN_UEXT_SCL);

DeviceState device;
CalibrationData cal;
ChargingProfile profile;

float dcdc_power;    // stores previous output power
int pwm_delta = 1;
int dcdc_off_timestamp = -cal.dcdc_restart_interval;    // start immediately

DigitalOut led_green(PIN_LED_GREEN);
DigitalOut led_red(PIN_LED_RED);
DigitalOut load_disable(PIN_LOAD_DIS);
DigitalOut can_disable(PIN_CAN_STB);
DigitalOut out_5v_enable(PIN_5V_OUT_EN);
DigitalOut uext_disable(PIN_UEXT_DIS);

AnalogOut ref_i_dcdc(PIN_REF_I_DCDC);

float solar_voltage;
float battery_voltage;
float dcdc_current;
float dcdc_current_offset = 0.0;
float load_current;     
float load_current_offset = 0.0;
float temp_mosfets;  // °C
float temp_battery;  // °C

// for daily energy counting
int seconds_zero_solar = 0;
int seconds_day = 0;
int day_counter = 0;
bool nighttime = false;

//----------------------------------------------------------------------------
// function prototypes

void setup();
void energy_counter();
void update_mppt();

void init_watchdog(float timeout);      // timeout in seconds
void feed_the_dog();

//----------------------------------------------------------------------------
int main()
{
    Ticker tick1, tick2;

    setup();
    wait(3);    // safety feature: be able to re-flash before starting

    init_watchdog(1.0);

    time_t last_second = time(NULL);

    while(1) {

        update_measurements();
        update_mppt();

        can_process_outbox();
        can_process_inbox();

        // called once per second
        if (time(NULL) - last_second >= 1) {
            last_second = time(NULL);

            //serial.printf("Still alive... time: %d\n", time(NULL));

            // updates charger state machine
            charger_update(battery_voltage, dcdc_current);

            // load management
            if (charger_discharging_enabled()) {
                load_disable = 0;
            } else {
                load_disable = 1;
            }

            can_send_data();

            energy_counter();

            device.bus_voltage = battery_voltage;
            device.input_voltage = solar_voltage;
            device.bus_current = dcdc_current;
            device.input_current = dcdc_power / solar_voltage;
            device.load_current = load_current;
            device.external_temperature = temp_mosfets;
            device.internal_temperature = temp_mosfets;
            device.load_enabled = !load_disable;

            output_oled();

            fflush(stdout);
        }
        feed_the_dog();
        //sleep();    // wake-up by ticker interrupts
    }
}

//----------------------------------------------------------------------------
void setup()
{
    led_green = 1;
    load_disable = 1;
    can_disable = 0;
    out_5v_enable = 0;
    uext_disable = 0;
    ref_i_dcdc = 0.1;         // 0 for buck, 1 for boost (TODO)

    // adjust default values for 12V LiFePO4 battery
    profile.num_cells = 4;
    profile.cell_voltage_max = 3.55;        // max voltage per cell
    profile.cell_voltage_recharge = 3.35;
    profile.trickle_enabled = false;
    profile.cell_voltage_load_disconnect = 3.0;
    profile.cell_voltage_load_reconnect  = 3.15;

    serial.baud(115200);
    printf("\nSerial interface started...\n");
    freopen("/serial", "w", stdout);  // retarget stdout

    i2c.frequency(400000);

    dcdc_init(PWM_FREQUENCY);
    dcdc_deadtime_ns(300);
    dcdc_lock_settings();
    dcdc_duty_cycle_limits((float) profile.cell_voltage_load_disconnect * profile.num_cells / cal.solar_voltage_max, 0.97);

    charger_init(&profile);
    
    setup_adc_timer();
    setup_adc();
    start_dma();

    wait(0.2);      // wait for ADC to collect some measurement values

    update_measurements();
    dcdc_current_offset = -dcdc_current;
    load_current_offset = -load_current;

    can.mode(CAN::Normal);
    can.attach(&can_receive);

    // TXFP: Transmit FIFO priority driven by request order (chronologically)
    // NART: No automatic retransmission
    CAN1->MCR |= CAN_MCR_TXFP | CAN_MCR_NART;
}

void update_mppt()
{
    float dcdc_power_new = battery_voltage * dcdc_current;
/*
    if (dcdc_enabled()) {
        serial.printf("B: %.2fV, %.2fA, S: %.2fV, Pwr: %.2fW, Charger: %d, DCDC: %d, PWM: %.1f\n",
            battery_voltage, dcdc_current, solar_voltage, dcdc_power_new,
            charger_get_state(), dcdc_enabled(), dcdc_get_duty_cycle() * 100.0);
    }
*/
    if (dcdc_enabled() == false && charger_charging_enabled() == true
        && battery_voltage < charger_read_target_voltage()
        && battery_voltage > (profile.cell_voltage_absolute_min * profile.num_cells)
        && solar_voltage > battery_voltage + cal.solar_voltage_offset_start
        && time(NULL) > dcdc_off_timestamp + cal.dcdc_restart_interval)
    {
        serial.printf("MPPT start!\n");
        dcdc_start(battery_voltage / (solar_voltage - 3.0));    // TODO: use factor (1V too low!)
        led_red = 1;
    }
    else if (dcdc_enabled() == true &&
        ((solar_voltage < battery_voltage + cal.solar_voltage_offset_stop
        && dcdc_current < cal.dcdc_current_min)
        || charger_charging_enabled() == false))
    {
        serial.printf("MPPT stop!\n");
        dcdc_stop();
        led_red = 0;
        dcdc_off_timestamp = time(NULL);
    }
    else if (dcdc_enabled()) {

        if (battery_voltage > charger_read_target_voltage()
            || dcdc_current > charger_read_target_current()
            || temp_mosfets > 80)
        {
            // increase input voltage --> lower output voltage and decreased current
            dcdc_duty_cycle_step(-1);
        }
        else if (dcdc_current < cal.dcdc_current_min) {
            // very low current --> decrease input voltage to increase current
            pwm_delta = 1;
            dcdc_duty_cycle_step(1);
        }
        else {
            /*
            serial.printf("P: %.2f, P_prev: %.2f, v_bat: %.2f, i_bat: %.2f, v_target: %.2f, i_target: %.2f, PWM: %.1f\n",
                dcdc_power_new, dcdc_power, battery_voltage, dcdc_current, charger_read_target_voltage(), charger_read_target_current(), dcdc_get_duty_cycle() * 100.0);
            */

            // start MPPT
            if (dcdc_power > dcdc_power_new) {
                pwm_delta = -pwm_delta;
            }
            dcdc_duty_cycle_step(pwm_delta);
        }
    }

    dcdc_power = dcdc_power_new;
}

//----------------------------------------------------------------------------
// must be called once per second
void energy_counter()
{
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
        device.input_Wh_day = 0.0;
        device.output_Wh_day = 0.0;
        seconds_day = 0;
        day_counter++;
    }
    else {
        seconds_day++;
    }

    device.input_Wh_day += dcdc_power / 3600.0;
    device.output_Wh_day += load_current * battery_voltage / 3600.0;
    device.input_Wh_total += dcdc_power / 3600.0;
    device.output_Wh_total += load_current * battery_voltage/ 3600.0;
}


// configures IWDG (timeout in seconds)
void init_watchdog(float timeout) {

    #define LSI_FREQ 40000     // approx. 40 kHz according to RM0091
    
    uint16_t prescaler;
    float calculated_timeout;

    IWDG->KR = 0x5555; // Disable write protection of IWDG registers

    // set prescaler
    if ((timeout * (LSI_FREQ/4)) < 0x7FF) {
        IWDG->PR = IWDG_PRESCALER_4;
        prescaler = 4;
    }
    else if ((timeout * (LSI_FREQ/8)) < 0xFF0) {
        IWDG->PR = IWDG_PRESCALER_8;
        prescaler = 8;
    }
    else if ((timeout * (LSI_FREQ/16)) < 0xFF0) {
        IWDG->PR = IWDG_PRESCALER_16;
        prescaler = 16;
    }
    else if ((timeout * (LSI_FREQ/32)) < 0xFF0) {
        IWDG->PR = IWDG_PRESCALER_32;
        prescaler = 32;
    }
    else if ((timeout * (LSI_FREQ/64)) < 0xFF0) {
        IWDG->PR = IWDG_PRESCALER_64;
        prescaler = 64;
    }
    else if ((timeout * (LSI_FREQ/128)) < 0xFF0) {
        IWDG->PR = IWDG_PRESCALER_128;
        prescaler = 128;
    }
    else {
        IWDG->PR = IWDG_PRESCALER_256;
        prescaler = 256;
    }
    
    // set reload value (between 0 and 0x0FFF)
    IWDG->RLR = (uint32_t)(timeout * (LSI_FREQ/prescaler));
    
    calculated_timeout = ((float)(prescaler * IWDG->RLR)) / LSI_FREQ;
    printf("WATCHDOG set with prescaler:%d reload value: 0x%X - timeout:%f\n",prescaler, IWDG->RLR, calculated_timeout);
    
    IWDG->KR = 0xAAAA;  // reload
    IWDG->KR = 0xCCCC;  // start

    feed_the_dog();
}
 
// Reset the watchdog timer
void feed_the_dog()
{
    IWDG->KR = 0xAAAA;
}

// this function is called by mbed if a serious error occured: error() function called
void mbed_die(void)
{
    dcdc_stop();
    load_disable = 1;

    while (1) {
        led_red = 1;
        led_green = 0;
        wait_ms(100);
        led_red = 0;
        led_green = 1;
        wait_ms(100);

        // stay here to indicate something was wrong
        feed_the_dog();
    }
}

#endif
