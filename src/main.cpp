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
#include "HalfBridge.h"
#include "AnalogValue.h"
#include "ChargeController.h"
#include "Adafruit_SSD1306.h"
//#include "SDFileSystem.h"
#include <math.h>     // log for thermistor calculation
#include "display.h"

//#define SDCARD_ENABLED
#define OLED_ENABLED

//----------------------------------------------------------------------------
// global variables

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
HalfBridge dcdc(_pwm_frequency);
float dcdc_power;    // saves previous output power
int pwm_delta = 1;

DigitalOut led_green(PIN_LED_GREEN);
DigitalOut led_red(PIN_LED_RED);
DigitalOut load_disable(PIN_LOAD_DIS);

#ifdef MPPT_20A
AnalogOut ref_i_dcdc(PIN_REF_I_DCDC);
#endif

AnalogIn v_ref(PIN_V_REF);

#ifdef MPPT_20A
AnalogValue solar_voltage(PIN_V_SOLAR, 1056.0 / 56.0); // solar voltage divider o-- 100k --o-- 5.6k --o
AnalogValue battery_voltage(PIN_V_BAT, 110 / 10); // battery voltage divider o-- 100k --o-- 10k --o
AnalogValue dcdc_current(PIN_I_DCDC, 1000 / 2 / (1500.0 / 22.0)); // op amp gain: 150/2.2 = 68.2, resistor: 2 mOhm
AnalogValue load_current(PIN_I_LOAD, 1000 / 2 / (1500.0 / 22.0));
AnalogValue thermistor_voltage(PIN_TEMP_INT, 1.0);
#endif
#ifdef MPPT_12A
AnalogValue solar_voltage(PIN_V_SOLAR, 156.8 / 6.8); // solar voltage divider o-- 100k --o-- 5.6k --o
AnalogValue battery_voltage(PIN_V_BAT, 130 / 10); // battery voltage divider o-- 100k --o-- 10k --o
AnalogValue dcdc_current(PIN_I_DCDC, 1000 / 5 / 50); // op amp gain: 150/2.2 = 68.2, resistor: 2 mOhm
AnalogValue load_current(PIN_I_LOAD, 1000 / 5 / 50);
AnalogValue thermistor_voltage(PIN_TEMP_INT, 1.0);
#endif

ChargeController charger(profile);

float temperature; // °C

// for daily energy counting
int seconds_zero_solar = 0;
int seconds_day = 0;
int day_counter = 0;
bool nighttime = false;
float input_Wh_day = 0.0;
float output_Wh_day = 0.0;
float input_Wh_total = 0.0;
float output_Wh_total = 0.0;

//----------------------------------------------------------------------------
// function prototypes

void setup();
void update_measurements();
void update_screen();
void energy_counter();
void log_data();
void rtc_write_backup_reg(uint32_t BackupRegister, uint32_t data);
uint32_t rtc_read_backup_reg(uint32_t BackupRegister);
void update_mppt();

//----------------------------------------------------------------------------
int main()
{
    Ticker tick1, tick2;

    setup();
    wait(3);    // safety feature: be able to re-flash before starting

    time_t last_second = time(NULL);

    tick1.attach(&update_measurements, 0.01);
    tick2.attach(&update_mppt, 0.05);
    //dcdc.start(0.5);  // for testing only (don't run update_mppt!)

    //time_state_changed = time(NULL) - 60*60*24; // start immediately

    while(1) {
        // called once per second
        if (time(NULL) - last_second >= 1) {
            last_second = time(NULL);

            // updates charger state machine
            charger.update(battery_voltage.read(), dcdc_current.read());

            // load management
            if (charger.discharging_enabled()) {
                load_disable = 0;
            } else {
                load_disable = 1;
            }

            energy_counter();
            update_screen();
            log_data();
            fflush(stdout);
        }
        sleep();    // wake-up by ticker interrupts
    }
}

//----------------------------------------------------------------------------
void setup()
{
    led_green = 1;
    load_disable = 1;

#ifdef MPPT_20A
    ref_i_dcdc = 0.1;         // 0 for buck, 1 for boost (TODO)
#endif

    // for RTC backup register
    __PWR_CLK_ENABLE();   // normally in SystemClock_Config()
    HAL_PWR_EnableBkUpAccess();

    set_time(rtc_read_backup_reg(0));

    input_Wh_day = rtc_read_backup_reg(1);
    output_Wh_day = rtc_read_backup_reg(2);
    input_Wh_total = rtc_read_backup_reg(3);
    output_Wh_total = rtc_read_backup_reg(4);

    dcdc.deadtime_ns(300);
    dcdc.lock_settings();
    dcdc.duty_cycle_limits((float) profile.cell_voltage_load_disconnect * profile.num_cells / solar_voltage_max, 0.97);

    wait(0.2);
    update_measurements();

    dcdc_current.calibrate_offset(0.0);
    load_current.calibrate_offset(0.0);

    serial.baud(115200);
    serial.printf("\nSerial interface started...\n");

#ifdef OLED_ENABLED
    i2c.frequency(400000);
#endif

    freopen("/serial", "w", stdout);  // retarget stdout
}

void update_mppt()
{
    //serial.printf("B: %.2fV, %.2fA, S: %.2fV, Charger: %d, DCDC: %d, PWM: %.1f\n",
    //    battery_voltage.read(), dcdc_current.read(), solar_voltage.read(),
    //    charger.get_state(), dcdc.enabled(), dcdc.get_duty_cycle() * 100.0);

    float dcdc_power_new = battery_voltage * dcdc_current;

    if (dcdc.enabled() == false && charger.charging_enabled() == true
        && battery_voltage < charger.read_target_voltage()
        && solar_voltage > battery_voltage + solar_voltage_offset_start)
    {
        dcdc.start(battery_voltage / (solar_voltage - 1.0));
        led_red = 1;
    }
    else if (solar_voltage < battery_voltage + solar_voltage_offset_stop
        || dcdc_current < dcdc_current_min)
    {
        dcdc.stop();
        led_red = 0;
        //serial.printf("DCDC stopped!\n");
    }

    if (battery_voltage > charger.read_target_voltage()) {
        // increase input voltage to lower output voltage
        if (dcdc_current > dcdc_current_min) {
            dcdc.duty_cycle_step(-1);
        }
        else {
            // switch off and wait for voltage to go down
            // (otherwise current could get negative, i.e. into solar panel)
            dcdc.stop();
            led_red = 0;
            //serial.printf("DCDC stopped!\n");
        }
    }
    else if (dcdc_current > charger.read_target_current()) {
        // increase input voltage to decrease current
        dcdc.duty_cycle_step(-1);
    }
    else if (dcdc.enabled()) {
        //printf("MPPT\n");
        // start MPPT
        if (dcdc_power > dcdc_power_new) {
            pwm_delta = -pwm_delta;
        }
        dcdc.duty_cycle_step(pwm_delta);
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
        input_Wh_day = 0.0;
        output_Wh_day = 0.0;
        seconds_day = 0;
        day_counter++;
    }
    else {
        seconds_day++;
    }

    input_Wh_day += dcdc_power / 3600.0;
    output_Wh_day += load_current * battery_voltage / 3600.0;
    input_Wh_total += dcdc_power / 3600.0;
    output_Wh_total += load_current * battery_voltage/ 3600.0;

    // save values to RTC backup register
    rtc_write_backup_reg(0, time(NULL));
    rtc_write_backup_reg(1, input_Wh_day);
    rtc_write_backup_reg(2, output_Wh_day);
    rtc_write_backup_reg(3, input_Wh_total);
    rtc_write_backup_reg(4, output_Wh_total);
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
    fprintf(fp, "%.2f;%.1f;", solar_voltage.read(), dcdc_power);
    fprintf(fp, "%.2f;%.1f;", battery_voltage.read(), dcdc_current.read());
    fprintf(fp, "%.1f;%.2f;", (load_current < 0 ? 0 : load_current.read()) * battery_voltage.read(),
        (load_current < 0 ? 0 : load_current.read()));
    fprintf(fp, "%d;", !load_disable);
    fprintf(fp, "%.0f;%.0f;", input_Wh_day, fabs(output_Wh_day));
    fprintf(fp, "%.1f;%.1f;", input_Wh_total / 1000.0, fabs(output_Wh_total / 1000.0));
    switch (charger.get_state()) {
        case CHG_IDLE:
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
        default:
            fprintf(fp, "Error;");
            break;
    }
    fprintf(fp, "\n");

    fclose(fp);
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

#ifdef MPPT_20A
    thermistor_voltage.update();

    // resistance of NTC (Ohm)
    unsigned long rts = 10000.0 * thermistor_voltage.read() / (3.3 - thermistor_voltage.read());
    // Temperature calculation using Beta equation for 10k thermistor
    // (25°C reference temperature for Beta equation assumed)
    temperature = 1.0/(1.0/(273.15+25) + 1.0/thermistorBetaValue*log(rts/10000.0)) - 273.15;
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

    if (dcdc.enabled() == false) {
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
    oled.printf("%4.1fV", solar_voltage.read());
    oled.setTextCursor(0, 26);
    oled.printf("%4.0fW", fabs(dcdc_power));

    oled.setTextCursor(42, 18);
    oled.printf("%5.2fV", battery_voltage.read());
    oled.setTextCursor(42, 26);
    oled.printf("%5.2fA", fabs(dcdc_current));

    oled.setTextCursor(90, 18);
    oled.printf("%5.2fA\n", fabs(load_current));
    oled.setTextCursor(90, 26);
    oled.printf("%5.1fW", fabs(load_current) * battery_voltage);

    oled.setTextCursor(0, 36);
    oled.printf("Day +%5.0fWh -%5.0fWh", input_Wh_day, fabs(output_Wh_day));
    oled.printf("Tot +%4.1fkWh -%4.1fkWh", input_Wh_total / 1000.0, fabs(output_Wh_total) / 1000.0);

    oled.setTextCursor(0, 56);
    oled.printf("T %.1f PWM %.1f%% ", temperature, dcdc.get_duty_cycle() * 100.0);
    switch (charger.get_state()) {
        case CHG_IDLE:
            oled.printf("Idle\n");
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
        default:
            oled.printf("Error\n");
            break;
    }
    oled.display();
}
#endif // OLED
