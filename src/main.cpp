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

#define DISPLAY_ENABLED
#define ADC_AVG_SAMPLES 8       // number of ADC values to read for averaging
#define MPPT_SAFE_MODE 1

// DC/DC converter settings
const int _pwm_frequency = 100; // kHz
const int min_solar_current = 100;  // mA    --> if lower, charger is switched off
const int solar_voltage_offset_start = 4000; // mV  charging switched on if Vsolar > Vbat + offset
const int solar_voltage_offset_stop = 2000; // mV  charging switched off if Vsolar < Vbat + offset
const int max_charge_current = 8000;  // mA       PCB maximum: 12 A
const int max_solar_voltage = 55000; // mV

const int k_pedal_generator = 5; // W/V bezogen auf Spannungsdifferenz zwischen Eingang und Ausgang

// typcial lead-acid battery
/*
const int num_cells = 6;
const int v_cell_max = 2400;        // max voltage per cell
const int v_cell_trickle = 2300;    // voltage for trickle charging of lead-acid batteries
const int v_cell_load_disconnect = 1950;
const int v_cell_load_reconnect  = 2200;
*/

// typcial LiFePO4 battery
const int num_cells = 4;
const int v_cell_max = 3550;        // max voltage per cell
const int v_cell_trickle = 3550;    // voltage for trickle charging of lead-acid batteries
const int v_cell_load_disconnect = 3000;
const int v_cell_load_reconnect  = 3150;

//----------------------------------------------------------------------------
// global variables

SPI spi(PB_5, NC, PB_3);

#ifdef DISPLAY_ENABLED
DogLCD lcd(spi, PA_1, PA_3, PA_2); //  spi, cs, a0, reset
#endif

Serial serial(PB_10, PB_11);

HalfBridge mppt(PA_8, PB_13);

DigitalOut led1(PA_9);
DigitalOut led2(PA_10);
DigitalOut led3(PB_12);

AnalogIn v_bat(PA_4);
AnalogIn v_solar(PA_5);
AnalogIn i_load(PA_6);
AnalogIn i_dcdc(PA_7);

Ticker tick1, tick2;

// ST HAL for ADC for internal temperature and voltage measurements
ADC_HandleTypeDef adc_handle;

//PID(float Kc, float tauI, float tauD, float interval)
#define PID_UPDATE_RATE 0.01
PID controller(-2e-4, 3e-3, 0, PID_UPDATE_RATE);
//PID controller(2, 0, 0, PID_UPDATE_RATE);

int solar_voltage;      // mV
int battery_voltage;    // mV
int dcdc_current;       // mA
int load_current;       // mA

float temperature = 0.0;


int dcdc_current_target = 0;
int solar_power_prev = 0;   // unit: mW
int solar_power = 0;        // unit: mW

int pwm_delta = 1;

enum charger_states {STANDBY, CHG_CC, CHG_CV, CHG_TRICKLE, BAT_ERROR, CHG_BIKE};     // states for charger state machine
int state = BAT_ERROR;

//----------------------------------------------------------------------------
// function prototypes

void setup();
void update_measurements();
void update_controller();
void enter_state(int next_state);
void state_machine();
void adjust_input_voltage(int delta_mV);

#ifdef DISPLAY_ENABLED
void update_screen();
#endif


//----------------------------------------------------------------------------
int main()
{
    setup();

    enter_state(STANDBY);

#ifdef DISPLAY_ENABLED
    tick1.attach(&update_screen, 0.5);
#endif

    //tick2.attach(&update_controller, PID_UPDATE_RATE);

    while(1) {
        update_measurements();
        state_machine();
        wait(0.2);
    }
}

//----------------------------------------------------------------------------
void setup()
{
#ifdef DISPLAY_ENABLED
    lcd.init();
    lcd.view(VIEW_TOP);
#endif

    mppt.frequency_kHz(_pwm_frequency);
    mppt.deadtime_ns(300);
    mppt.set_duty_cycle(0.90);
    mppt.lock_settings();

    mppt.duty_cycle_limits(0.5, 0.97);
//    mppt.duty_cycle_limits((float) v_cell_load_disconnect * num_cells / max_solar_voltage, 0.97);

    update_measurements();


    controller.setInputLimits(0.0, 12000);
    controller.setOutputLimits(13000, 55000);
    //controller.setBias(0.0);
    controller.setMode(AUTO_MODE);

    serial.baud(115200);
    serial.printf("\nSerial interface started...\n");

    // Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
    adc_handle.Instance = ADC1;
    adc_handle.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV4;
    adc_handle.Init.Resolution = ADC_RESOLUTION12b;
    adc_handle.Init.ScanConvMode = DISABLE;
    adc_handle.Init.ContinuousConvMode = DISABLE;
    adc_handle.Init.DiscontinuousConvMode = DISABLE;
    adc_handle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    adc_handle.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    adc_handle.Init.DMAContinuousRequests = DISABLE;
    adc_handle.Init.EOCSelection = EOC_SINGLE_CONV;
    HAL_ADC_Init(&adc_handle);   //Go turn on the ADC
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

        case CHG_BIKE:
            serial.printf("Enter State: CHG_BIKE\n");
            //if (solar_voltage < battery_voltage)
            //    solar_voltage = battery_voltage + solar_voltage_offset;
            mppt.set_duty_cycle((float) battery_voltage / (solar_voltage - 1000.0)); // start with proper PWM duty cycle
            //mppt.start();
            //led3 = 1;
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
    int err = 0, p_target;

    switch (state) {

        case STANDBY:
            if  (solar_voltage > battery_voltage + solar_voltage_offset_start) {
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
            if (solar_voltage < battery_voltage + solar_voltage_offset_stop
                || dcdc_current < min_solar_current) {
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

            if (dcdc_current < min_solar_current) {
                enter_state(STANDBY);
                return;
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

        case CHG_BIKE:
            p_target = ((solar_voltage - battery_voltage) / 100.0) * ((solar_voltage - battery_voltage) / 100.0) * k_pedal_generator;  // mW
            //serial.printf("P_target = %d, p_actual = %d\n", p_target, solar_power);
            if (p_target < 0) {
                p_target = 0;
            }

            dcdc_current_target = (float) p_target / battery_voltage * 1000.0;

            if (solar_power > p_target) {
                // increase input voltage and decrease motor torque
                //mppt.set_duty_cycle(mppt.get_duty_cycle() - 0.001);
            }
            else {
                //mppt.set_duty_cycle(mppt.get_duty_cycle() + 0.001);
            }
            break;


        case BAT_ERROR:
            if (err == 0) {
                enter_state(STANDBY);
            }
            break;

        default:
            mppt.stop();
            break;
    }
}


//----------------------------------------------------------------------------

void adjust_input_voltage(int delta_mV)
{
    float duty_cycle = mppt.get_duty_cycle();

    duty_cycle = (float)battery_voltage / (battery_voltage / duty_cycle + delta_mV);

    //if (sqrt(pow(duty_cycle - mppt.get_duty_cycle(), 2.0)) < 1) {
        serial.printf("New duty cycle: %.1f %%\n", duty_cycle * 100.0);
        mppt.set_duty_cycle(duty_cycle);
    //}
}


//----------------------------------------------------------------------------

void update_controller(void)
{
    int input_voltage;

    //dcdc_current_target = 300;
//    dcdc_current = 1100;

    //We want the process variable to be 1.7V
    controller.setSetPoint((float)dcdc_current_target);
//    controller.setSetPoint(800);

    // update the process variable.
    controller.setProcessValue((float)dcdc_current);
//    controller.setProcessValue(1);

    // set the new output.
    input_voltage = (int)controller.compute();

    mppt.set_duty_cycle((float) battery_voltage / input_voltage);

//    serial.printf("Controller: actual = %d, target = %d, output = %d, duty = %f\n", dcdc_current, dcdc_current_target, input_voltage, (float) battery_voltage / input_voltage);
//    serial.printf("Controller: actual = %d, target = %d, output = %d\n", dcdc_current, dcdc_current_target, input_voltage);

}

#ifdef DISPLAY_ENABLED
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
    }

    sprintf(str, "Temp: %.1f C", temperature);
    lcd.string(0,7,font_6x8, str);
}
#endif // DISPLAY_ENABLED


//----------------------------------------------------------------------------
void update_measurements(void)
{
    const int vcc = 3300;     // mV

    // solar voltage divider o-- 150k --o-- 6.8k --o
    unsigned long sum_adc_readings = 0;
    for (int i = 0; i < ADC_AVG_SAMPLES; i++) {
        sum_adc_readings += v_solar.read_u16();
    }
    solar_voltage = (sum_adc_readings / ADC_AVG_SAMPLES) * 1568 / 68 * vcc / 0xFFFF;

    // battery voltage divider o-- 120k --o-- 10k --o
    sum_adc_readings = 0;
    for (int i = 0; i < ADC_AVG_SAMPLES; i++) {
        sum_adc_readings += v_bat.read_u16();
    }
    battery_voltage = (sum_adc_readings / ADC_AVG_SAMPLES) * 130 / 10 * vcc / 0xFFFF;

    sum_adc_readings = 0;
    for (int i = 0; i < ADC_AVG_SAMPLES; i++) {
        sum_adc_readings += i_dcdc.read_u16();
    }
    dcdc_current = (sum_adc_readings / ADC_AVG_SAMPLES) * 1000 / 5 / 50 * vcc / 0xFFFF;

    sum_adc_readings = 0;
    for (int i = 0; i < ADC_AVG_SAMPLES; i++) {
        sum_adc_readings += i_load.read_u16();
    }
    load_current = (sum_adc_readings / ADC_AVG_SAMPLES) * 1000 / 5 / 50 * vcc / 0xFFFF;

    solar_power = battery_voltage * dcdc_current / 1000;

    // Trying internal temperature and voltage reference
    // Results:
    //   - LDO is more accurate than internal voltage reference
    //   - Temperature reading very inaccurate (>40°C at 25°C)

    // read factory calibration values
    const uint16_t VREFINT_CAL = *((uint16_t *)0x1FFFF7BA); // 2 Byte at this address is VRefInt @3.3V/30°C
    const uint16_t TS_CAL1 = *((uint16_t *)0x1FFFF7B8);
    const uint16_t TS_CAL2 = *((uint16_t *)0x1FFFF7C2);

    //serial.printf("TS_CAL1 = %d, TS_CAL2 = %d\n", TS_CAL1, TS_CAL2);

    // ST HAL for internal voltage and temperature measurements

    ADC_ChannelConfTypeDef sConfig;  //Declare the ST HAL ADC object
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    sConfig.SamplingTime = ADC_SAMPLETIME_7CYCLES_5;

    adc_handle.Instance->CHSELR = 0;

    sConfig.Channel = ADC_CHANNEL_VREFINT;
    HAL_ADC_ConfigChannel(&adc_handle, &sConfig);
    HAL_ADC_Start(&adc_handle); // Start conversion

    // Wait end of conversion and get value
    if (HAL_ADC_PollForConversion(&adc_handle, 10) == HAL_OK) {
        uint16_t value = HAL_ADC_GetValue(&adc_handle);
        //vcc = (float) VREFINT_CAL / value * 3.3;
        //serial.printf("VCC = %f\n", vcc);
    }

    adc_handle.Instance->CHSELR = 0;

    sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
    HAL_ADC_ConfigChannel(&adc_handle, &sConfig);
    //ADC->CCR |= ADC_CCR_TSEN;
    HAL_ADC_Start(&adc_handle); // Start conversion

    // Wait end of conversion and get value
    if (HAL_ADC_PollForConversion(&adc_handle, 10) == HAL_OK) {
        uint16_t value = HAL_ADC_GetValue(&adc_handle);
        temperature = (float) (30.0 + (110.0 - 30.0) / (TS_CAL2 - TS_CAL1) * (value - TS_CAL1));
        //serial.printf("Internal Temperature = %f\n", temperature);
    }
}
