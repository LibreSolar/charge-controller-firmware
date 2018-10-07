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

#include "config.h"
#include "data_objects.h"
#include "dcdc.h"
#include "charger.h"

#include "Adafruit_SSD1306.h"
#include "display.h"

Serial serial_uext(PIN_UEXT_TX, PIN_UEXT_RX, "serial_uext");

extern I2C i2c;

Adafruit_SSD1306_I2c oled(i2c, PIN_UEXT_SSEL, 0x78, 64, 128);

//extern DeviceState device;

extern Serial serial;

//SDFileSystem sd(PIN_UEXT_MOSI, PIN_UEXT_MISO, PIN_UEXT_SCK, PIN_UEXT_SSEL, "sd");

void output_serial()
{
    #ifdef SDCARD_ENABLED
        //sd.mount();
        FILE *fp = fopen("/sd/solar_log.csv", "a");
    #else
        //FILE *fp = fopen("/serial_uext", "a");
        FILE *fp = fopen("/serial", "a");
    #endif

    if(fp == NULL) {
        printf("Could not open file for write\n");
    }
    else {
        char buffer[32];
        time_t seconds = time(NULL)+60*60*2;
        strftime(buffer, 32, "%F %T", localtime(&seconds));
        //printf("Time as a custom formatted string = %s", buffer);
        fprintf(fp, "%s;", buffer);
//        fprintf(fp, "%d;", seconds_day);
//        fprintf(fp, "%d;", day_counter);
        fprintf(fp, "%.2f;%.1f;", device.input_voltage, device.bus_voltage * device.bus_current);
        fprintf(fp, "%.2f;%.1f;", device.bus_voltage, device.bus_current);
        fprintf(fp, "%.1f;%.2f;", device.load_current * device.bus_voltage, device.load_current);
        fprintf(fp, "%d;", device.load_enabled);
        fprintf(fp, "%.0f;%.0f;", device.input_Wh_day, fabs(device.output_Wh_day));
        fprintf(fp, "%.1f;%.1f;", device.input_Wh_total / 1000.0, fabs(device.output_Wh_total / 1000.0));
        switch (charger_get_state()) {
            case CHG_IDLE:
                fprintf(fp, "Standby;");
                break;
            case CHG_CC:
                fprintf(fp, "CC;");
                break;
            case CHG_CV:
                fprintf(fp, "CV;");
                break;
            case CHG_TRICKLE:
                fprintf(fp, "Trickle;");
                break;
            default:
                fprintf(fp, "Error;");
                break;
        }
        fprintf(fp, "%.1f;", dcdc_get_duty_cycle() * 100.0);
        fprintf(fp, "%d;", dcdc_enabled());
        fprintf(fp, "\n");
    }

    fclose(fp);

    #ifdef SDCARD_ENABLED
    sd.unmount();
    #endif
}

void output_serial_json()
{
    serial.printf("{");
    bool first = true;
    for(unsigned int i = 0; i < dataObjectsCount; ++i) {
        if (first == false) {
            serial.printf(",");
        }
        else {
            first = false;
        }
        serial.printf("\"%s\":", dataObjects[i].name);
        switch (dataObjects[i].type) {
            case T_FLOAT32:
                serial.printf("%.3f", *((float*)dataObjects[i].data));
                break;
            case T_STRING:
                serial.printf("\"%s\"", (char*)dataObjects[i].data);
                break;
            case T_INT32:
                serial.printf("%d", *((int*)dataObjects[i].data));
                break;
            case T_BOOL:
                serial.printf("%s", (*((bool*)dataObjects[i].data) == true ? "true" : "false"));
                break;
        }
    }
    serial.printf("}\n");
}

void output_oled()
{
    oled.clearDisplay();

    oled.drawBitmap(6, 0, bmp_pv_panel, 16, 16, 1);
    oled.drawBitmap(104, 0, bmp_load, 16, 16, 1);

    if (dcdc_enabled() == false) {
        oled.drawBitmap(27, 3, bmp_disconnected, 32, 8, 1);
    }
    else {
        oled.drawBitmap(34, 3, bmp_arrow_right, 5, 7, 1);
    }

    if (device.load_enabled) {
        oled.drawBitmap(84, 3, bmp_arrow_right, 5, 7, 1);
    }
    else {
        oled.drawBitmap(81, 3, bmp_disconnected, 17, 7, 1);
    }

    oled.drawRect(52, 2, 18, 9, 1);     // battery shape
    oled.drawRect(69, 3, 3, 7, 1);      // battery terminal
    oled.drawRect(54, 4, 2, 5, 1);      // bar 1
    oled.drawRect(57, 4, 2, 5, 1);      // bar 2
    oled.drawRect(60, 4, 2, 5, 1);      // bar 3
    oled.drawRect(63, 4, 2, 5, 1);      // bar 4
    oled.drawRect(66, 4, 2, 5, 1);      // bar 5

    oled.setTextCursor(0, 18);
    oled.printf("%4.1fV", device.input_voltage);
    oled.setTextCursor(0, 26);
    oled.printf("%4.0fW", fabs(device.bus_voltage * device.bus_current));

    oled.setTextCursor(42, 18);
    oled.printf("%5.2fV", device.bus_voltage);
    oled.setTextCursor(42, 26);
    oled.printf("%5.2fA", fabs(device.bus_current));

    oled.setTextCursor(90, 18);
    oled.printf("%5.2fA\n", fabs(device.load_current));
    oled.setTextCursor(90, 26);
    oled.printf("%5.1fW", fabs(device.load_current) * device.bus_voltage);

    oled.setTextCursor(0, 36);
    oled.printf("Day +%5.0fWh -%5.0fWh", device.input_Wh_day, fabs(device.output_Wh_day));
    oled.printf("Tot +%4.1fkWh -%4.1fkWh", device.input_Wh_total / 1000.0, fabs(device.output_Wh_total) / 1000.0);

    oled.setTextCursor(0, 56);
    oled.printf("T %.1f PWM %.1f%% ", device.internal_temperature, dcdc_get_duty_cycle() * 100.0);
    switch (charger_get_state()) {
        case CHG_IDLE:
            oled.printf("Idle");
            break;
        case CHG_CC:
            oled.printf("CC");
            break;
        case CHG_CV:
            oled.printf("CV");
            break;
        case CHG_TRICKLE:
            oled.printf("Trkl");
            break;
        default:
            oled.printf("Err.");
            break;
    }
    oled.display();
}
