 /* LibreSolar MPPT charge controller firmware
  * Copyright (c) 2016-2017 Martin Jäger (www.libre.solar)
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
//#include "config_12a.h"     // choose old PCB revision (12A)
#include "config.h"     // choose new PCB revision (20A)
#include "BuckConverter.h"
//#include "DogLCD.h"
//#include "font_6x8.h"
#include "Adafruit_SSD1306.h"
//#include "SDFileSystem.h"
#include <math.h>     // log for thermistor calculation
#include "display.h"

//#define DOGLCD_ENABLED
//#define SDCARD_ENABLED
#define OLED_ENABLED

#define ADC_AVG_SAMPLES 8       // number of ADC values to read for averaging

//----------------------------------------------------------------------------
// global variables

#ifdef DOGLCD_ENABLED
SPI spi(PIN_UEXT_MOSI, NC, PIN_UEXT_SCK);
DogLCD lcd(spi, PIN_UEXT_SSEL, PIN_UEXT_RX, PIN_UEXT_TX); //  spi, CS, CD, reset
#endif

#ifdef SDCARD_ENABLED
SDFileSystem sd(PIN_UEXT_MOSI, PIN_UEXT_MISO, PIN_UEXT_SCK, PIN_UEXT_SSEL, "sd");
#endif

#ifdef OLED_ENABLED
I2C i2c(PIN_UEXT_SDA, PIN_UEXT_SCL);
Adafruit_SSD1306_I2c oled(i2c, PB_2, 0x78, 64, 128);
#endif

Serial serial(PIN_SWD_TX, PIN_SWD_RX, "serial");
Serial serial_uext(PIN_UEXT_TX, PIN_UEXT_RX, "serial_uext");

// PWM will be generated on pins PA8 (TIM1_CH1) and PB13 (TIM1_CH1N)
BuckConverter mppt(_pwm_frequency);

DigitalOut led_green(PIN_LED_GREEN);
DigitalOut led_red(PIN_LED_RED);
DigitalOut load_disable(PIN_LOAD_DIS);

//AnalogOut ref_i_dcdc(PIN_REF_I_DCDC);

AnalogIn v_ref(PIN_V_REF);
AnalogIn v_bat(PIN_V_BAT);
AnalogIn v_solar(PIN_V_SOLAR);
AnalogIn v_temp(PIN_V_TEMP);
AnalogIn i_load(PIN_I_LOAD);
AnalogIn i_dcdc(PIN_I_DCDC);

int solar_voltage;      // mV
int battery_voltage;    // mV
int dcdc_current = 0;       // mA
int load_current;       // mA
float temperature;       // mA
int solar_power = 0;        // unit: mW

int dcdc_current_offset = 0;
int load_current_offset = 0;

int time_state_changed = 0;

// for daily energy counting
int seconds_zero_solar = 0;
int seconds_day = 0;
int day_counter = 0;
bool nighttime = false;
float input_Wh_day = 0.0;
float output_Wh_day = 0.0;
float input_Wh_total = 0.0;
float output_Wh_total = 0.0;

int pwm_delta = 1;

enum charger_states {STANDBY, CHG_CC, CHG_CV, CHG_TRICKLE, CHG_EQUALIZATION, BAT_ERROR, CHG_BIKE, TEST};     // states for charger state machine
int state = BAT_ERROR;

//----------------------------------------------------------------------------
// function prototypes

void setup();
void update_measurements();
void update_controller();
void enter_state(int next_state);
void state_machine();
void adjust_input_voltage(int delta_mV);
void update_screen();
void energy_counter();
void load_management();
void log_data();
void rtc_write_backup_reg(uint32_t BackupRegister, uint32_t data);
uint32_t rtc_read_backup_reg(uint32_t BackupRegister);
float adc_read_avg(AnalogIn& input);

//----------------------------------------------------------------------------
int main()
{
    Ticker tick1, tick2;

    setup();
    wait(3);    // safety feature: be able to re-flash before starting

    time_t last_second = time(NULL);

    tick1.attach(&update_measurements, 0.01);
    tick2.attach(&state_machine, 0.1);

    enter_state(STANDBY);
    time_state_changed = time(NULL) - 60*60*24; // start immediately
    //enter_state(TEST);

    while(1) {
        // called once per second
        if (time(NULL) - last_second >= 1) {
            last_second = time(NULL);
            energy_counter();
            update_screen();
            log_data();
        }
        sleep();    // wake-up by ticker interrupts
    }
}

//----------------------------------------------------------------------------
void setup()
{
    #ifdef DOGLCD_ENABLED
        lcd.init();
        lcd.view(VIEW_TOP);
    #endif

    led_green = 1;
    load_disable = 0;
    //    ref_i_dcdc = 0;         // 0 for buck, 1 for boost (TODO)

    // for RTC backup register
    __PWR_CLK_ENABLE();   // normally in SystemClock_Config()
    HAL_PWR_EnableBkUpAccess();

    set_time(rtc_read_backup_reg(0));

    input_Wh_day = float(rtc_read_backup_reg(1)) / 1000.0; // mWh
    output_Wh_day = float(rtc_read_backup_reg(2)) / 1000.0; // mWh
    input_Wh_total = float(rtc_read_backup_reg(3)); // Wh
    output_Wh_total = float(rtc_read_backup_reg(4)); // Wh

    mppt.frequency_kHz(_pwm_frequency);
    mppt.deadtime_ns(300);
    mppt.set_duty_cycle(0.90);
    mppt.lock_settings();
    mppt.set_current_limit(charge_current_max);
    mppt.set_voltage_limit(num_cells * cell_voltage_max);
    mppt.duty_cycle_limits((float) cell_voltage_load_disconnect * num_cells / max_solar_voltage, 0.97);

    update_measurements();

    // calibration of current sensors in no load condition
    load_current_offset = -load_current;
    dcdc_current_offset = -dcdc_current;

    serial.baud(115200);
    serial.printf("\nSerial interface started...\n");

#ifdef OLED_ENABLED
    i2c.frequency(400000);
#endif

    freopen("/serial", "w", stdout);  // retarget stdout
}

void load_management() {

    if (battery_voltage < cell_voltage_load_disconnect * num_cells) {
        load_disable = 1;
    }
    if (battery_voltage >= cell_voltage_load_reconnect * num_cells) {
        load_disable = 0;
    }
}

// actions to be performed upon entering a state
void enter_state(int next_state)
{
    switch (next_state)
    {
        case STANDBY:
            serial.printf("Enter State: STANDBY\n");
            mppt.stop();
            led_red = 0;
            break;

        case CHG_CC:
            serial.printf("Enter State: CHG_CC\n");
            //serial.printf("P=%d Vsol=%d Vbat=%d Ibat=%d delta_time_CV=%d\n", solar_power, solar_voltage, battery_voltage, dcdc_current, time(NULL) - mppt.last_time_CV());
            mppt.set_current_limit(charge_current_max);
            mppt.set_voltage_limit(num_cells * cell_voltage_max);
            mppt.last_time_CV_reset();
            mppt.start((float) battery_voltage / (solar_voltage - 1000.0));
            led_red = 1;
            break;

        case CHG_CV:
            serial.printf("Enter State: CHG_CV\n");
            //serial.printf("P=%d Vsol=%d Vbat=%d Ibat=%d delta_time_CV=%d\n", solar_power, solar_voltage, battery_voltage, dcdc_current, time(NULL) - mppt.last_time_CV());
            mppt.set_voltage_limit(num_cells * cell_voltage_max);
            break;

        case CHG_TRICKLE:
            serial.printf("Enter State: CHG_TRICKLE\n");
            //serial.printf("P=%d Vsol=%d Vbat=%d Ibat=%d delta_time_CV=%d\n", solar_power, solar_voltage, battery_voltage, dcdc_current, time(NULL) - mppt.last_time_CV());
            mppt.set_voltage_limit(num_cells * cell_voltage_trickle);
            mppt.last_time_CV_reset();
            break;

        case CHG_EQUALIZATION:
            serial.printf("Enter State: CHG_EQUALIZATION\n");
            mppt.set_voltage_limit(num_cells * cell_voltage_equalization);
            mppt.last_time_CV_reset();
            mppt.set_current_limit(current_limit_equalization);
            break;

        case TEST:
            serial.printf("Enter State: TEST\n");
            mppt.start(0.50);
            led_red = 1;
            break;

        case BAT_ERROR:
            serial.printf("Enter State: BAT_ERROR\n");
            mppt.stop();
            led_red = 0;
            break;

        default:
            mppt.stop();
            led_red = 0;
            break;
    }
    time_state_changed = time(NULL);
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
 *
 */
void state_machine()
{
    mppt.update(solar_voltage, battery_voltage, dcdc_current);
    load_management();

    switch (state) {

        case STANDBY: {
            if  (solar_voltage > battery_voltage + solar_voltage_offset_start
                 && battery_voltage < num_cells * cell_voltage_recharge
                 && (time(NULL) - time_state_changed) > time_limit_recharge)
            {
                enter_state(CHG_CC);
            }
            break;
        }

        case CHG_CC: {
            if (mppt.last_time_CV() <= time(NULL)) {
                serial.printf("Last_time_cv = %d, time(NULL) = %d\n", mppt.last_time_CV(), time(NULL));
                enter_state(CHG_CV);
            }
            else if (dcdc_current < charge_current_cutoff
                     || solar_voltage < battery_voltage + solar_voltage_offset_stop) {
                enter_state(STANDBY);
            }
            break;
        }

        case CHG_CV: {
            // cut-off limit reached because battery full (i.e. CV mode still
            // reached by available solar power within last 2s) or CV period long enough?
            if ((dcdc_current < current_cutoff_CV && (time(NULL) - mppt.last_time_CV()) < 2)
                || (time(NULL) - time_state_changed) > time_limit_CV * 60)
            {
                if (equalization_enabled) {
                    // TODO: additional conditions!
                    enter_state(CHG_EQUALIZATION);
                }
                else if (trickle_enabled) {
                    enter_state(CHG_TRICKLE);
                }
                else {
                    enter_state(STANDBY);
                }
            }
            else if (dcdc_current < charge_current_cutoff
                     || solar_voltage < battery_voltage + solar_voltage_offset_stop) {
                enter_state(STANDBY);
            }
            break;
        }

        case CHG_TRICKLE: {
            if (time(NULL) - mppt.last_time_CV() > time_trickle_recharge * 60) {
                enter_state(CHG_CC);
            }
            else if (dcdc_current < charge_current_cutoff
                     || solar_voltage < battery_voltage + solar_voltage_offset_stop) {
                // current cutoff might never be reached because of lakage
                // current inside battery --> stay in trickle till end of day
                enter_state(STANDBY);
            }
            break;
        }

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
        seconds_day = 0;
        day_counter++;
    }
    else {
        seconds_day++;
    }

    input_Wh_day += solar_power / 1000.0 / 3600.0;
    output_Wh_day += load_current / 1000.0 * battery_voltage / 1000.0 / 3600.0;
    input_Wh_total += solar_power / 1000.0 / 3600.0;
    output_Wh_total += load_current / 1000.0 * battery_voltage / 1000.0 / 3600.0;

    // save values to RTC backup register
    rtc_write_backup_reg(0, time(NULL));
    rtc_write_backup_reg(1, (uint32_t)(input_Wh_day * 1000));  // mWh
    rtc_write_backup_reg(2, (uint32_t)(output_Wh_day * 1000));  // mWh
    rtc_write_backup_reg(3, (uint32_t)(input_Wh_total));  // kWh
    rtc_write_backup_reg(4, (uint32_t)(output_Wh_total));  // kWh
}

void log_data()
{
    //FILE *fp = fopen("/sd/solar_log.csv", "a");
    FILE *fp = fopen("/serial_uext", "a");

    if(fp == NULL) {
        serial.printf("Could not open file for write\n");
        fclose(fp);
        return;
    }

    fprintf(fp, "%d;", (int)time(NULL));
    fprintf(fp, "%d;", day_counter);
    fprintf(fp, "%.2f;%.1f;", solar_voltage / 1000.0, solar_power / 1000.0);
    fprintf(fp, "%.2f;%.1f;", battery_voltage / 1000.0, dcdc_current / 1000.0);
    fprintf(fp, "%.1f;%.2f;", (load_current < 0 ? 0 : load_current) / 1000.0 * battery_voltage / 1000.0,
        (load_current < 0 ? 0 : load_current) / 1000.0);
    fprintf(fp, "%d;", !load_disable);
    fprintf(fp, "%.0f;%.0f;", input_Wh_day, fabs(output_Wh_day));
    fprintf(fp, "%.1f;%.1f;", input_Wh_total / 1000.0, fabs(output_Wh_total / 1000.0));
    switch (state) {
        case STANDBY:
            fprintf(fp, "Standby;");
            break;
        case CHG_CC:
            fprintf(fp, "CC charge;");
            break;
        case CHG_CV:
            fprintf(fp, "CV charge;");
            break;
        case CHG_TRICKLE:
            fprintf(fp, "Trickle chg;");
            break;
        case CHG_BIKE:
            fprintf(fp, "Bike gen;");
            break;
        default:
            fprintf(fp, "Error;");
            break;
    }
    fprintf(fp, "\n");

    fclose(fp);
}

float adc_read_avg (AnalogIn& input) {
    // use read_u16 for faster calculation
    unsigned int sum_adc_readings = 0;
    for (int i = 0; i < ADC_AVG_SAMPLES; i++) {
        sum_adc_readings += input.read_u16(); // normalized to 0xFFFF
    }
    return (float) (sum_adc_readings / ADC_AVG_SAMPLES) / (float)0xFFFF;
}

//----------------------------------------------------------------------------
void update_measurements()
{
    int vcc = 3300;
    float tmp = 0.0;

    // low pass filter of measurement values:
    // y(n) = (1-c)*x(n) + c*y(n-1)    with c=exp(-Ta/RC)
    // https://en.wikipedia.org/wiki/Low-pass_filter#Simple_infinite_impulse_response_filter
    float c = 0.2;      // filter constant

    // first call of function
    if (battery_voltage == 0) {
        c = 0.0;
    }

#ifdef MPPT_12A
    // solar voltage divider o-- 150k --o-- 6.8k --o
    tmp = adc_read(v_solar) * vcc * 1568 / 68;
    solar_voltage = (100 - c) * tmp + c * solar_voltage;

    // battery voltage divider o-- 120k --o-- 10k --o
    tmp = adc_read(v_bat) * vcc * 130 / 10;
    battery_voltage = (100 - c) * tmp + c * battery_voltage;

    tmp = adc_read(i_dcdc) * vcc  * 1000 / 5 / 50;
    dcdc_current = (1.0 - c) * (tmp + dcdc_current_offset) + c * dcdc_current;

    tmp = adc_read(i_load) * vcc * 1000 / 5 / 50;
    load_current = (1.0 - c) * (tmp + load_current_offset) + c * load_current;

    solar_power = battery_voltage * dcdc_current / 1000;
#endif

#ifdef MPPT_20A
    // reference voltage (should be 2.5V)
    vcc = 2500 / adc_read_avg(v_ref);

    // solar voltage divider o-- 100k --o-- 5.6k --o
    tmp = adc_read_avg(v_solar) * vcc * 1056 / 56;
    solar_voltage = (1.0 - c) * tmp + c * solar_voltage;

    // battery voltage divider o-- 100k --o-- 10k --o
    tmp = adc_read_avg(v_bat) * vcc * 110 / 10;
    battery_voltage = (1.0 - c) * tmp + c * battery_voltage;

    // dcdc current (op amp gain: 150/2.2 = 68.2, resistor: 2 mOhm)
    tmp = adc_read_avg(i_dcdc) * vcc * 1000 / 2 / (1500 / 22);
    dcdc_current = (1.0 - c) * (tmp + dcdc_current_offset) + c * dcdc_current;

    // load current (op amp gain: 150/2.2 = 68.2, resistor: 2 mOhm)
    tmp = adc_read_avg(i_load) * vcc * 1000 / 2 / (1500 / 22);
    load_current = (1.0 - c) * (tmp + load_current_offset) + c * load_current;

    solar_power = battery_voltage * dcdc_current / 1000;

    // temperature sensed by NTC
    tmp = adc_read_avg(v_temp) * vcc;  // voltage read by ADC
    unsigned long rts = 10000.0 * tmp / (3300.0 - tmp); // Ohm
    // Temperature calculation using Beta equation for 10k thermistor
    // (25°C reference temperature for Beta equation assumed)
    float temp_K = 1.0/(1.0/(273.15+25) + 1.0/thermistorBetaValue*log(rts/10000.0)); // K
    temperature = (1.0 - c) * (temp_K - 273.15) + c * temperature;
#endif

}

uint32_t rtc_read_backup_reg(uint32_t BackupRegister) {
    RTC_HandleTypeDef RtcHandle;
    RtcHandle.Instance = RTC;
    return HAL_RTCEx_BKUPRead(&RtcHandle, BackupRegister);
}

void rtc_write_backup_reg(uint32_t BackupRegister, uint32_t data) {
    RTC_HandleTypeDef RtcHandle;
    RtcHandle.Instance = RTC;
    HAL_PWR_EnableBkUpAccess();
    HAL_RTCEx_BKUPWrite(&RtcHandle, BackupRegister, data);
    HAL_PWR_DisableBkUpAccess();
}


#ifdef OLED_ENABLED
void update_screen(void)
{
    oled.clearDisplay();
    oled.drawBitmap(6, 0, bmp_pv_panel, 16, 16, 1);
    oled.drawBitmap(104, 0, bmp_load, 16, 16, 1);

    if (state == STANDBY) {
        oled.drawBitmap(27, 3, bmp_disconnected, 32, 8, 1);
    }
    else {
        oled.drawBitmap(34, 3, bmp_arrow_right, 5, 7, 1);
    }

    if (load_disable == 1) {
        oled.drawBitmap(81, 3, bmp_disconnected, 17, 7, 1);
    }
    else {
        oled.drawBitmap(84, 3, bmp_arrow_right, 5, 7, 1);
    }

    oled.drawRect(52, 2, 18, 9, 1);     // battery shape
    oled.drawRect(69, 3, 3, 7, 1);      // battery terminal
    oled.drawRect(54, 4, 2, 5, 1);      // bar 1
    oled.drawRect(57, 4, 2, 5, 1);      // bar 2
    oled.drawRect(60, 4, 2, 5, 1);      // bar 3
    oled.drawRect(63, 4, 2, 5, 1);      // bar 4
    oled.drawRect(66, 4, 2, 5, 1);      // bar 5

    oled.setTextCursor(0, 18);
    oled.printf("%4.1fV", solar_voltage / 1000.0);
    oled.setTextCursor(0, 26);
    oled.printf("%4.0fW", abs(solar_power) / 1000.0);

    oled.setTextCursor(42, 18);
    oled.printf("%5.2fV", battery_voltage / 1000.0);
    oled.setTextCursor(42, 26);
    oled.printf("%5.2fA", abs(dcdc_current) / 1000.0);

    oled.setTextCursor(90, 18);
    oled.printf("%5.2fA\n", abs(load_current) / 1000.0);
    oled.setTextCursor(90, 26);
    oled.printf("%5.1fW", abs(load_current) / 1000.0 * battery_voltage / 1000.0);

    oled.setTextCursor(0, 36);
    oled.printf("Day +%5.0fWh -%5.0fWh", input_Wh_day, fabs(output_Wh_day));
    oled.printf("Tot +%4.1fkWh -%4.1fkWh", input_Wh_total / 1000.0, fabs(output_Wh_total) / 1000.0);

    oled.setTextCursor(0, 56);
    oled.printf("PWM %.1f%% ", mppt.get_duty_cycle() * 100.0);
    switch (state) {
        case STANDBY:
            oled.printf("Standby\n");
            break;
        case CHG_CC:
            oled.printf("CC charge\n");
            break;
        case CHG_CV:
            oled.printf("CV charge\n");
            break;
        case CHG_TRICKLE:
            oled.printf("Trickle chg\n");
            break;
        case CHG_BIKE:
            oled.printf("Bike gen\n");
            break;
        default:
            oled.printf("Error\n");
            break;
    }
    oled.display();
}
#endif // OLED
