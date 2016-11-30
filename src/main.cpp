 /* LibreSolar MPPT charge controller firmware
  * Copyright (c) 2016 Martin Jäger (www.libre.solar)
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

#include "mbed.h"
#include "HalfBridge.h"
#include "PID.h"
#include "DogLCD.h"
#include "font_6x8.h"
#include "N3310LCD.h"

#define NOKIALCD_ENABLED        // select DOGLCD or NOKIALCD
//#define DOGLCD_ENABLED        // select DOGLCD or NOKIALCD
#define ADC_AVG_SAMPLES 8       // number of ADC values to read for averaging
#define MPPT_SAFE_MODE 1

// DC/DC converter settings
const int _pwm_frequency = 100; // kHz
const int solar_voltage_offset_start = 5000; // mV  charging switched on if Vsolar > Vbat + offset
const int solar_voltage_offset_stop = 1000; // mV  charging switched on if Vsolar > Vbat + offset
const int max_charge_current = 10000;  // mA        PCB maximum: 12 A
const int charge_current_cutoff = 20;  // mA       --> if lower, charger is switched off
const int max_cv_chg_voltage_offset = 100;  // mV   max voltage offset vs. vmax to switch back into CC mode
const int max_solar_voltage = 55000; // mV

// typcial lead-acid battery
const int num_cells = 6;
const int v_cell_max = 2400;        // max voltage per cell
const int v_cell_trickle = 2300;    // voltage for trickle charging of lead-acid batteries
const int v_cell_recharge = 2200;   // voltage to switch from CV or trickle back to CC charging
const int v_cell_load_disconnect = 1950;
const int v_cell_load_reconnect  = 2200;

/*
// typcial LiFePO4 battery
const int num_cells = 4;
const int v_cell_max = 3550;        // max voltage per cell
const int v_cell_trickle = 3550;    // voltage for trickle charging of lead-acid batteries
const int v_cell_load_disconnect = 3000;
const int v_cell_load_reconnect  = 3150;
*/

//----------------------------------------------------------------------------
// global variables


#ifdef DOGLCD_ENABLED
SPI spi(PB_5, NC, PB_3);
DogLCD lcd(spi, PA_1, PA_3, PA_2); //  spi, cs, a0, reset
#endif
#ifdef NOKIALCD_ENABLED
//           MOSI, MISO, SCK,  CE,   DC,   RST,  BL
N3310LCD lcd(PB_5, NC,   PB_3, PA_1, PB_7, PB_6, NC);
#endif

Serial serial(PB_10, PB_11);

HalfBridge mppt(PA_8, PB_13);

DigitalOut led1(PA_9);
DigitalOut led2(PA_10);
DigitalOut led3(PB_12);
DigitalOut load_disable(PB_2);

AnalogIn v_bat(PA_4);
AnalogIn v_solar(PA_5);
AnalogIn i_load(PA_6);
AnalogIn i_dcdc(PA_7);

Ticker tick1, tick2, tick3;

int solar_voltage;      // mV
int battery_voltage;    // mV
int dcdc_current;       // mA
int load_current;       // mA

int dcdc_current_target = 0;
int solar_power_prev = 0;   // unit: mW
int solar_power = 0;        // unit: mW

bool battery_full = false;

// for daily energy counting
long seconds_zero_solar = 0;
bool nighttime = false;
float input_Wh_day = 0.0;
float output_Wh_day = 0.0;
float input_Wh_total = 0.0;
float output_Wh_total = 0.0;

int pwm_delta = 1;

enum charger_states {STANDBY, CHG_CC, CHG_CV, CHG_TRICKLE, BAT_ERROR, CHG_BIKE, TEST};     // states for charger state machine
int state = BAT_ERROR;

//----------------------------------------------------------------------------
// function prototypes

void setup();
void update_measurements(bool init = false);
void update_controller();
void enter_state(int next_state);
void state_machine();
void adjust_input_voltage(int delta_mV);
void update_screen();
void energy_counter();

//----------------------------------------------------------------------------
int main()
{
    // boot from System Memory where USB DFU bootloader is located
    //OB->USER |= 1 << 4;

    load_disable = 0;
    setup();

    enter_state(STANDBY);
    //enter_state(TEST);

    tick1.attach(&update_screen, 0.5);

    //tick2.attach(&update_controller, PID_UPDATE_RATE);
    tick2.attach(&state_machine, 0.1);

    tick3.attach(&energy_counter, 1);

    while(1) {
        update_measurements();
        //state_machine();

        if (battery_voltage < v_cell_load_disconnect * num_cells) {
            load_disable = 1;
        }
        if (battery_voltage >= v_cell_load_reconnect * num_cells) {
            load_disable = 0;
        }

        wait(0.01);
    }
}

//----------------------------------------------------------------------------
void setup()
{
    #ifdef NOKIALCD_ENABLED
        lcd.init();
        lcd.cls();
    #endif
    #ifdef DOGLCD_ENABLED
        lcd.init();
        lcd.view(VIEW_TOP);
    #endif

    mppt.frequency_kHz(_pwm_frequency);
    mppt.deadtime_ns(300);
    mppt.set_duty_cycle(0.90);
    mppt.lock_settings();

    mppt.duty_cycle_limits((float) v_cell_load_disconnect * num_cells / max_solar_voltage, 0.97);

    update_measurements(true);  // initial setting of measurement values

    serial.baud(115200);
    serial.printf("\nSerial interface started...\n");
}

// actions to be performed upon entering a state
void enter_state(int next_state)
{
    switch (next_state)
    {
        case STANDBY:
            serial.printf("Enter State: STANDBY\n");
            mppt.stop();
            led1 = 1;
            led3 = 0;
            break;

        case CHG_CC:
            serial.printf("Enter State: CHG_CC\n");
            mppt.set_duty_cycle((float) battery_voltage / (solar_voltage - 1000.0)); // start with proper PWM duty cycle
            mppt.start();
            led3 = 1;
            break;

        case CHG_CV:
            serial.printf("Enter State: CHG_CV\n");
            // coming from CHG_CC --> DC/DC FETs already on and charging enabled
            break;

        case CHG_TRICKLE:
            serial.printf("Enter State: CHG_TRICKLE\n");
            break;

        case TEST:
            serial.printf("Enter State: TEST\n");
            mppt.set_duty_cycle(0.9);
            mppt.start();
            led3 = 1;
            break;

        case BAT_ERROR:
            serial.printf("Enter State: BAT_ERROR\n");
            mppt.stop();
            led3 = 0;
            break;

        default:
            mppt.stop();
            led3 = 0;
            break;
    }
    state = next_state;
}

/*****************************************************************************
 *  Charger state machine
 *
 *  INIT       Initial state when system boots up. Checks communication to BMS
 *             and turns the CHG and DSG FETs on
 *  STANDBY    Battery can be discharged, but no charging power is available
 *             or battery is fully charged
 *  CHG_CC     Constant current (CC) charging with maximum current possible
 *             (MPPT algorithm is running)
 *  CHG_CV     Constant voltage (CV) charging at maximum cell voltage
 *  BAT_ERROR  An error occured. Charging disabled.

 */
void state_machine()
{
    switch (state) {

        case STANDBY:
            if  (solar_voltage > battery_voltage + solar_voltage_offset_start
                && battery_voltage < num_cells * v_cell_recharge) {
                //enter_state(CHG_BIKE);
                enter_state(CHG_CC);
            }
            break;


        case CHG_CC:
            //serial.printf("CHG_CC state still alive\n");
            if (battery_voltage > num_cells * v_cell_max) {
                enter_state(CHG_CV);
            }
            else if (dcdc_current > max_charge_current) {
                // increase PV voltage
                mppt.duty_cycle_step(-1);
            }
            else if (dcdc_current < charge_current_cutoff ||
                solar_voltage < battery_voltage + solar_voltage_offset_stop) {
                //enter_state(BAT_ERROR);
                enter_state(STANDBY);
                return;
            }
            else { // start MPPT

                if (solar_power_prev > solar_power) {
                    pwm_delta = -pwm_delta;
                }
                mppt.duty_cycle_step(pwm_delta);
            }
            solar_power_prev = solar_power;
            break;

        case CHG_CV:

            if (dcdc_current < charge_current_cutoff ||
                solar_voltage < battery_voltage + solar_voltage_offset_stop) {
                // battery charging finished?
                if (battery_voltage > (num_cells * v_cell_max - max_cv_chg_voltage_offset)) {
                    // TODO: define different current and time cut off (compare to Victron manual)
                    enter_state(CHG_TRICKLE);
                }
                else {
                    enter_state(STANDBY);
                }
                return;
            }
            else if (dcdc_current > max_charge_current) {
                // increase PV voltage
                mppt.duty_cycle_step(-1);
            }
            else if (battery_voltage < (num_cells * v_cell_max - max_cv_chg_voltage_offset)) {
                enter_state(CHG_CC);
            }
            else {
                if (battery_voltage > num_cells * v_cell_max) {
                    // increase input voltage and decrease PV current
                    mppt.duty_cycle_step(-1);
                }
                else  {
                    mppt.duty_cycle_step(1);
                }
            }
            break;

        case CHG_TRICKLE:

            if (dcdc_current < charge_current_cutoff ||
                solar_voltage < battery_voltage + solar_voltage_offset_stop) {
                // current cutoff will probably never be reached because of
                // leakage current inside battery --> stay in trickle till end of day
                enter_state(STANDBY);
                return;
            }
            else if (dcdc_current > max_charge_current) {
                // increase PV voltage
                mppt.duty_cycle_step(-1);
            }
            else if (battery_voltage < (num_cells * v_cell_max - max_cv_chg_voltage_offset)) {
                enter_state(CHG_CC);
            }
            else {
                if (battery_voltage > num_cells * v_cell_trickle) {
                    // increase input voltage and decrease PV current
                    mppt.duty_cycle_step(-1);
                }
                else  {
                    mppt.duty_cycle_step(1);
                }
            }
            break;

        case BAT_ERROR:
            //if (err == 0) {
            //    enter_state(STANDBY);
            //}
            break;

        case TEST:

            break;

        default:
            mppt.stop();
            break;
    }
}


//----------------------------------------------------------------------------
// must be called once per second
void energy_counter()
{
    if (solar_power < 100) {
        seconds_zero_solar++;
    }
    else {
        seconds_zero_solar = 0;
    }

    // detect night time for reset of daily counter in the morning
    if (seconds_zero_solar > 60*60*5) {
        nighttime = true;
    }

    if (nighttime && solar_power > 100) {
        nighttime = false;
        input_Wh_day = 0.0;
        output_Wh_day = 0.0;
    }

    input_Wh_day += solar_power / 1000.0 / 3600.0;
    output_Wh_day += load_current / 1000.0 * battery_voltage / 1000.0 / 3600.0;
    input_Wh_total += solar_power / 1000.0 / 3600.0;
    output_Wh_total += load_current / 1000.0 * battery_voltage / 1000.0 / 3600.0;
}

#ifdef DOGLCD_ENABLED
void update_screen(void)
{
    char str[20];

    lcd.clear_screen();

    sprintf(str, "V Sol: %.2fV", solar_voltage / 1000.0);
    lcd.string(0,0,font_6x8, str);

    sprintf(str, "V Bat: %.2fV", battery_voltage / 1000.0);
    lcd.string(0,1,font_6x8, str);

    sprintf(str, "I DCDC %.2fA", dcdc_current / 1000.0);
    lcd.string(0,2,font_6x8, str);

    sprintf(str, "I Load: %.2fA", load_current / 1000.0);
    lcd.string(0,3,font_6x8, str);

    sprintf(str, "Power: %.2f W", solar_power / 1000.0);
    lcd.string(0,4,font_6x8, str);

    sprintf(str, "PWM duty: %.1f %%", mppt.get_duty_cycle() * 100.0);
    lcd.string(0,5,font_6x8, str);

    lcd.string(0,6,font_6x8,"State:");
    switch (state) {
        case STANDBY:
            lcd.string(7*6,6,font_6x8,"Standby");
            break;
        case CHG_CC:
            lcd.string(7*6,6,font_6x8,"CC charge");
            break;
        case CHG_CV:
            lcd.string(7*6,6,font_6x8,"CV charge");
            break;
        case CHG_TRICKLE:
            lcd.string(7*6,6,font_6x8,"Trickle chg");
            break;
        case CHG_BIKE:
            lcd.string(7*6,6,font_6x8,"Bike gen");
            break;
        case TEST:
            lcd.string(7*6,6,font_6x8,"Test mode");
            break;
    }

    //sprintf(str, "Temp: %.1f C", temperature);
    //lcd.string(0,7,font_6x8, str);
}
#endif // DISPLAY_ENABLED

#ifdef NOKIALCD_ENABLED
void update_screen(void)
{
    char str[20];

    lcd.cls();

    sprintf(str, "S %.2fV %.1fW ", solar_voltage / 1000.0, solar_power / 1000.0);
    lcd.writeString(0, 1, str, NORMAL);

    sprintf(str, "B %.2fV %.1fA ", battery_voltage / 1000.0, dcdc_current / 1000.0);
    lcd.writeString(0, 2, str, NORMAL);

    if (load_disable) {
        sprintf(str, "L cut off");
        lcd.writeString(0, 3, str, NORMAL);
    }
    else {
        sprintf(str, "L %.1fW %.2fA ", load_current / 1000.0 * battery_voltage / 1000.0, load_current / 1000.0);
        lcd.writeString(0, 3, str, NORMAL);
    }

    sprintf(str, "D %.0fWh -%.0fWh ", input_Wh_day, output_Wh_day);
    lcd.writeString(0, 4, str, NORMAL);

    sprintf(str, "T %.1fk -%.1fk ", input_Wh_total / 1000.0, output_Wh_total / 1000.0);
    lcd.writeString(0, 5, str, NORMAL);

    switch (state) {
        case STANDBY:
            sprintf(str, "Standby");
            break;
        case CHG_CC:
            sprintf(str, "CC charge");
            break;
        case CHG_CV:
            sprintf(str, "CV charge");
            break;
        case CHG_TRICKLE:
            sprintf(str, "Trickle chg");
            break;
        case CHG_BIKE:
            sprintf(str, "Bike gen");
            break;
        default:
            sprintf(str, "Error");
            break;
    }
    lcd.writeString(0, 6, str, NORMAL);

}
#endif // NOKIALCD


//----------------------------------------------------------------------------
void update_measurements(bool init)
{
    const int vcc = 3300;     // mV
    int temp = 0;

    // low pass filter of measurement values:
    // y(n) = (1-c)*x(n) + c*y(n-1)    with c=exp(-Ta/RC)
    float c = 0.6;      // filter constant

    // otherwise load will be directly disconnected
    if (init == true) {
        c = 0.0;
    }

    // solar voltage divider o-- 150k --o-- 6.8k --o
    unsigned long sum_adc_readings = 0;
    for (int i = 0; i < ADC_AVG_SAMPLES; i++) {
        sum_adc_readings += v_solar.read_u16();
    }
    temp = (sum_adc_readings / ADC_AVG_SAMPLES) * 1568 / 68 * vcc / 0xFFFF;

    solar_voltage = (1.0 - c) * temp + c * solar_voltage;

    // battery voltage divider o-- 120k --o-- 10k --o
    sum_adc_readings = 0;
    for (int i = 0; i < ADC_AVG_SAMPLES; i++) {
        sum_adc_readings += v_bat.read_u16();
    }
    temp = (sum_adc_readings / ADC_AVG_SAMPLES) * 130 / 10 * vcc / 0xFFFF;
    battery_voltage = (1.0 - c) * temp + c * battery_voltage;

    sum_adc_readings = 0;
    for (int i = 0; i < ADC_AVG_SAMPLES; i++) {
        sum_adc_readings += i_dcdc.read_u16();
    }
    temp = (sum_adc_readings / ADC_AVG_SAMPLES) * 1000 / 5 / 50 * vcc / 0xFFFF;
    dcdc_current = (1.0 - c) * temp + c * dcdc_current;

    sum_adc_readings = 0;
    for (int i = 0; i < ADC_AVG_SAMPLES; i++) {
        sum_adc_readings += i_load.read_u16();
    }
    temp = (sum_adc_readings / ADC_AVG_SAMPLES) * 1000 / 5 / 50 * vcc / 0xFFFF;
    load_current = (1.0 - c) * temp + c * load_current;

    solar_power = battery_voltage * dcdc_current / 1000;
}
