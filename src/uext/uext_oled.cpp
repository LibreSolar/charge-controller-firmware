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

#ifdef OLED_ENABLED     // otherwise don't compile code to reduce firmware size

#include "uext.h"
#include "pcb.h"

#include "half_bridge.h"
#include "dcdc.h"
#include "power_port.h"
#include "battery.h"
#include "load.h"
#include "log.h"

extern log_data_t log_data;
extern dcdc_t dcdc;
extern power_port_t hs_port;
extern power_port_t ls_port;
extern battery_state_t bat_state;
extern load_output_t load;

#include "Adafruit_SSD1306.h"

const unsigned char bmp_load [] = {
    0x20, 0x22, 0x04, 0x70, 0x88, 0x8B, 0x88, 0x70, 0x04, 0x22, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x07, 0x04, 0x07, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char bmp_arrow_right [] = {
    0x41, 0x63, 0x36, 0x1C
};

const unsigned char bmp_pv_panel [] = {
    0x60, 0x98, 0x86, 0xC9, 0x31, 0x19, 0x96, 0x62, 0x32, 0x2C, 0xC4, 0x64, 0x98, 0x08, 0xC8, 0x30,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x02, 0x02, 0x03, 0x04, 0x04, 0x04, 0x03, 0x00, 0x00
};

const unsigned char bmp_disconnected [] = {
    0x08, 0x08, 0x08, 0x08, 0x00, 0x41, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x41, 0x00, 0x08, 0x08,
    0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

I2C i2c(PIN_UEXT_SDA, PIN_UEXT_SCL);
Adafruit_SSD1306_I2c oled(i2c, PIN_UEXT_SSEL, 0x78, 64, 128);

void uext_init() {;}    // no need for initialization
void uext_process_asap() {;}

void uext_process_1s()
{
    float tmp;

    oled.clearDisplay();

    oled.drawBitmap(6, 0, bmp_pv_panel, 16, 16, 1);
    oled.drawBitmap(104, 0, bmp_load, 16, 16, 1);

    if (half_bridge_enabled() == false) {
        oled.drawBitmap(27, 3, bmp_disconnected, 32, 8, 1);
    }
    else {
        oled.drawBitmap(34, 3, bmp_arrow_right, 5, 7, 1);
    }

    if (load.enabled) {
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

    // solar panel data
    if (half_bridge_enabled()) {
        tmp = -hs_port.voltage * hs_port.current;
        oled.setTextCursor(0, 18);
        oled.printf("%4.0fW", (abs(tmp) < 1) ? 0 : tmp);     // remove negative zeros
    }
    else {
        oled.setTextCursor(8, 18);
        oled.printf("n/a");
    }
    //if (solar_port->voltage > bat_port->voltage) {
        oled.setTextCursor(0, 26);
        oled.printf("%4.1fV", hs_port.voltage);
    //}

    // battery data
    tmp = ls_port.voltage * ls_port.current;
    oled.setTextCursor(42, 18);
    oled.printf("%5.1fW", (abs(tmp) < 0.1) ? 0 : tmp);    // remove negative zeros
    oled.setTextCursor(42, 26);
    oled.printf("%5.1fV", ls_port.voltage);

    // load data
    tmp = ls_port.voltage * load.current;
    oled.setTextCursor(90, 18);
    oled.printf("%5.1fW", (abs(tmp) < 0.1) ? 0 : tmp);    // remove negative zeros
    oled.setTextCursor(90, 26);
    oled.printf("%5.1fA\n", (abs(load.current) < 0.1) ? 0 : load.current);

    oled.setTextCursor(0, 36);
    oled.printf("Day +%5.0fWh -%5.0fWh", log_data.solar_in_day_Wh, fabs(log_data.load_out_day_Wh));
    oled.printf("Tot +%4.1fkWh -%4.1fkWh", log_data.solar_in_total_Wh / 1000.0, fabs(log_data.load_out_total_Wh) / 1000.0);

    oled.setTextCursor(0, 56);
    oled.printf("T %.0fC PWM %.0f%% SOC %d%%", dcdc.temp_mosfets, half_bridge_get_duty_cycle() * 100.0, bat_state.soc);

    oled.display();
}

#endif /* OLED_ENABLED */