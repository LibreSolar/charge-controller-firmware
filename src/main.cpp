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
#include "HalfBridge.h"
#include "AnalogValue.h"
#include "ChargeController.h"
//#include "24LCxx_I2C.h"
#include <math.h>     // log for thermistor calculation

#include "output.h"
//#include "output_can.h"

//----------------------------------------------------------------------------
// global variables

Serial serial(PIN_SWD_TX, PIN_SWD_RX, "serial");
CAN can(PIN_CAN_RX, PIN_CAN_TX, CAN_SPEED);

DigitalOut uext_sda(PIN_UEXT_SDA);
DigitalOut uext_scl(PIN_UEXT_SCL);
I2C i2c(PIN_UEXT_SDA, PIN_UEXT_SCL);

DeviceState device;
CalibrationData cal;

ChargingProfile profile;
ChargeController charger(profile);

// PWM will be generated on pins PA8 (TIM1_CH1) and PB13 (TIM1_CH1N)
HalfBridge dcdc(_pwm_frequency);
float dcdc_power;    // saves previous output power
int pwm_delta = 1;
int dcdc_off_timestamp = -cal.dcdc_restart_interval;    // start immediately

DigitalOut led_green(PIN_LED_GREEN);
DigitalOut led_red(PIN_LED_RED);
DigitalOut load_disable(PIN_LOAD_DIS);
DigitalOut can_disable(PIN_CAN_STB);
DigitalOut out_5v_enable(PIN_5V_OUT_EN);
DigitalOut uext_disable(PIN_UEXT_DIS);

AnalogOut ref_i_dcdc(PIN_REF_I_DCDC);
AnalogIn v_ref(PIN_V_REF);

AnalogValue solar_voltage(PIN_V_SOLAR, 1056.0 / 56.0, 4, 0.2); // solar voltage divider o-- 100k --o-- 5.6k --o
AnalogValue battery_voltage(PIN_V_BAT, 110 / 10, 4, 0.2); // battery voltage divider o-- 100k --o-- 10k --o
AnalogValue dcdc_current(PIN_I_DCDC, 1000 / 2 / (1500.0 / 22.0), 4, 0.3); // op amp gain: 150/2.2 = 68.2, resistor: 2 mOhm
AnalogValue load_current(PIN_I_LOAD, 1000 / 2 / (1500.0 / 22.0));
AnalogValue thermistor_mosfets(PIN_TEMP_INT, 1.0);
AnalogValue thermistor_battery(PIN_TEMP_BAT, 1.0);
float temp_mosfets; // °C

//C24LCXX_I2C eeprom(PIN_EEPROM_SDA, PIN_EEPROM_SCL, 0x07);

// for daily energy counting
int seconds_zero_solar = 0;
int seconds_day = 0;
int day_counter = 0;
bool nighttime = false;

//----------------------------------------------------------------------------
// function prototypes

void setup();
void update_measurements();
void energy_counter();
void update_mppt();

//----------------------------------------------------------------------------
int main()
{
    Ticker tick1, tick2, tick3;

    setup();
    wait(3);    // safety feature: be able to re-flash before starting

    time_t last_second = time(NULL);

    //tick1.attach(&update_measurements, 0.01);     // too often, may cause crash of MCU
    //tick2.attach(&update_mppt, 0.2);
    dcdc.start(0.9);  // for testing only (don't run update_mppt!)

    //tick3.attach(&can_send_data, 1);

    int count_10s = 0;

    while(1) {

        update_measurements();

        //can_process_outbox();
        //can_process_inbox();

        // called once per second
        if (time(NULL) - last_second >= 1) {
            last_second = time(NULL);
            count_10s++;

            serial.printf("Still alive... time: %d\n", time(NULL));
            //serial.printf("Clock speed: %d\n", SystemCoreClock);

            // updates charger state machine
            charger.update(battery_voltage.read(), dcdc_current.read());

            // load management
            if (charger.discharging_enabled()) {
                load_disable = 0;
            } else {
                load_disable = 1;
            }

            //can_send_data();

            energy_counter();

            device.bus_voltage = battery_voltage;
            device.input_voltage = solar_voltage;
            device.bus_current = dcdc_current;
            device.input_current = dcdc_power / solar_voltage;
            device.load_current = load_current;
            device.external_temperature = temp_mosfets;
            device.internal_temperature = temp_mosfets;
            device.load_enabled = !load_disable;

            #ifdef WIFI_ENABLED
                if (count_10s >= 10) {
                    count_10s = 0;
                    //output_wifi(device, charger, dcdc);
                }
            #else
                //output_serial_json();
                output_oled();
            #endif

            fflush(stdout);
        }
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
    //uext_disable = 0;
    ref_i_dcdc = 0.1;         // 0 for buck, 1 for boost (TODO)

    // adjust default values for 12V LiFePO4 battery
    profile.num_cells = 4;
    profile.cell_voltage_max = 3.55;        // max voltage per cell
    profile.cell_voltage_recharge = 3.35;
    profile.trickle_enabled = false;
    profile.cell_voltage_load_disconnect = 3.0;
    profile.cell_voltage_load_reconnect  = 3.15;

    dcdc.deadtime_ns(300);
    dcdc.lock_settings();
    dcdc.duty_cycle_limits((float) profile.cell_voltage_load_disconnect * profile.num_cells / cal.solar_voltage_max, 0.97);

    wait(0.2);
    update_measurements();

    dcdc_current.calibrate_offset(0.0);
    load_current.calibrate_offset(0.0);

    //serial.baud(9600);
    serial.baud(115200);
    serial.printf("\nSerial interface started...\n");
    freopen("/serial", "w", stdout);  // retarget stdout

    i2c.frequency(400000);


    //can.mode(CAN::Normal);
    //can.attach(&can_receive);

    // TXFP: Transmit FIFO priority driven by request order (chronologically)
    // NART: No automatic retransmission
    //CAN1->MCR |= CAN_MCR_TXFP | CAN_MCR_NART;
}

void update_mppt()
{
    float dcdc_power_new = battery_voltage * dcdc_current;

    if (dcdc.enabled()) {
        serial.printf("B: %.2fV, %.2fA, S: %.2fV, Pwr: %.2fW, Charger: %d, DCDC: %d, PWM: %.1f\n",
            battery_voltage.read(), dcdc_current.read(), solar_voltage.read(), dcdc_power_new,
            charger.get_state(), dcdc.enabled(), dcdc.get_duty_cycle() * 100.0);
    }

    if (dcdc.enabled() == false && charger.charging_enabled() == true
        && battery_voltage < charger.read_target_voltage()
        && battery_voltage > (profile.cell_voltage_absolute_min * profile.num_cells)
        && solar_voltage > battery_voltage + cal.solar_voltage_offset_start
        && time(NULL) > dcdc_off_timestamp + cal.dcdc_restart_interval)
    {
        serial.printf("MPPT start!\n");
        dcdc.start(battery_voltage / (solar_voltage - 3.0));    // TODO: use factor (1V too low!)
        led_red = 1;
    }
    else if (dcdc.enabled() == true &&
        ((solar_voltage < battery_voltage + cal.solar_voltage_offset_stop
        && dcdc_current < cal.dcdc_current_min)
        || charger.charging_enabled() == false))
    {
        serial.printf("MPPT stop!\n");
        dcdc.stop();
        led_red = 0;
        dcdc_off_timestamp = time(NULL);
    }
    else if (dcdc.enabled()) {

        if (battery_voltage > charger.read_target_voltage()
            || dcdc_current > charger.read_target_current()
            || temp_mosfets > 80)
        {
            // increase input voltage --> lower output voltage and decreased current
            dcdc.duty_cycle_step(-1);
        }
        else if (dcdc_current < cal.dcdc_current_min) {
            // very low current --> decrease input voltage to increase current
            pwm_delta = 1;
            dcdc.duty_cycle_step(1);
        }
        else {
            //serial.printf("P: %.2f, P_prev: %.2f, v_bat: %.2f, i_bat: %.2f, v_target: %.2f, i_target: %.2f, PWM: %.1f\n",
            //    dcdc_power_new, dcdc_power, battery_voltage.read(), dcdc_current.read(), charger.read_target_voltage(), charger.read_target_current(), dcdc.get_duty_cycle() * 100.0);

            // start MPPT
            if (dcdc_power > dcdc_power_new) {
                pwm_delta = -pwm_delta;
            }
            dcdc.duty_cycle_step(pwm_delta);
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


//----------------------------------------------------------------------------
void update_measurements()
{
#ifdef MPPT_20A
    //AnalogValue::update_reference_voltage(v_ref, 2.5);
#endif

    solar_voltage.update();
    battery_voltage.update();
    dcdc_current.update();
    load_current.update();

    thermistor_mosfets.update();

    // resistance of NTC (Ohm)
    unsigned long rts = 10000.0 * thermistor_mosfets.read() / (3.3 - thermistor_mosfets.read());
    // Temperature calculation using Beta equation for 10k thermistor
    // (25°C reference temperature for Beta equation assumed)
    temp_mosfets = 1.0/(1.0/(273.15+25) + 1.0/cal.thermistorBetaValue*log(rts/10000.0)) - 273.15;
}

#endif
